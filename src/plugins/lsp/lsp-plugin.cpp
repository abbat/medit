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


#include "plugins/lsp/lsp-plugin.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-diagnostics.h"
#include "plugins/lsp/lsp-symbols.h"
#include "plugins/lsp/lsp-navigate.h"
#include "plugins/lsp/lsp-references.h"
#include "plugins/lsp/lsp-edits.h"
#include "plugins/lsp/lsp-completion.h"
#include "plugins/lsp/lsp-highlight.h"
#include "plugins/lsp/lsp-signature.h"

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

    MooLineView   *references;
    MooPane       *references_pane;
    GtkTextTag    *reference_place_tag;
    GtkTextTag    *reference_none_tag;

    GtkTreeView   *symbols;
    GtkTreeStore  *symbol_store;
    MooPane       *symbols_pane;
    MooEdit       *symbols_doc;         /* what the pending request is about */
    GtkTextBuffer *symbols_buffer;      /* connected to ::changed */
    gint64         symbols_request;
    guint          symbols_timeout;
} LspWindowPlugin;

#define MOO_LSP_SYMBOLS_PANE_ID "LspSymbols"
#define MOO_LSP_REFERENCES_PANE_ID "LspReferences"

/*
 * Every live window plugin. A reply from a server can arrive after its window
 * is gone, so a callback checks that its own is still in here first.
 */
static GSList *lsp_windows;

static void     watch_active_buffer     (LspWindowPlugin *stuff);
static void     queue_symbols_update    (LspWindowPlugin *stuff);
static void     clear_symbols_doc       (LspWindowPlugin *stuff);
static GtkWidget *create_symbols_pane   (LspWindowPlugin *stuff);
static GtkWidget *create_references_pane (LspWindowPlugin *stuff);

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
 * A change of preferences reaches what is already running here. Switching the
 * client as a whole on and off is not one of them: that is the plugin's own
 * enabled state, and the framework attaches and detaches everything itself.
 */
void
_moo_lsp_apply_prefs (void)
{
    lsp_manager_refresh_diagnostics ();

    /*
     * The marks are already on the text, and nothing else would take them off:
     * with the setting gone the next question is never asked, and the answer
     * to the last one would stay on screen.
     */
    if (!moo_prefs_get_bool (MOO_LSP_PREFS_HIGHLIGHT))
        lsp_highlight_clear ();
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
 * matched here by hand, the way the terminal matches its own -- and against
 * what the accelerator is now rather than what it was compiled as, so that a
 * user who rebound it gets what they bound and one who cleared it gets
 * nothing.
 */
static gboolean
accel_pressed (MooEditView *view,
               GdkEventKey *event,
               const char  *action_id)
{
    MooEditWindow *window = moo_edit_view_get_window (view);

    if (!window)
        return FALSE;

    return _moo_accel_check_action_event (GTK_WIDGET (view), event,
                                          moo_window_get_action (MOO_WINDOW (window),
                                                                 action_id));
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

    if (lsp_signature_key_press (view, event))
        return TRUE;

    if (accel_pressed (view, event, "LspComplete"))
    {
        lsp_completion_start (view, NULL);
        return TRUE;
    }

    if (accel_pressed (view, event, "LspSignature"))
    {
        lsp_signature_start (view, NULL);
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

    lsp_signature_cancel ();

    return FALSE;
}


static gboolean
view_button_press (MooEditView            *view,
                   GdkEventButton         *event,
                   G_GNUC_UNUSED gpointer  data)
{
    if (lsp_completion_visible ())
        lsp_completion_cancel ();

    /* A click is the cursor going somewhere else, which is the end of the call
       that was being typed. */
    lsp_signature_cancel ();

    /*
     * Remembered for the context menu: GtkTextView leaves the cursor where it
     * was on a right click, so an entry that went by the cursor would answer
     * about the wrong place unless the word had been selected first. Any other
     * button moves the cursor itself, and then the cursor is the truth.
     */
    if (event->button == 3)
        lsp_navigate_note_click (view, event->window,
                                 (int) event->x, (int) event->y);
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
 * The window the view is in, which is where the answer will be shown. A view
 * that is in none is a document being taken apart, and there is nothing to
 * ask about it.
 */
static MooEditWindow *
window_of_doc (MooEdit      *doc,
               MooEditView **view_out)
{
    MooEditView *view = moo_edit_get_view (doc);

    if (view_out)
        *view_out = view;

    return view ? moo_edit_view_get_window (view) : NULL;
}


static void
find_references_doc_cb (MooEdit *doc)
{
    MooEditView *view = NULL;
    MooEditWindow *window = window_of_doc (doc, &view);

    if (window)
        lsp_find_references (window, view);
}


static void
rename_doc_cb (MooEdit *doc)
{
    MooEditView *view = NULL;
    MooEditWindow *window = window_of_doc (doc, &view);

    if (window)
        lsp_rename (window, view);
}


/*
 * The context menu entry is only worth showing on a document some server
 * handles. Whether that server can answer the question is checked again when
 * the entry is used, since it may still be starting up.
 */
static void
update_doc_actions (MooEdit *doc)
{
    static const char *ids[] = {
        "LspGoToDefinition", "LspFindReferences", "LspRename"
    };
    gboolean handled = lsp_manager_lookup_doc (doc) != NULL;
    guint i;

    for (i = 0; i < G_N_ELEMENTS (ids); ++i)
    {
        GtkAction *action = moo_edit_get_action_by_id (doc, ids[i]);

        if (action)
            g_object_set (action, "visible", handled, (const char*) NULL);
    }
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
    lsp_signature_text_inserted (view, copy);
    g_free (copy);
}


/*
 * Where the cursor is is what the highlights are about, and a buffer says so
 * through a property rather than a signal of its own -- ::mark-set fires for
 * every mark there is, several times per keystroke.
 */
static void
buffer_cursor_moved (MooEdit *doc)
{
    lsp_highlight_cursor_moved (doc);
}


static gboolean
lsp_doc_plugin_create (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_connect_after (moo_edit_get_buffer (doc), "insert-text",
                            G_CALLBACK (buffer_insert_text), doc);
    g_signal_connect_swapped (moo_edit_get_buffer (doc), "notify::cursor-position",
                              G_CALLBACK (buffer_cursor_moved), doc);

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
    g_signal_handlers_disconnect_by_func (moo_edit_get_buffer (doc),
                                          (gpointer) buffer_cursor_moved, doc);

    if (lsp_completion_visible ())
        lsp_completion_cancel ();

    lsp_signature_cancel ();
    lsp_highlight_clear ();

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
    LspServer *server;
    GSList *l;

    moo_line_view_clear (stuff->output);

    if (!ldoc)
        return;

    server = lsp_doc_get_server (ldoc);

    /*
     * A server that has given up says why, and here is where it is said. This
     * is not a diagnostic and is not switched off with them below: it is the
     * reason there are none, and without it the pane is empty in exactly the
     * way a document with nothing wrong with it is.
     */
    if (lsp_server_get_state (server) == LSP_SERVER_FAILED)
    {
        const char *message = lsp_server_get_error (server);

        if (message)
            moo_line_view_write_line (stuff->output, message, -1,
                                      severity_tag (stuff, LSP_SEVERITY_ERROR));
        return;
    }

    /*
     * "Underline problems and list them in the Diagnostics pane" -- the second
     * half of that sentence is this. Without it the marks came off the document
     * and the pane went on listing them.
     */
    if (!moo_prefs_get_bool (MOO_LSP_PREFS_DIAGNOSTICS))
        return;

    encoding = lsp_server_get_position_encoding (server);

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

        text = lsp_diagnostic_detail (diagnostic);

        if (text)
        {
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
/* The references pane
 */

/* The window plugin of a window, for a reply that has to find its way back. */
static LspWindowPlugin *
window_plugin_of (MooEditWindow *window)
{
    GSList *l;

    for (l = lsp_windows; l != NULL; l = l->next)
    {
        LspWindowPlugin *stuff = (LspWindowPlugin*) l->data;

        if (stuff->window == window)
            return stuff;
    }

    return NULL;
}


static gboolean
references_activate (LspWindowPlugin *stuff,
                     int              line)
{
    LspLocation *location;

    location = (LspLocation*) moo_line_view_get_data (stuff->references, line);

    if (!location)
        return FALSE;

    lsp_go_to_place (stuff->window, location->path, location->line,
                     location->character, location->encoding);

    return TRUE;
}


void
_moo_lsp_show_references (MooEditWindow *window,
                          GSList        *locations,
                          const char    *message)
{
    LspWindowPlugin *stuff = window_plugin_of (window);
    GSList *l;

    if (!stuff || !stuff->references)
    {
        lsp_locations_free (locations);
        return;
    }

    moo_line_view_clear (stuff->references);

    if (!locations)
        moo_line_view_write_line (stuff->references, message, -1,
                                  stuff->reference_none_tag);

    for (l = locations; l != NULL; l = l->next)
    {
        LspLocation *location = (LspLocation*) l->data;
        int view_line = moo_line_view_start_line (stuff->references);

        moo_line_view_write (stuff->references, location->display, -1,
                             stuff->reference_place_tag);

        if (location->text)
        {
            moo_line_view_write (stuff->references, "  ", -1, NULL);
            moo_line_view_write (stuff->references, location->text, -1, NULL);
        }

        moo_line_view_end_line (stuff->references);

        /* The line owns the location from here, and takes it with it when the
           pane is cleared or the window closed. */
        moo_line_view_set_data (stuff->references, view_line, location,
                                (GDestroyNotify) lsp_location_free);
        moo_line_view_set_cursor (stuff->references, view_line, MOO_TEXT_CURSOR_LINK);
    }

    g_slist_free (locations);

    /*
     * Presented rather than merely filled: nobody opened this pane, and an
     * answer that arrives where it cannot be seen is the same as no answer.
     */
    moo_edit_window_show_pane (window, MOO_LSP_REFERENCES_PANE_ID);
}


static GtkWidget *
create_references_pane (LspWindowPlugin *stuff)
{
    GtkWidget *swin;

    stuff->references = MOO_LINE_VIEW (g_object_new (MOO_TYPE_LINE_VIEW,
                                                     "highlight-current-line", TRUE,
                                                     "highlight-current-line-unfocused", TRUE,
                                                     (const char*) NULL));

    stuff->reference_place_tag = moo_line_view_create_tag (stuff->references, NULL,
                                                           "weight", PANGO_WEIGHT_BOLD,
                                                           (const char*) NULL);
    stuff->reference_none_tag = moo_line_view_create_tag (stuff->references, NULL,
                                                          "foreground", "#777777",
                                                          (const char*) NULL);

    g_signal_connect_swapped (stuff->references, "activate",
                              G_CALLBACK (references_activate), stuff);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (stuff->references));
    gtk_widget_show_all (swin);

    return swin;
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
find_references_cb (MooEditWindow *window)
{
    lsp_find_references (window, NULL);
}


static void
rename_cb (MooEditWindow *window)
{
    lsp_rename (window, NULL);
}


static void
format_cb (MooEditWindow *window)
{
    lsp_format (window);
}


static void
show_references_cb (MooEditWindow *window)
{
    moo_edit_window_show_pane (window, MOO_LSP_REFERENCES_PANE_ID);
}


static void
signature_cb (MooEditWindow *window)
{
    MooEditView *view = moo_edit_window_get_active_view (window);

    if (view)
        lsp_signature_start (view, NULL);
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

    /*
     * Filled when it is opened, and not only when something changes: a server
     * that failed while the pane was closed has nothing left to announce, and
     * the pane would come up empty on the one occasion the user opens it to
     * find out why.
     */
    g_signal_connect_swapped (swin, "map", G_CALLBACK (queue_pane_update), stuff);

    label = moo_pane_label_new (MOO_STOCK_FIND_IN_FILES, NULL,
                                _("References"), _("References"));
    stuff->references_pane = moo_edit_window_add_pane (stuff->window,
                                                       MOO_LSP_REFERENCES_PANE_ID,
                                                       create_references_pane (stuff),
                                                       label, MOO_PANE_POS_BOTTOM);
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
    stuff->references = NULL;
    stuff->references_pane = NULL;
    stuff->symbols = NULL;
    stuff->symbol_store = NULL;
    stuff->symbols_pane = NULL;

    moo_edit_window_remove_pane (stuff->window, MOO_LSP_PLUGIN_ID);
    moo_edit_window_remove_pane (stuff->window, MOO_LSP_REFERENCES_PANE_ID);
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

    moo_prefs_new_key_bool (MOO_LSP_PREFS_DIAGNOSTICS, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_COMPLETION, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_HOVER, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_SIGNATURE, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_HIGHLIGHT, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_DEBUG, FALSE);
    moo_prefs_new_key_int (MOO_LSP_PREFS_SYNC_DELAY, MOO_LSP_SYNC_DELAY_DEFAULT);

    /* A section of its own in Configure Shortcuts; see the terminal for why,
       and note that it is part of the accelerator path -- these are
       Shortcuts/Editor/Lsp/... in the preferences. */
    if (!moo_window_class_find_group (klass, MOO_LSP_PLUGIN_ID))
        moo_window_class_new_group (klass, MOO_LSP_PLUGIN_ID, _("LSP"));

    moo_window_class_new_action (klass, "ShowLspDiagnostics", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Diagnostics"),
                                 "label", _("Diagnostics"),
                                 "tooltip", _("Show the diagnostics pane"),
                                 "stock-id", GTK_STOCK_DIALOG_WARNING,
                                 "closure-callback", show_diagnostics_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "GoToDefinition", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Go to Definition"),
                                 "label", _("Go to _Definition"),
                                 "tooltip", _("Go to the definition of what is under the cursor"),
                                 "default-accel", MOO_EDIT_ACCEL_GO_TO_DEFINITION,
                                 "closure-callback", goto_definition_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "FindReferences", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Find References"),
                                 "label", _("Find _References"),
                                 "tooltip", _("List every use of what is under the cursor"),
                                 "stock-id", MOO_STOCK_FIND_IN_FILES,
                                 "default-accel", MOO_EDIT_ACCEL_FIND_REFERENCES,
                                 "closure-callback", find_references_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspFormat", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Format Document"),
                                 "label", _("_Format Document"),
                                 "tooltip", _("Let the language server lay the document out"),
                                 "closure-callback", format_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "RenameSymbol", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Rename"),
                                 "label", _("_Rename..."),
                                 "tooltip", _("Rename what is under the cursor everywhere"),
                                 "default-accel", MOO_EDIT_ACCEL_RENAME,
                                 "closure-callback", rename_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "GoToTypeDefinition", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Go to Type Definition"),
                                 "label", _("Go to _Type Definition"),
                                 "tooltip", _("Go to the definition of the type of what is under the cursor"),
                                 "closure-callback", goto_type_definition_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "GoToImplementation", MOO_LSP_PLUGIN_ID,
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

        moo_edit_class_new_action (edit_klass, "LspFindReferences",
                                   "display-name", _("Find References"),
                                   "label", _("Find _References"),
                                   "tooltip", _("List every use of what is under the cursor"),
                                   "closure-callback", find_references_doc_cb,
                                   (char*) 0);

        moo_edit_class_new_action (edit_klass, "LspRename",
                                   "display-name", _("Rename"),
                                   "label", _("_Rename..."),
                                   "tooltip", _("Rename what is under the cursor everywhere"),
                                   "closure-callback", rename_doc_cb,
                                   (char*) 0);

        if (doc_xml)
        {
            plugin->doc_ui_merge_id = moo_ui_xml_new_merge_id (doc_xml);
            moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id,
                                 "Editor/Popup/PopupStart",
                                 "LspGoToDefinition", "LspGoToDefinition", -1);
            moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id,
                                 "Editor/Popup/PopupStart",
                                 "LspFindReferences", "LspFindReferences", -1);
            moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id,
                                 "Editor/Popup/PopupStart",
                                 "LspRename", "LspRename", -1);
        }

        g_type_class_unref (edit_klass);
    }

    moo_window_class_new_action (klass, "LspEditConfig", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("LSP Servers"),
                                 "label", _("LSP _Servers..."),
                                 "tooltip", _("Edit the list of language servers"),
                                 "stock-id", GTK_STOCK_INDEX,
                                 "closure-callback", edit_config_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspRestartServers", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Restart Language Servers"),
                                 "label", _("Restart Language Servers"),
                                 "tooltip", _("Re-read the configuration and start every server again"),
                                 "stock-id", MOO_STOCK_RESTART,
                                 "closure-callback", restart_servers_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspComplete", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Complete Word"),
                                 "label", _("_Complete Word"),
                                 "tooltip", _("Ask the language server what could go here"),
                                 "default-accel", MOO_EDIT_ACCEL_COMPLETE,
                                 "closure-callback", complete_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "LspSignature", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("Parameter Hints"),
                                 "label", _("_Parameter Hints"),
                                 "tooltip", _("Show what the call being typed takes"),
                                 "default-accel", MOO_EDIT_ACCEL_SIGNATURE,
                                 "closure-callback", signature_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "ShowLspReferences", MOO_LSP_PLUGIN_ID,
                                 "display-name", _("References"),
                                 "label", _("References"),
                                 "tooltip", _("Show the references pane"),
                                 "stock-id", MOO_STOCK_FIND_IN_FILES,
                                 "closure-callback", show_references_cb,
                                 nullptr);

    moo_window_class_new_action (klass, "ShowLspSymbols", MOO_LSP_PLUGIN_ID,
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
                             "ShowLspReferences", "ShowLspReferences", -1);
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
                             "LspSignature", "LspSignature", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToDefinition", "GoToDefinition", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToTypeDefinition", "GoToTypeDefinition", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "GoToImplementation", "GoToImplementation", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "FindReferences", "FindReferences", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "RenameSymbol", "RenameSymbol", -1);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Document",
                             "LspFormat", "LspFormat", -1);
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
    moo_window_class_remove_action (klass, "ShowLspReferences");
    moo_window_class_remove_action (klass, "ShowLspSymbols");
    moo_window_class_remove_action (klass, "GoToDefinition");
    moo_window_class_remove_action (klass, "GoToTypeDefinition");
    moo_window_class_remove_action (klass, "GoToImplementation");
    moo_window_class_remove_action (klass, "FindReferences");
    moo_window_class_remove_action (klass, "RenameSymbol");
    moo_window_class_remove_action (klass, "LspFormat");
    moo_window_class_remove_action (klass, "LspComplete");
    moo_window_class_remove_action (klass, "LspSignature");
    moo_window_class_remove_action (klass, "LspEditConfig");
    moo_window_class_remove_action (klass, "LspRestartServers");

    lsp_completion_cancel ();
    lsp_signature_cancel ();
    lsp_highlight_clear ();
    lsp_navigate_reset ();

    if (plugin->ui_merge_id)
        moo_ui_xml_remove_ui (xml, plugin->ui_merge_id);
    plugin->ui_merge_id = 0;

    {
        MooEditClass *edit_klass = (MooEditClass*) g_type_class_ref (MOO_TYPE_EDIT);
        MooUiXml *doc_xml = moo_editor_get_doc_ui_xml (editor);

        moo_edit_class_remove_action (edit_klass, "LspGoToDefinition");
        moo_edit_class_remove_action (edit_klass, "LspFindReferences");
        moo_edit_class_remove_action (edit_klass, "LspRename");

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
    /*
     * Off until asked for, unlike the other builtin plugins. This one runs
     * other people's programs -- one per project root, kept alive as long as a
     * document of that project is open -- and doing that on a first run
     * because a language server happens to be installed is not medit's
     * decision to make. Preferences -> Plugins turns it on.
     */
    MooPluginParams params = { FALSE, TRUE };

    return moo_plugin_register (MOO_LSP_PLUGIN_ID,
                                lsp_plugin_get_type (),
                                &lsp_plugin_info,
                                &params);
}
