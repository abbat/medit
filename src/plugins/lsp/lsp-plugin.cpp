/*
 *   plugins/lsp/lsp-plugin.cpp
 *
 *   Copyright (C) 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A client for the language server protocol, built around one server process
 * per project root as described in lsp.xml. The whole plugin is compiled out
 * when cmake does not find json-glib.
 *
 * Nothing here depends on the gtk version: unlike the terminal, this builds
 * for both.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugins/lsp/lsp-plugin.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-diagnostics.h"
#include "plugins/lsp/lsp-symbols.h"

#include "mooedit/mooplugin-macro.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"
#include "mooedit/mootextview.h"
#include "plugins/support/moolineview.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moopane.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooutils-misc.h"

typedef struct {
    MooPlugin parent;
    guint     ui_merge_id;
} LspPlugin;

typedef struct {
    MooDocPlugin parent;
} LspDocPlugin;

typedef struct {
    MooWinPlugin   parent;

    MooEditWindow *window;

    MooLineView   *output;
    MooPane       *pane;
    GtkTextTag    *location_tag;
    GtkTextTag    *severity_tag[4];
    GtkTextTag    *detail_tag;
    guint          update_idle;

    GtkTreeView   *symbols;
    GtkTreeStore  *symbol_store;
    MooPane       *symbols_pane;
    MooEdit       *symbols_doc;         /* what the pending request is about */
    GtkTextBuffer *symbols_buffer;      /* connected to ::changed */
    gint64         symbols_request;
    guint          symbols_timeout;
} LspWindowPlugin;

#define MOO_LSP_SYMBOLS_PANE_ID "LspSymbols"

/*
 * Every live window plugin. A reply from a server can arrive after its window
 * is gone, so a callback checks that its own is still in here first.
 */
static GSList *lsp_windows;

static void     watch_active_buffer     (LspWindowPlugin *stuff);
static void     queue_symbols_update    (LspWindowPlugin *stuff);
static void     clear_symbols_doc       (LspWindowPlugin *stuff);
static GtkWidget *create_symbols_pane   (LspWindowPlugin *stuff);

/* Where a line of the pane points, in the document's own coordinates. */
typedef struct {
    int line;
    int character;
} LspPaneLocation;

MOO_PLUGIN_DEFINE_INFO (lsp,
                        N_("LSP"), N_("Language server protocol client"),
                        "Anton Batenev <antonbatenev@yandex.ru>",
                        MOO_VERSION)

MOO_DOC_PLUGIN_DEFINE (Lsp, lsp)
MOO_WIN_PLUGIN_DEFINE (Lsp, lsp)


gboolean
_moo_lsp_debug (void)
{
    return moo_getenv_bool ("MEDIT_LSP_DEBUG") ||
           moo_prefs_get_bool (MOO_LSP_PREFS_DEBUG);
}


/**********************************************************************/
/* The document
 */

/*
 * Saving an untitled document, and choosing another language by hand, both
 * change which server applies -- or whether one applies at all -- so the
 * document is detached and attached again.
 *
 * Both signals also fire while a document is being opened, when nothing has
 * really changed yet, so an attached document that still matches is left
 * alone; otherwise every file opened would be announced to the server twice.
 */
static void
doc_changed_identity (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));
    LspDoc *ldoc = lsp_manager_lookup_doc (doc);

    if (ldoc && lsp_doc_is_current (ldoc))
        return;

    lsp_manager_remove_doc (doc);
    lsp_manager_add_doc (doc);
}


static gboolean
lsp_doc_plugin_create (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_connect_swapped (doc, "filename-changed",
                              G_CALLBACK (doc_changed_identity), plugin);
    g_signal_connect_swapped (doc, "notify::lang",
                              G_CALLBACK (doc_changed_identity), plugin);

    lsp_manager_add_doc (doc);

    return TRUE;
}


static void
lsp_doc_plugin_destroy (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_handlers_disconnect_by_data (doc, plugin);

    lsp_manager_remove_doc (doc);
}


/**********************************************************************/
/* The diagnostics pane
 */

static void
pane_location_free (gpointer data)
{
    g_free (data);
}


static GtkTextTag *
severity_tag (LspWindowPlugin *stuff,
              int              severity)
{
    if (severity < 1 || severity > 4)
        severity = LSP_SEVERITY_ERROR;

    return stuff->severity_tag[severity - 1];
}


static void
fill_pane (LspWindowPlugin *stuff)
{
    MooEdit *doc = moo_edit_window_get_active_doc (stuff->window);
    LspDoc *ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    GtkTextBuffer *buffer = doc ? moo_edit_get_buffer (doc) : NULL;
    LspPositionEncoding encoding = LSP_POSITION_ENCODING_UTF16;
    GSList *l;

    moo_line_view_clear (stuff->output);

    if (!ldoc)
        return;

    encoding = lsp_server_get_position_encoding (lsp_doc_get_server (ldoc));

    for (l = lsp_doc_get_diagnostics (ldoc); l != NULL; l = l->next)
    {
        LspDiagnostic *diagnostic = (LspDiagnostic*) l->data;
        LspPaneLocation *location;
        GtkTextIter iter;
        char *text;
        int view_line;

        /*
         * The server counts a character in UTF-16 code units, which is not
         * what the user is shown anywhere else in medit, so the position is
         * resolved in the buffer and read back as a character offset.
         */
        lsp_position_to_iter (buffer, diagnostic->start_line,
                              diagnostic->start_character, encoding, &iter);

        location = g_new0 (LspPaneLocation, 1);
        location->line = gtk_text_iter_get_line (&iter);
        location->character = gtk_text_iter_get_line_offset (&iter);

        view_line = moo_line_view_start_line (stuff->output);

        text = g_strdup_printf ("%d:%d", location->line + 1, location->character + 1);
        moo_line_view_write (stuff->output, text, -1, stuff->location_tag);
        g_free (text);

        moo_line_view_write (stuff->output, "  ", -1, NULL);
        moo_line_view_write (stuff->output, lsp_severity_name (diagnostic->severity),
                             -1, severity_tag (stuff, diagnostic->severity));
        moo_line_view_write (stuff->output, ": ", -1, NULL);
        moo_line_view_write (stuff->output, diagnostic->message, -1, NULL);

        if (diagnostic->source || diagnostic->code)
        {
            text = g_strdup_printf ("  [%s%s%s]",
                                    diagnostic->source ? diagnostic->source : "",
                                    diagnostic->source && diagnostic->code ? " " : "",
                                    diagnostic->code ? diagnostic->code : "");
            moo_line_view_write (stuff->output, text, -1, stuff->detail_tag);
            g_free (text);
        }

        moo_line_view_end_line (stuff->output);

        moo_line_view_set_data (stuff->output, view_line, location, pane_location_free);
        moo_line_view_set_cursor (stuff->output, view_line, MOO_TEXT_CURSOR_LINK);
    }
}


static gboolean
update_pane (LspWindowPlugin *stuff)
{
    stuff->update_idle = 0;
    fill_pane (stuff);
    return FALSE;
}


static void
queue_pane_update (LspWindowPlugin *stuff)
{
    if (!stuff->update_idle)
        stuff->update_idle = g_idle_add_full (G_PRIORITY_LOW,
                                              (GSourceFunc) update_pane,
                                              stuff, NULL);
}


static void
active_doc_changed (LspWindowPlugin *stuff)
{
    queue_pane_update (stuff);
    watch_active_buffer (stuff);
    queue_symbols_update (stuff);
}


static void
diagnostics_changed (MooEdit *doc,
                     gpointer data)
{
    LspWindowPlugin *stuff = (LspWindowPlugin*) data;

    if (doc == moo_edit_window_get_active_doc (stuff->window))
        queue_pane_update (stuff);
}


static gboolean
pane_activate (LspWindowPlugin *stuff,
               int              line)
{
    LspPaneLocation *location;
    MooEditView *view;

    location = (LspPaneLocation*) moo_line_view_get_data (stuff->output, line);

    if (!location)
        return FALSE;

    view = moo_edit_window_get_active_view (stuff->window);

    if (!view)
        return FALSE;

    gtk_widget_grab_focus (GTK_WIDGET (view));
    moo_text_view_move_cursor (MOO_TEXT_VIEW (view), location->line,
                               location->character, FALSE, FALSE);

    return TRUE;
}


/**********************************************************************/
/* The symbol tree
 */

static void
symbols_reply (JsonNode   *result,
               JsonObject *error,
               gpointer    data)
{
    LspWindowPlugin *stuff = (LspWindowPlugin*) data;
    MooEdit *doc;
    LspDoc *ldoc;

    /* The window may have gone away while the server was thinking. */
    if (!g_slist_find (lsp_windows, stuff))
        return;

    stuff->symbols_request = 0;

    if (error || !stuff->symbols_doc)
        return;

    /* And the active document may have changed under the reply. */
    doc = moo_edit_window_get_active_doc (stuff->window);

    if (doc != stuff->symbols_doc)
        return;

    ldoc = lsp_manager_lookup_doc (doc);

    if (!ldoc)
        return;

    lsp_symbols_fill (stuff->symbol_store, result, moo_edit_get_buffer (doc),
                      lsp_server_get_position_encoding (lsp_doc_get_server (ldoc)));

    gtk_tree_view_expand_all (stuff->symbols);
}


static void
clear_symbols_doc (LspWindowPlugin *stuff)
{
    if (stuff->symbols_doc)
    {
        g_object_remove_weak_pointer (G_OBJECT (stuff->symbols_doc),
                                      (gpointer*) &stuff->symbols_doc);
        stuff->symbols_doc = NULL;
    }
}


static void
request_symbols (LspWindowPlugin *stuff)
{
    MooEdit *doc = moo_edit_window_get_active_doc (stuff->window);
    LspDoc *ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    LspServer *server = ldoc ? lsp_doc_get_server (ldoc) : NULL;
    JsonObject *params;

    if (stuff->symbols_request)
    {
        lsp_server_cancel (server, stuff->symbols_request);
        stuff->symbols_request = 0;
    }

    clear_symbols_doc (stuff);

    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "documentSymbolProvider"))
    {
        gtk_tree_store_clear (stuff->symbol_store);
        return;
    }

    /* The server must be looking at the text the answer will be matched
       against, or the positions come back for the previous version. */
    lsp_doc_flush (ldoc);

    stuff->symbols_doc = doc;
    g_object_add_weak_pointer (G_OBJECT (doc), (gpointer*) &stuff->symbols_doc);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));

    stuff->symbols_request = lsp_server_call (server, "textDocument/documentSymbol",
                                              params, symbols_reply, stuff, NULL);
}


static gboolean
symbols_timeout (gpointer data)
{
    LspWindowPlugin *stuff = (LspWindowPlugin*) data;

    stuff->symbols_timeout = 0;
    request_symbols (stuff);

    return G_SOURCE_REMOVE;
}


/*
 * Nothing is asked for while the pane is closed: the tree is the only thing
 * the answer is used for, and a document symbol request is real work for the
 * server.
 */
static void
queue_symbols_update (LspWindowPlugin *stuff)
{
    if (!stuff->symbols || !gtk_widget_get_mapped (GTK_WIDGET (stuff->symbols)))
        return;

    if (stuff->symbols_timeout)
        g_source_remove (stuff->symbols_timeout);

    stuff->symbols_timeout = g_timeout_add (500, symbols_timeout, stuff);
}


static void
symbols_buffer_changed (LspWindowPlugin *stuff)
{
    queue_symbols_update (stuff);
}


static void
watch_active_buffer (LspWindowPlugin *stuff)
{
    MooEdit *doc = moo_edit_window_get_active_doc (stuff->window);
    GtkTextBuffer *buffer = doc ? moo_edit_get_buffer (doc) : NULL;

    if (buffer == stuff->symbols_buffer)
        return;

    if (stuff->symbols_buffer)
        g_signal_handlers_disconnect_by_func (stuff->symbols_buffer,
                                              (gpointer) symbols_buffer_changed, stuff);

    stuff->symbols_buffer = buffer;

    if (buffer)
        g_signal_connect_swapped (buffer, "changed",
                                  G_CALLBACK (symbols_buffer_changed), stuff);
}


static void
symbols_row_activated (LspWindowPlugin *stuff,
                       GtkTreePath     *path)
{
    GtkTreeIter iter;
    MooEditView *view;
    int line = 0, character = 0;

    if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (stuff->symbol_store), &iter, path))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (stuff->symbol_store), &iter,
                        LSP_SYMBOL_COLUMN_LINE, &line,
                        LSP_SYMBOL_COLUMN_CHARACTER, &character,
                        -1);

    view = moo_edit_window_get_active_view (stuff->window);

    if (!view)
        return;

    gtk_widget_grab_focus (GTK_WIDGET (view));
    moo_text_view_move_cursor (MOO_TEXT_VIEW (view), line, character, FALSE, FALSE);
}


static void
symbols_mapped (LspWindowPlugin *stuff)
{
    request_symbols (stuff);
}


static void
show_symbols_cb (MooEditWindow *window)
{
    moo_edit_window_show_pane (window, MOO_LSP_SYMBOLS_PANE_ID);
}


static GtkWidget *
create_symbols_pane (LspWindowPlugin *stuff)
{
    GtkWidget *swin;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    stuff->symbol_store = lsp_symbols_new_store ();
    stuff->symbols = GTK_TREE_VIEW (gtk_tree_view_new_with_model (
                                        GTK_TREE_MODEL (stuff->symbol_store)));
    g_object_unref (stuff->symbol_store);

    gtk_tree_view_set_headers_visible (stuff->symbols, FALSE);
    gtk_tree_view_set_search_column (stuff->symbols, LSP_SYMBOL_COLUMN_NAME);

    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                       "markup", LSP_SYMBOL_COLUMN_MARKUP,
                                                       (const char*) NULL);
    gtk_tree_view_append_column (stuff->symbols, column);

    g_signal_connect_swapped (stuff->symbols, "row-activated",
                              G_CALLBACK (symbols_row_activated), stuff);
    g_signal_connect_swapped (stuff->symbols, "map",
                              G_CALLBACK (symbols_mapped), stuff);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (stuff->symbols));
    gtk_widget_show_all (swin);

    return swin;
}


static void
show_diagnostics_cb (MooEditWindow *window)
{
    moo_edit_window_show_pane (window, MOO_LSP_PLUGIN_ID);
}


static gboolean
lsp_window_plugin_create (LspWindowPlugin *stuff)
{
    GtkWidget *swin;
    MooPaneLabel *label;
    guint i;
    static const char *severity_colors[] = { "#c01c28", "#b5820a", "#1c71d8", "#77767b" };

    stuff->window = MOO_WIN_PLUGIN (stuff)->window;

    /* Before anything can issue a request: a reply looks itself up in here. */
    lsp_windows = g_slist_prepend (lsp_windows, stuff);

    stuff->output = MOO_LINE_VIEW (g_object_new (MOO_TYPE_LINE_VIEW,
                                                 "highlight-current-line", TRUE,
                                                 "highlight-current-line-unfocused", TRUE,
                                                 (const char*) NULL));

    stuff->location_tag = moo_line_view_create_tag (stuff->output, NULL,
                                                    "weight", PANGO_WEIGHT_BOLD,
                                                    (const char*) NULL);
    stuff->detail_tag = moo_line_view_create_tag (stuff->output, NULL,
                                                  "foreground", "#777777",
                                                  (const char*) NULL);

    for (i = 0; i < G_N_ELEMENTS (severity_colors); ++i)
        stuff->severity_tag[i] = moo_line_view_create_tag (stuff->output, NULL,
                                                           "foreground", severity_colors[i],
                                                           (const char*) NULL);

    g_signal_connect_swapped (stuff->output, "activate",
                              G_CALLBACK (pane_activate), stuff);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (stuff->output));
    gtk_widget_show_all (swin);

    label = moo_pane_label_new (GTK_STOCK_DIALOG_WARNING, NULL,
                                _("Diagnostics"), _("Diagnostics"));
    stuff->pane = moo_edit_window_add_pane (stuff->window, MOO_LSP_PLUGIN_ID,
                                            swin, label, MOO_PANE_POS_BOTTOM);
    moo_pane_label_free (label);

    label = moo_pane_label_new (GTK_STOCK_INDEX, NULL,
                                _("Symbols"), _("Symbols"));
    stuff->symbols_pane = moo_edit_window_add_pane (stuff->window,
                                                    MOO_LSP_SYMBOLS_PANE_ID,
                                                    create_symbols_pane (stuff),
                                                    label, MOO_PANE_POS_RIGHT);
    moo_pane_label_free (label);

    g_signal_connect_swapped (stuff->window, "notify::active-doc",
                              G_CALLBACK (active_doc_changed), stuff);

    lsp_manager_add_listener (diagnostics_changed, stuff);

    watch_active_buffer (stuff);

    return TRUE;
}


static void
lsp_window_plugin_destroy (LspWindowPlugin *stuff)
{
    lsp_windows = g_slist_remove (lsp_windows, stuff);

    lsp_manager_remove_listener (diagnostics_changed, stuff);

    if (stuff->update_idle)
        g_source_remove (stuff->update_idle);
    stuff->update_idle = 0;

    if (stuff->symbols_timeout)
        g_source_remove (stuff->symbols_timeout);
    stuff->symbols_timeout = 0;

    clear_symbols_doc (stuff);

    if (stuff->symbols_buffer)
        g_signal_handlers_disconnect_by_data (stuff->symbols_buffer, stuff);
    stuff->symbols_buffer = NULL;

    g_signal_handlers_disconnect_by_data (stuff->window, stuff);

    stuff->output = NULL;
    stuff->pane = NULL;
    stuff->symbols = NULL;
    stuff->symbol_store = NULL;
    stuff->symbols_pane = NULL;

    moo_edit_window_remove_pane (stuff->window, MOO_LSP_PLUGIN_ID);
    moo_edit_window_remove_pane (stuff->window, MOO_LSP_SYMBOLS_PANE_ID);
}


/**********************************************************************/
/* The plugin
 */

static gboolean
lsp_plugin_init (LspPlugin *plugin)
{
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditor *editor = moo_editor_instance ();
    MooUiXml *xml = moo_editor_get_ui_xml (editor);

    g_return_val_if_fail (klass != NULL, FALSE);

    moo_prefs_new_key_bool (MOO_LSP_PREFS_ENABLED, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_DIAGNOSTICS, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_COMPLETION, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_HOVER, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_DEBUG, FALSE);
    moo_prefs_new_key_int (MOO_LSP_PREFS_SYNC_DELAY, MOO_LSP_SYNC_DELAY_DEFAULT);

    moo_window_class_new_action (klass, "ShowLspDiagnostics", NULL,
                                 "display-name", _("Diagnostics"),
                                 "label", _("Diagnostics"),
                                 "tooltip", _("Show the diagnostics pane"),
                                 "stock-id", GTK_STOCK_DIALOG_WARNING,
                                 "closure-callback", show_diagnostics_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "ShowLspSymbols", NULL,
                                 "display-name", _("Symbols"),
                                 "label", _("Symbols"),
                                 "tooltip", _("Show the symbol tree"),
                                 "stock-id", GTK_STOCK_INDEX,
                                 "closure-callback", show_symbols_cb,
                                 nullptr);

    if (xml)
    {
        plugin->ui_merge_id = moo_ui_xml_new_merge_id (xml);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Tools",
                             "ShowLspDiagnostics", "ShowLspDiagnostics", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Tools",
                             "ShowLspSymbols", "ShowLspSymbols", -1);
    }

    g_type_class_unref (klass);

    lsp_manager_init ();

    return TRUE;
}


static void
lsp_plugin_deinit (LspPlugin *plugin)
{
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditor *editor = moo_editor_instance ();
    MooUiXml *xml = moo_editor_get_ui_xml (editor);

    moo_window_class_remove_action (klass, "ShowLspDiagnostics");
    moo_window_class_remove_action (klass, "ShowLspSymbols");

    if (plugin->ui_merge_id)
        moo_ui_xml_remove_ui (xml, plugin->ui_merge_id);
    plugin->ui_merge_id = 0;

    g_type_class_unref (klass);

    /* Leaves no language server behind. */
    lsp_manager_shutdown ();
}


MOO_PLUGIN_DEFINE (Lsp, lsp,
                   NULL, NULL, NULL, NULL,
                   NULL,
                   lsp_window_plugin_get_type (), lsp_doc_plugin_get_type ())


gboolean
moo_lsp_plugin_init (void)
{
    return moo_plugin_register (MOO_LSP_PLUGIN_ID,
                                lsp_plugin_get_type (),
                                &lsp_plugin_info,
                                NULL);
}
