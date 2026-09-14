/*
 *   plugins/lsp/lsp-actions.cpp
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

#include "plugins/lsp/lsp-actions.h"
#include "plugins/lsp/lsp-edits.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-navigate.h"

#include "mooedit/mooeditor.h"
#include "mooutils/moodialogs.h"
#include "mooutils/mooi18n.h"


/**********************************************************************/
/* Reading the reply
 */

static char *
disabled_reason (JsonObject *object)
{
    JsonObject *disabled = lsp_json_get_object (object, "disabled");
    const char *reason;

    if (!disabled)
        return NULL;

    /*
     * The member is required, so a server that left it out still means the
     * action cannot be used -- which is what has to survive, the text being
     * only what is shown next to it.
     */
    reason = lsp_json_get_string (disabled, "reason");

    return g_strdup (reason ? reason : "");
}


static LspCodeAction *
code_action_new (JsonObject *object)
{
    LspCodeAction *action;
    const char *title = lsp_json_get_string (object, "title");
    JsonNode *command = lsp_json_get_node (object, "command");
    JsonNode *edit = lsp_json_get_node (object, "edit");
    char *disabled = disabled_reason (object);
    const char *name = NULL;
    JsonNode *arguments = NULL;

    if (!title || !title[0])
    {
        g_free (disabled);
        return NULL;
    }

    /*
     * A Command has the name of the command where a CodeAction has the whole
     * Command, and the two are told apart by nothing but that: a string there
     * means this entry is the older shape and its arguments are its own.
     */
    if (command && JSON_NODE_HOLDS_VALUE (command))
    {
        name = lsp_json_get_string (object, "command");
        arguments = lsp_json_get_node (object, "arguments");
    }
    else if (command && JSON_NODE_HOLDS_OBJECT (command))
    {
        JsonObject *inner = json_node_get_object (command);

        name = lsp_json_get_string (inner, "command");
        arguments = lsp_json_get_node (inner, "arguments");
    }

    if (edit && !JSON_NODE_HOLDS_OBJECT (edit))
        edit = NULL;

    if (arguments && !JSON_NODE_HOLDS_ARRAY (arguments))
        arguments = NULL;

    /*
     * Neither an edit nor a command is an action that would have to be asked
     * about again with codeAction/resolve, which medit does not claim and so
     * must not be sent; showing it would offer an entry that does nothing.
     *
     * A disabled one is the exception, and carries neither on purpose: saying
     * what would have to be true for the fix to apply is all it is for.
     */
    if (!edit && !(name && name[0]) && !disabled)
        return NULL;

    action = g_new0 (LspCodeAction, 1);
    action->title = g_strdup (title);
    action->kind = g_strdup (lsp_json_get_string (object, "kind"));
    action->disabled = disabled;
    action->preferred = lsp_json_get_bool (object, "isPreferred", FALSE);
    action->edit = edit ? json_node_copy (edit) : NULL;
    action->command = g_strdup (name);
    action->arguments = arguments ? json_node_copy (arguments) : NULL;

    return action;
}


void
lsp_code_action_free (LspCodeAction *action)
{
    if (!action)
        return;

    g_free (action->title);
    g_free (action->kind);
    g_free (action->disabled);
    g_free (action->command);

    if (action->edit)
        json_node_free (action->edit);
    if (action->arguments)
        json_node_free (action->arguments);

    g_free (action);
}


void
lsp_code_actions_free (GSList *actions)
{
    g_slist_free_full (actions, (GDestroyNotify) lsp_code_action_free);
}


GSList *
lsp_code_actions_parse (JsonNode *result)
{
    JsonArray *array;
    GSList *actions = NULL;
    guint i, n;

    if (!result || !JSON_NODE_HOLDS_ARRAY (result))
        return NULL;

    array = json_node_get_array (result);
    n = json_array_get_length (array);

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        LspCodeAction *action;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        action = code_action_new (json_node_get_object (node));

        if (action)
            actions = g_slist_prepend (actions, action);
    }

    /* The server's order, which is the order it means them to be offered in. */
    return g_slist_reverse (actions);
}


/**********************************************************************/
/* The context a request carries
 */

static int
compare_positions (int line,
                   int character,
                   int other_line,
                   int other_character)
{
    if (line != other_line)
        return line - other_line;

    return character - other_character;
}


/*
 * Whether two ranges have anything in common, ends included. The inclusion
 * matters: asking with no selection asks about an empty range at the cursor,
 * and a cursor left just after the last character of what is underlined is
 * still the user pointing at it.
 */
static gboolean
ranges_touch (int start_line,
              int start_character,
              int end_line,
              int end_character,
              int other_start_line,
              int other_start_character,
              int other_end_line,
              int other_end_character)
{
    return compare_positions (start_line, start_character,
                              other_end_line, other_end_character) <= 0 &&
           compare_positions (other_start_line, other_start_character,
                              end_line, end_character) <= 0;
}


JsonArray *
_lsp_code_action_context (JsonArray *diagnostics,
                          int        start_line,
                          int        start_character,
                          int        end_line,
                          int        end_character)
{
    JsonArray *context = json_array_new ();
    guint i, n = diagnostics ? json_array_get_length (diagnostics) : 0;

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (diagnostics, i);
        int line = 0, character = 0, last_line = 0, last_character = 0;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        if (!lsp_json_get_range (lsp_json_get_object (json_node_get_object (node), "range"),
                                 &line, &character, &last_line, &last_character))
            continue;

        if (!ranges_touch (start_line, start_character, end_line, end_character,
                           line, character, last_line, last_character))
            continue;

        json_array_add_element (context, json_node_copy (node));
    }

    return context;
}


/**********************************************************************/
/* Doing what was picked
 */

typedef struct {
    MooEditWindow *window;      /* weak */
} LspCommandRequest;


static void
command_request_free (gpointer data)
{
    LspCommandRequest *request = (LspCommandRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);

    g_free (request);
}


/*
 * A command runs inside the server and answers with whatever it likes, or
 * with nothing; what it changed arrives separately, as workspace/applyEdit.
 * So there is nothing to do here but repeat a refusal, which otherwise would
 * look like a menu entry that did nothing at all.
 */
static void
command_reply (G_GNUC_UNUSED JsonNode *result,
               JsonObject             *error,
               gpointer                data)
{
    LspCommandRequest *request = (LspCommandRequest*) data;
    MooEditWindow *window = request->window;
    const char *message;

    if (!error || !window || !MOO_IS_EDIT_WINDOW (window))
        return;

    message = lsp_json_get_string (error, "message");

    moo_error_dialog (_("The command failed"),
                      message && message[0] ? message : NULL,
                      GTK_WIDGET (window));
}


typedef struct {
    MooEditWindow      *window;     /* weak */
    LspServer          *server;
    LspPositionEncoding encoding;   /* of that server */
    LspCodeAction      *action;     /* owned */
} LspCodeActionChoice;


static void
choice_free (gpointer  data,
             GClosure *closure G_GNUC_UNUSED)
{
    LspCodeActionChoice *choice = (LspCodeActionChoice*) data;

    if (choice->window)
        g_object_remove_weak_pointer (G_OBJECT (choice->window),
                                      (gpointer*) &choice->window);

    lsp_server_unref (choice->server);
    lsp_code_action_free (choice->action);
    g_free (choice);
}


static void
run_command (LspCodeActionChoice *choice)
{
    LspCodeAction *action = choice->action;
    LspCommandRequest *request;
    JsonObject *params = json_object_new ();

    lsp_json_set_string (params, "command", action->command);

    if (action->arguments)
        lsp_json_set_node (params, "arguments", json_node_copy (action->arguments));

    request = g_new0 (LspCommandRequest, 1);
    request->window = choice->window;
    g_object_add_weak_pointer (G_OBJECT (choice->window),
                               (gpointer*) &request->window);

    lsp_server_call (choice->server, "workspace/executeCommand", params,
                     command_reply, request, command_request_free);
}


static void
action_chosen (G_GNUC_UNUSED GtkMenuItem *item,
               gpointer                   data)
{
    LspCodeActionChoice *choice = (LspCodeActionChoice*) data;
    LspCodeAction *action = choice->action;

    if (!choice->window || !MOO_IS_EDIT_WINDOW (choice->window))
        return;

    /*
     * The edit first when there is both, which is the order the protocol
     * gives them in: the command is what finishes the work the edit began.
     */
    if (action->edit)
    {
        GSList *edits = lsp_workspace_edit_parse (action->edit);

        lsp_text_edits_apply (choice->window, edits, choice->encoding);
        lsp_text_edits_free (edits);
    }

    if (action->command && action->command[0] && lsp_server_is_ready (choice->server))
        run_command (choice);
}


/**********************************************************************/
/* Offering them
 */

/*
 * Under the cursor, the way the completion popup goes: the menu is about a
 * place in the text, and a menu that came up under the pointer would be about
 * wherever the pointer was left.
 */
static void
menu_position (G_GNUC_UNUSED GtkMenu *menu,
               int                   *x,
               int                   *y,
               gboolean              *push_in,
               gpointer               data)
{
    MooEditView *view = (MooEditView*) data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    GdkWindow *window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
                                                  GTK_TEXT_WINDOW_TEXT);
    GtkTextIter iter;
    GdkRectangle rect;
    int window_x = 0, window_y = 0;
    int origin_x = 0, origin_y = 0;

    *push_in = TRUE;

    if (!window)
        return;

    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));
    gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &rect);
    gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                           GTK_TEXT_WINDOW_TEXT,
                                           rect.x, rect.y + rect.height,
                                           &window_x, &window_y);
    gdk_window_get_origin (window, &origin_x, &origin_y);

    *x = origin_x + window_x;
    *y = origin_y + window_y;
}


static GtkWidget *
menu_item_new (LspCodeAction *action)
{
    GtkWidget *item = gtk_menu_item_new_with_label (action->title);

    /*
     * A disabled action is shown rather than dropped: the server saying why
     * this particular fix does not apply here is the answer to the question,
     * and an entry that quietly went missing would read as the server having
     * nothing to say.
     */
    if (action->disabled)
    {
        gtk_widget_set_sensitive (item, FALSE);

        if (action->disabled[0])
            gtk_widget_set_tooltip_text (item, action->disabled);
    }

    return item;
}


static void
show_menu (MooEditWindow       *window,
           MooEditView         *view,
           LspServer           *server,
           LspPositionEncoding  encoding,
           GSList              *actions)
{
    GtkWidget *menu = gtk_menu_new ();
    GSList *l;

    for (l = actions; l != NULL; l = l->next)
    {
        LspCodeAction *action = (LspCodeAction*) l->data;
        GtkWidget *item = menu_item_new (action);
        LspCodeActionChoice *choice = g_new0 (LspCodeActionChoice, 1);

        choice->window = window;
        choice->server = lsp_server_ref (server);
        choice->encoding = encoding;
        choice->action = action;
        g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &choice->window);

        g_signal_connect_data (item, "activate", G_CALLBACK (action_chosen),
                               choice, choice_free, (GConnectFlags) 0);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);
    }

    g_object_ref_sink (menu);
    g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, menu_position, view,
                    0, gtk_get_current_event_time ());
    gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);

    g_object_unref (menu);
}


/**********************************************************************/
/* Asking
 */

typedef struct {
    MooEditWindow      *window;     /* weak */
    MooEditView        *view;       /* weak, the menu is placed in it */
    LspServer          *server;
    LspPositionEncoding encoding;   /* of that server */
} LspCodeActionRequest;


static void
code_action_request_free (gpointer data)
{
    LspCodeActionRequest *request = (LspCodeActionRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);
    if (request->view)
        g_object_remove_weak_pointer (G_OBJECT (request->view),
                                      (gpointer*) &request->view);

    lsp_server_unref (request->server);
    g_free (request);
}


static void
code_action_reply (JsonNode   *result,
                   JsonObject *error,
                   gpointer    data)
{
    LspCodeActionRequest *request = (LspCodeActionRequest*) data;
    MooEditWindow *window = request->window;
    GSList *actions;

    if (!window || !MOO_IS_EDIT_WINDOW (window) ||
        !request->view || !MOO_IS_EDIT_VIEW (request->view))
        return;

    if (error)
    {
        const char *message = lsp_json_get_string (error, "message");

        moo_error_dialog (_("Code actions failed"),
                          message && message[0] ? message : NULL,
                          GTK_WIDGET (window));
        return;
    }

    actions = lsp_code_actions_parse (result);

    /*
     * Said out loud rather than passed over in silence: nothing on screen
     * would have changed either way, and a menu that did not appear is not
     * something to be left guessing about.
     */
    if (!actions)
    {
        moo_info_dialog (_("No code actions here"),
                         _("The server has nothing to offer at this place."),
                         GTK_WIDGET (window));
        return;
    }

    show_menu (window, request->view, request->server, request->encoding, actions);

    /* Each action belongs to the item that offers it now. */
    g_slist_free (actions);
}


/*
 * What the question is about: the selection when there is one, and the place
 * lsp_ask_position() found otherwise. A selection is the user having said
 * which lines they mean -- that is what an "extract this" refactoring is
 * asked with -- and an empty range at the cursor is every other case.
 */
static void
asked_range (MooEdit             *doc,
             LspPositionEncoding  encoding,
             int                  line,
             int                  character,
             int                 *start_line,
             int                 *start_character,
             int                 *end_line,
             int                 *end_character)
{
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextIter start, end;

    *start_line = *end_line = line;
    *start_character = *end_character = character;

    if (!buffer || !gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        return;

    lsp_iter_to_position (&start, encoding, start_line, start_character);
    lsp_iter_to_position (&end, encoding, end_line, end_character);
}


void
lsp_code_actions (MooEditWindow *window,
                  MooEditView   *view)
{
    LspDoc *ldoc = NULL;
    LspServer *server;
    LspCodeActionRequest *request;
    MooEditView *placement;
    JsonObject *params;
    JsonObject *context;
    GtkTextIter iter;
    LspPositionEncoding encoding;
    int line = 0, character = 0;
    int start_line, start_character, end_line, end_character;

    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));

    if (!lsp_can_ask (window, "textDocument/codeAction"))
        return;

    if (!lsp_ask_position (window, view, &ldoc, &iter, &line, &character))
        return;

    /*
     * Where the menu goes. The view the context menu was opened in when it was
     * one, and the view the document is shown in otherwise -- which is the
     * same view in every case but a split one.
     */
    placement = view ? view : moo_edit_window_get_active_view (window);

    if (!placement)
        return;

    server = lsp_doc_get_server (ldoc);
    encoding = lsp_server_get_position_encoding (server);

    asked_range (lsp_doc_get_doc (ldoc), encoding, line, character,
                 &start_line, &start_character, &end_line, &end_character);

    /* The server answers about the text it has, so it had better have this one. */
    lsp_doc_flush (ldoc);

    context = json_object_new ();
    lsp_json_set_array (context, "diagnostics",
                        _lsp_code_action_context (lsp_doc_get_raw_diagnostics (ldoc),
                                                  start_line, start_character,
                                                  end_line, end_character));
    /* CodeActionTriggerKind.Invoked: a person asked, rather than the editor. */
    lsp_json_set_int (context, "triggerKind", 1);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "range",
                         lsp_json_range (start_line, start_character,
                                         end_line, end_character));
    lsp_json_set_object (params, "context", context);

    request = g_new0 (LspCodeActionRequest, 1);
    request->window = window;
    request->view = placement;
    request->server = lsp_server_ref (server);
    request->encoding = encoding;
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &request->window);
    g_object_add_weak_pointer (G_OBJECT (placement), (gpointer*) &request->view);

    lsp_server_call (server, "textDocument/codeAction", params,
                     code_action_reply, request, code_action_request_free);
}
