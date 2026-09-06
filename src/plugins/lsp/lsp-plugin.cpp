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
#include "plugins/lsp/lsp-navigate.h"
#include "plugins/lsp/lsp-completion.h"

#include "mooedit/mooplugin-macro.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"
#include "mooedit/mooedit-accels.h"
#include "mooedit/mooeditaction-factory.h"
#include "mooedit/mootextview.h"
#include "plugins/support/moolineview.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moopane.h"
#include "mooutils/moostock.h"
#include "mooutils/mooaccel.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooutils-misc.h"

typedef struct {
    MooPlugin parent;
    guint     ui_merge_id;
    guint     doc_ui_merge_id;
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


/*
 * A change of preferences reaches what is already running here. Turning the
 * client off stops every server; turning it on again has to walk the open
 * documents, since the ones opened while it was off were never attached.
 */
void
_moo_lsp_apply_prefs (void)
{
    gboolean enabled = moo_prefs_get_bool (MOO_LSP_PREFS_ENABLED);

    if (!enabled)
    {
        if (lsp_manager_is_running ())
            lsp_manager_shutdown ();
        return;
    }

    if (!lsp_manager_is_running ())
    {
        MooEditArray *docs = moo_editor_get_docs (moo_editor_instance ());
        guint i;

        lsp_manager_init ();

        for (i = 0; i < moo_edit_array_get_size (docs); ++i)
            lsp_manager_add_doc (docs->elms[i]);

        moo_edit_array_free (docs);
        return;
    }

    lsp_manager_refresh_diagnostics ();
}


gboolean
_moo_lsp_debug (void)
{
    return moo_getenv_bool ("MEDIT_LSP_DEBUG") ||
           moo_prefs_get_bool (MOO_LSP_PREFS_DEBUG);
}


/**********************************************************************/
/* Hooks on a document view
 */

#define LSP_VIEW_HOOKED_QUARK "moo-lsp-view-hooked"

/*
 * moo_window_key_press_event() hands the key to the focused widget before it
 * tries the accelerators, so the text view swallows Ctrl+Space and the
 * LspComplete action never fires. The action's accelerator is therefore
 * matched here by hand, the way the terminal matches its own -- and by
 * reading it back from the accel map rather than from the default, so that a
 * user who rebound it still gets what they bound.
 */
static gboolean
complete_accel_pressed (MooEditView *view,
                        GdkEventKey *event)
{
    MooEditWindow *window = moo_edit_view_get_window (view);
    GtkAction *action;
    const char *accel_path;
    const char *accel;
    guint key;
    GdkModifierType mods;

    if (!window)
        return FALSE;

    action = moo_window_get_action (MOO_WINDOW (window), "LspComplete");

    if (!action)
        return FALSE;

    accel_path = gtk_action_get_accel_path (action);

    if (!accel_path)
        return FALSE;

    /*
     * _moo_get_accel() answers out of the map of accelerators that were
     * actually set, which is empty for one that has only ever had its
     * default; _moo_accel_register() puts the default in a map of its own.
     */
    accel = _moo_get_accel (accel_path);

    if (!accel || !accel[0])
        accel = _moo_get_default_accel (accel_path);

    if (!accel || !accel[0] || !_moo_accel_parse (accel, &key, &mods))
        return FALSE;

    return moo_accel_check_event (GTK_WIDGET (view), event, key, mods);
}


static gboolean
view_key_press (MooEditView            *view,
                GdkEventKey            *event,
                G_GNUC_UNUSED gpointer  data)
{
    /*
     * Connected without _after, so this runs before MooTextView's own class
     * handler and the popup gets Up, Down, Enter and Escape before the text
     * view does anything with them.
     */
    lsp_navigate_forget_click ();

    if (lsp_completion_key_press (view, event))
        return TRUE;

    if (complete_accel_pressed (view, event))
    {
        lsp_completion_start (view, NULL);
        return TRUE;
    }

    return FALSE;
}


static gboolean
view_focus_out (G_GNUC_UNUSED MooEditView    *view,
                G_GNUC_UNUSED GdkEventFocus *event,
                G_GNUC_UNUSED gpointer       data)
{
    if (lsp_completion_visible ())
        lsp_completion_cancel ();

    return FALSE;
}


static gboolean
view_button_press (MooEditView            *view,
                   GdkEventButton         *event,
                   G_GNUC_UNUSED gpointer  data)
{
    if (lsp_completion_visible ())
        lsp_completion_cancel ();

    /*
     * Remembered for the context menu: GtkTextView leaves the cursor where it
     * was on a right click, so an entry that went by the cursor would answer
     * about the wrong place unless the word had been selected first. Any other
     * button moves the cursor itself, and then the cursor is the truth.
     */
    if (event->button == 3)
        lsp_navigate_note_click (view, (int) event->x, (int) event->y);
    else
        lsp_navigate_forget_click ();

    return FALSE;
}


static gboolean
view_query_tooltip (MooEditView            *view,
                    int                     x,
                    int                     y,
                    gboolean                keyboard_mode,
                    GtkTooltip             *tooltip,
                    G_GNUC_UNUSED gpointer  data)
{
    return lsp_hover_query_tooltip (view, x, y, keyboard_mode, tooltip);
}


/*
 * Views are hooked as they turn up rather than through a signal, since a
 * window has no notification for a view being added and a split view is
 * created long after the document is. The context menu is not done here:
 * medit builds it from its own ui xml rather than from GtkTextView's
 * ::populate-popup, so the entry is a document action instead.
 */
static void
hook_view (MooEditView *view)
{
    if (!view || g_object_get_data (G_OBJECT (view), LSP_VIEW_HOOKED_QUARK))
        return;

    g_object_set_data (G_OBJECT (view), LSP_VIEW_HOOKED_QUARK, GINT_TO_POINTER (TRUE));

    gtk_widget_set_has_tooltip (GTK_WIDGET (view), TRUE);

    g_signal_connect (view, "query-tooltip",
                      G_CALLBACK (view_query_tooltip), NULL);
    g_signal_connect (view, "key-press-event",
                      G_CALLBACK (view_key_press), NULL);
    g_signal_connect (view, "focus-out-event",
                      G_CALLBACK (view_focus_out), NULL);
    g_signal_connect (view, "button-press-event",
                      G_CALLBACK (view_button_press), NULL);
}


static void
hook_views_of_doc (MooEdit *doc)
{
    MooEditViewArray *views;
    guint i;

    if (!doc)
        return;

    views = moo_edit_get_views (doc);

    for (i = 0; i < moo_edit_view_array_get_size (views); ++i)
        hook_view (views->elms[i]);

    moo_edit_view_array_free (views);
}


/**********************************************************************/
/* The document
 */

static void
goto_definition_doc_cb (MooEdit *doc)
{
    MooEditView *view = moo_edit_get_view (doc);

    if (view)
        lsp_goto_definition_at_click (view, "textDocument/definition");
}


/*
 * The context menu entry is only worth showing on a document some server
 * handles. Whether that server can answer the question is checked again when
 * the entry is used, since it may still be starting up.
 */
static void
update_doc_actions (MooEdit *doc)
{
    GtkAction *action = moo_edit_get_action_by_id (doc, "LspGoToDefinition");

    if (action)
        g_object_set (action, "visible",
                      lsp_manager_lookup_doc (doc) != NULL, (const char*) NULL);
}

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
    update_doc_actions (doc);
}


/*
 * After the text is in, so that the trigger character the server named is
 * already part of the document when it is asked what could follow it.
 */
static void
buffer_insert_text (G_GNUC_UNUSED GtkTextBuffer *buffer,
                    G_GNUC_UNUSED GtkTextIter   *iter,
                    const char                  *text,
                    int                          len,
                    MooEdit                     *doc)
{
    MooEditView *view = moo_edit_get_view (doc);
    char *copy;

    if (!view || !text)
        return;

    copy = len < 0 ? g_strdup (text) : g_strndup (text, len);
    lsp_completion_text_inserted (view, copy);
    g_free (copy);
}


static gboolean
lsp_doc_plugin_create (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_connect_after (moo_edit_get_buffer (doc), "insert-text",
                            G_CALLBACK (buffer_insert_text), doc);

    g_signal_connect_swapped (doc, "filename-changed",
                              G_CALLBACK (doc_changed_identity), plugin);
    g_signal_connect_swapped (doc, "notify::lang",
                              G_CALLBACK (doc_changed_identity), plugin);

    lsp_manager_add_doc (doc);
    hook_views_of_doc (doc);
    update_doc_actions (doc);

    return TRUE;
}


static void
lsp_doc_plugin_destroy (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_handlers_disconnect_by_data (doc, plugin);
    g_signal_handlers_disconnect_by_func (moo_edit_get_buffer (doc),
                                          (gpointer) buffer_insert_text, doc);

    if (lsp_completion_visible ())
        lsp_completion_cancel ();

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
    hook_views_of_doc (moo_edit_window_get_active_doc (stuff->window));
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
edit_config_cb (MooEditWindow *window)
{
    _moo_lsp_edit_config (GTK_WIDGET (window));
}


static void
restart_servers_cb (G_GNUC_UNUSED MooEditWindow *window)
{
    lsp_manager_reload ();
}


static void
complete_cb (MooEditWindow *window)
{
    MooEditView *view = moo_edit_window_get_active_view (window);

    if (view)
        lsp_completion_start (view, NULL);
}


static void
goto_definition_cb (MooEditWindow *window)
{
    lsp_goto_definition (window, "textDocument/definition");
}


static void
goto_type_definition_cb (MooEditWindow *window)
{
    lsp_goto_definition (window, "textDocument/typeDefinition");
}


static void
goto_implementation_cb (MooEditWindow *window)
{
    lsp_goto_definition (window, "textDocument/implementation");
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

    moo_window_class_new_action (klass, "GoToDefinition", NULL,
                                 "display-name", _("Go to Definition"),
                                 "label", _("Go to _Definition"),
                                 "tooltip", _("Go to the definition of what is under the cursor"),
                                 "default-accel", MOO_EDIT_ACCEL_GO_TO_DEFINITION,
                                 "closure-callback", goto_definition_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "GoToTypeDefinition", NULL,
                                 "display-name", _("Go to Type Definition"),
                                 "label", _("Go to _Type Definition"),
                                 "tooltip", _("Go to the definition of the type of what is under the cursor"),
                                 "closure-callback", goto_type_definition_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "GoToImplementation", NULL,
                                 "display-name", _("Go to Implementation"),
                                 "label", _("Go to _Implementation"),
                                 "tooltip", _("Go to the implementation of what is under the cursor"),
                                 "closure-callback", goto_implementation_cb,
                                 nullptr);

    {
        /*
         * The document context menu is built from the document ui xml with
         * document actions, so the entry there is registered separately from
         * the window action above.
         */
        MooEditClass *edit_klass = (MooEditClass*) g_type_class_ref (MOO_TYPE_EDIT);
        MooUiXml *doc_xml = moo_editor_get_doc_ui_xml (editor);

        moo_edit_class_new_action (edit_klass, "LspGoToDefinition",
                                   "display-name", _("Go to Definition"),
                                   "label", _("Go to _Definition"),
                                   "tooltip", _("Go to the definition of what is under the cursor"),
                                   "closure-callback", goto_definition_doc_cb,
                                   (char*) 0);

        if (doc_xml)
        {
            plugin->doc_ui_merge_id = moo_ui_xml_new_merge_id (doc_xml);
            moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id,
                                 "Editor/Popup/PopupStart",
                                 "LspGoToDefinition", "LspGoToDefinition", -1);
        }

        g_type_class_unref (edit_klass);
    }

    moo_window_class_new_action (klass, "LspEditConfig", NULL,
                                 "display-name", _("LSP Servers"),
                                 "label", _("LSP _Servers..."),
                                 "tooltip", _("Edit the list of language servers"),
                                 "closure-callback", edit_config_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspRestartServers", NULL,
                                 "display-name", _("Restart Language Servers"),
                                 "label", _("Restart Language Servers"),
                                 "tooltip", _("Re-read the configuration and start every server again"),
                                 "stock-id", MOO_STOCK_RESTART,
                                 "closure-callback", restart_servers_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspComplete", NULL,
                                 "display-name", _("Complete Word"),
                                 "label", _("_Complete Word"),
                                 "tooltip", _("Ask the language server what could go here"),
                                 "default-accel", MOO_EDIT_ACCEL_COMPLETE,
                                 "closure-callback", complete_cb,
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
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Tools",
                             "LspEditConfig", "LspEditConfig", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Tools",
                             "LspRestartServers", "LspRestartServers", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "LspComplete", "LspComplete", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToDefinition", "GoToDefinition", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToTypeDefinition", "GoToTypeDefinition", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToImplementation", "GoToImplementation", -1);
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
    moo_window_class_remove_action (klass, "GoToDefinition");
    moo_window_class_remove_action (klass, "GoToTypeDefinition");
    moo_window_class_remove_action (klass, "GoToImplementation");
    moo_window_class_remove_action (klass, "LspComplete");
    moo_window_class_remove_action (klass, "LspEditConfig");
    moo_window_class_remove_action (klass, "LspRestartServers");

    lsp_completion_cancel ();
    lsp_navigate_reset ();

    if (plugin->ui_merge_id)
        moo_ui_xml_remove_ui (xml, plugin->ui_merge_id);
    plugin->ui_merge_id = 0;

    {
        MooEditClass *edit_klass = (MooEditClass*) g_type_class_ref (MOO_TYPE_EDIT);
        MooUiXml *doc_xml = moo_editor_get_doc_ui_xml (editor);

        moo_edit_class_remove_action (edit_klass, "LspGoToDefinition");

        if (plugin->doc_ui_merge_id && doc_xml)
            moo_ui_xml_remove_ui (doc_xml, plugin->doc_ui_merge_id);
        plugin->doc_ui_merge_id = 0;

        g_type_class_unref (edit_klass);
    }

    g_type_class_unref (klass);

    /* Leaves no language server behind. */
    lsp_manager_shutdown ();
}


MOO_PLUGIN_DEFINE (Lsp, lsp,
                   NULL, NULL, NULL, NULL,
                   _moo_lsp_prefs_page,
                   lsp_window_plugin_get_type (), lsp_doc_plugin_get_type ())


gboolean
moo_lsp_plugin_init (void)
{
    return moo_plugin_register (MOO_LSP_PLUGIN_ID,
                                lsp_plugin_get_type (),
                                &lsp_plugin_info,
                                NULL);
}
