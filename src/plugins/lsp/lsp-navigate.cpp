/*
 *   plugins/lsp/lsp-navigate.cpp
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

#include "plugins/lsp/lsp-navigate.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooedit/mooeditor.h"
#include "mooedit/mootextview.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooutils-file.h"

#include <string.h>

/*
 * The reply to a hover request arrives long after the tooltip was asked for,
 * so the answer is cached and the tooltip asked again. One entry is enough:
 * the pointer is only ever in one place.
 */
static struct {
    MooEdit     *doc;           /* weak, cleared when the document goes */
    MooEditView *view;          /* weak, the one to ask again */
    int          line;
    int          character;
    char        *text;          /* NULL means "asked, nothing to say" */
    gboolean     have_answer;
    gint64       request;
    LspServer   *server;
} hover;


static void
forget_hover_target (void)
{
    if (hover.doc)
    {
        g_object_remove_weak_pointer (G_OBJECT (hover.doc), (gpointer*) &hover.doc);
        hover.doc = NULL;
    }

    if (hover.view)
    {
        g_object_remove_weak_pointer (G_OBJECT (hover.view), (gpointer*) &hover.view);
        hover.view = NULL;
    }
}


/*
 * Where the last right click on a view landed, in that document's own
 * coordinates. The context menu is opened by that very press, so by the time
 * one of its entries is used this is the place the user pointed at. Anything
 * else that moves the cursor clears it, so a menu opened from the keyboard is
 * answered about the cursor and not about an old click.
 */
static struct {
    MooEditView *view;          /* weak */
    int          line;
    int          character;
} click;

static void     forget_click        (void);


void
lsp_navigate_forget_click (void)
{
    forget_click ();
}


static void
forget_click (void)
{
    if (click.view)
    {
        g_object_remove_weak_pointer (G_OBJECT (click.view), (gpointer*) &click.view);
        click.view = NULL;
    }
}


void
lsp_navigate_note_click (MooEditView *view,
                         int          x,
                         int          y)
{
    GtkTextIter iter;
    int buffer_x = 0, buffer_y = 0;

    g_return_if_fail (MOO_IS_EDIT_VIEW (view));

    gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
                                           GTK_TEXT_WINDOW_WIDGET,
                                           x, y, &buffer_x, &buffer_y);
    gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter,
                                        buffer_x, buffer_y);

    forget_click ();

    click.view = view;
    click.line = gtk_text_iter_get_line (&iter);
    click.character = gtk_text_iter_get_line_offset (&iter);
    g_object_add_weak_pointer (G_OBJECT (view), (gpointer*) &click.view);
}


void
lsp_navigate_reset (void)
{
    forget_hover_target ();
    forget_click ();

    if (hover.request && hover.server)
        lsp_server_cancel (hover.server, hover.request);

    g_free (hover.text);
    hover.text = NULL;
    hover.have_answer = FALSE;
    hover.request = 0;
    hover.server = NULL;
}


/**********************************************************************/
/* Where a position is, and what the server should be asked about it
 */

static gboolean
get_cursor_position (MooEditWindow  *window,
                     LspDoc        **ldoc_out,
                     MooEdit       **doc_out,
                     int            *line,
                     int            *character)
{
    MooEdit *doc = moo_edit_window_get_active_doc (window);
    MooEditView *view = moo_edit_window_get_active_view (window);
    LspDoc *ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    GtkTextBuffer *buffer;
    GtkTextIter iter;

    if (!ldoc || !view)
        return FALSE;

    buffer = moo_edit_get_buffer (doc);
    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));

    lsp_iter_to_position (&iter,
                          lsp_server_get_position_encoding (lsp_doc_get_server (ldoc)),
                          line, character);

    if (ldoc_out)
        *ldoc_out = ldoc;
    if (doc_out)
        *doc_out = doc;

    return TRUE;
}


static JsonObject *
position_params (LspDoc *ldoc,
                 int     line,
                 int     character)
{
    JsonObject *params = json_object_new ();

    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "position", lsp_json_position (line, character));

    return params;
}


/* "textDocument/definition" -> "definitionProvider" */
static char *
provider_name (const char *method)
{
    const char *slash = strrchr (method, '/');

    return g_strdup_printf ("%sProvider", slash ? slash + 1 : method);
}


gboolean
lsp_can_goto (MooEditWindow *window,
              const char    *method)
{
    MooEdit *doc = moo_edit_window_get_active_doc (window);
    LspDoc *ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    LspServer *server = ldoc ? lsp_doc_get_server (ldoc) : NULL;
    char *provider;
    gboolean can;

    if (!server || !lsp_server_is_ready (server))
        return FALSE;

    provider = provider_name (method);
    can = lsp_server_has_provider (server, provider);
    g_free (provider);

    return can;
}


/**********************************************************************/
/* Going to a definition
 */

typedef struct {
    MooEditWindow      *window;     /* weak */
    LspPositionEncoding encoding;   /* of the server that was asked */
} LspGotoRequest;

/* Where to put the cursor once medit has finished opening the document. */
typedef struct {
    MooEditView *view;          /* weak */
    int          line;
    int          character;
} LspGotoPlace;


static gboolean
place_cursor (gpointer data)
{
    LspGotoPlace *place = (LspGotoPlace*) data;

    if (place->view)
        moo_text_view_move_cursor (MOO_TEXT_VIEW (place->view),
                                   place->line, place->character, FALSE, FALSE);

    return G_SOURCE_REMOVE;
}


static void
place_free (gpointer data)
{
    LspGotoPlace *place = (LspGotoPlace*) data;

    if (place->view)
        g_object_remove_weak_pointer (G_OBJECT (place->view), (gpointer*) &place->view);

    g_free (place);
}


/*
 * moo_editor_open_file() queues a move to (line, 0) at G_PRIORITY_HIGH_IDLE+9,
 * and do_move_cursor() removes whatever is in view->priv->move_cursor_idle
 * when it runs -- which is another move queued the same way, so asking for
 * one here would only get it cancelled. Hence a plain idle of a lower
 * priority, which runs after theirs and which nothing else touches.
 */
static void
place_cursor_later (MooEditView *view,
                    int          line,
                    int          character)
{
    LspGotoPlace *place = g_new0 (LspGotoPlace, 1);

    place->view = view;
    place->line = line;
    place->character = character;
    g_object_add_weak_pointer (G_OBJECT (view), (gpointer*) &place->view);

    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, place_cursor, place, place_free);
}


static void
goto_request_free (gpointer data)
{
    LspGotoRequest *request = (LspGotoRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);

    g_free (request);
}


/*
 * The reply is a Location, an array of them, or an array of LocationLink,
 * depending on the server and on what it found. Only the first is used: going
 * somewhere is the point, and a list of candidates needs a pane of its own.
 */
static gboolean
first_location (JsonNode  *result,
                char     **uri,
                int       *line,
                int       *character)
{
    JsonObject *object = NULL;
    JsonObject *range;
    const char *target;

    if (!result)
        return FALSE;

    if (JSON_NODE_HOLDS_OBJECT (result))
    {
        object = json_node_get_object (result);
    }
    else if (JSON_NODE_HOLDS_ARRAY (result))
    {
        JsonArray *array = json_node_get_array (result);

        if (json_array_get_length (array) == 0)
            return FALSE;

        {
            JsonNode *first = json_array_get_element (array, 0);

            if (!first || !JSON_NODE_HOLDS_OBJECT (first))
                return FALSE;

            object = json_node_get_object (first);
        }
    }

    if (!object)
        return FALSE;

    /* A LocationLink names its target differently from a Location. */
    if (lsp_json_has (object, "targetUri"))
    {
        target = lsp_json_get_string (object, "targetUri");
        range = lsp_json_get_object (object, "targetSelectionRange");

        if (!range)
            range = lsp_json_get_object (object, "targetRange");
    }
    else
    {
        target = lsp_json_get_string (object, "uri");
        range = lsp_json_get_object (object, "range");
    }

    if (!target || !range)
        return FALSE;

    *uri = g_strdup (target);

    return lsp_json_get_position (lsp_json_get_object (range, "start"),
                                  line, character);
}


static void
goto_reply (JsonNode   *result,
            JsonObject *error,
            gpointer    data)
{
    LspGotoRequest *request = (LspGotoRequest*) data;
    MooEditWindow *window = request->window;
    MooEditor *editor;
    MooEdit *doc;
    char *uri = NULL;
    char *path;
    GFile *file;
    int line = 0, character = 0;

    if (error || !window || !MOO_IS_EDIT_WINDOW (window))
        return;

    if (!first_location (result, &uri, &line, &character))
        return;

    file = g_file_new_for_uri (uri);
    path = g_file_get_path (file);
    moo_file_free (file);
    g_free (uri);

    if (!path)
        return;

    /*
     * The line is opened first and the column applied afterwards:
     * moo_editor_open_path() takes a line only, and the character has to be
     * converted against the buffer of whatever document it ends up in.
     */
    editor = moo_edit_window_get_editor (window);
    doc = moo_editor_open_path (editor, path, NULL, line, window);

    g_free (path);

    if (!doc)
        return;

    /*
     * The encoding comes from the request rather than from the document that
     * was opened: a document only just added to a window has no LspDoc yet,
     * because its language is settled after it is inserted and the plugin
     * attaches on notify::lang. The server that answered is the one whose
     * coordinates these are anyway.
     */
    if (character > 0)
    {
        MooEditView *view = moo_edit_window_get_active_view (window);
        GtkTextIter iter;

        if (view && lsp_position_to_iter (moo_edit_get_buffer (doc), line, character,
                                          request->encoding, &iter))
            place_cursor_later (view, gtk_text_iter_get_line (&iter),
                                gtk_text_iter_get_line_offset (&iter));
    }
}


static void
ask_where (MooEditWindow *window,
           LspDoc        *ldoc,
           const char    *method,
           int            line,
           int            character)
{
    LspGotoRequest *request;

    /* The server has to have the text the position refers to. */
    lsp_doc_flush (ldoc);

    request = g_new0 (LspGotoRequest, 1);
    request->window = window;
    request->encoding = lsp_server_get_position_encoding (lsp_doc_get_server (ldoc));
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &request->window);

    lsp_server_call (lsp_doc_get_server (ldoc), method,
                     position_params (ldoc, line, character),
                     goto_reply, request, goto_request_free);
}


void
lsp_goto_definition (MooEditWindow *window,
                     const char    *method)
{
    LspDoc *ldoc = NULL;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));
    g_return_if_fail (method != NULL);

    if (!lsp_can_goto (window, method))
        return;

    if (!get_cursor_position (window, &ldoc, NULL, &line, &character))
        return;

    ask_where (window, ldoc, method, line, character);
}


void
lsp_goto_definition_at_click (MooEditView *view,
                              const char  *method)
{
    MooEditWindow *window;
    MooEdit *doc;
    LspDoc *ldoc;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_VIEW (view));
    g_return_if_fail (method != NULL);

    window = moo_edit_view_get_window (view);

    if (!window || !lsp_can_goto (window, method))
        return;

    /*
     * Without a press to go by -- the menu was opened with the keyboard --
     * the cursor is the right thing to ask about.
     */
    if (click.view != view)
    {
        lsp_goto_definition (window, method);
        return;
    }

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;

    if (!ldoc)
        return;

    buffer = moo_edit_get_buffer (doc);
    gtk_text_buffer_get_iter_at_line (buffer, &iter, click.line);

    if (click.character > 0)
    {
        int chars = gtk_text_iter_get_chars_in_line (&iter);

        gtk_text_iter_set_line_offset (&iter, MIN (click.character, MAX (chars - 1, 0)));
    }

    lsp_iter_to_position (&iter,
                          lsp_server_get_position_encoding (lsp_doc_get_server (ldoc)),
                          &line, &character);

    ask_where (window, ldoc, method, line, character);
}


/**********************************************************************/
/* Hover
 */

/*
 * MarkupContent, MarkedString, or an array of MarkedString, all of which may
 * carry the text in one of two members.
 */
static char *
hover_text (JsonNode *result)
{
    JsonObject *object;
    JsonNode *contents;

    if (!result || !JSON_NODE_HOLDS_OBJECT (result))
        return NULL;

    object = json_node_get_object (result);
    contents = lsp_json_get_node (object, "contents");

    if (!contents)
        return NULL;

    if (JSON_NODE_HOLDS_VALUE (contents) &&
        json_node_get_value_type (contents) == G_TYPE_STRING)
            return g_strdup (json_node_get_string (contents));

    if (JSON_NODE_HOLDS_OBJECT (contents))
    {
        JsonObject *content = json_node_get_object (contents);
        const char *value = lsp_json_get_string (content, "value");

        return value ? g_strdup (value) : NULL;
    }

    if (JSON_NODE_HOLDS_ARRAY (contents))
    {
        JsonArray *array = json_node_get_array (contents);
        GString *text = g_string_new (NULL);
        guint i, n = json_array_get_length (array);

        for (i = 0; i < n; ++i)
        {
            JsonNode *item = json_array_get_element (array, i);
            const char *value = NULL;

            if (!item)
                continue;

            if (JSON_NODE_HOLDS_VALUE (item) &&
                json_node_get_value_type (item) == G_TYPE_STRING)
                    value = json_node_get_string (item);
            else if (JSON_NODE_HOLDS_OBJECT (item))
                value = lsp_json_get_string (json_node_get_object (item), "value");

            if (value && value[0])
            {
                if (text->len)
                    g_string_append_c (text, '\n');
                g_string_append (text, value);
            }
        }

        if (!text->len)
        {
            g_string_free (text, TRUE);
            return NULL;
        }

        return g_string_free (text, FALSE);
    }

    return NULL;
}


static void
hover_reply (JsonNode                 *result,
             JsonObject               *error,
             G_GNUC_UNUSED gpointer    data)
{
    hover.request = 0;
    hover.have_answer = TRUE;

    g_free (hover.text);
    hover.text = error ? NULL : hover_text (result);

    /*
     * Ask the tooltip again, now that there is something to answer with. The
     * view is held with a weak pointer because the reply can outlive the
     * window it was asked from.
     */
    if (hover.text && hover.view)
        gtk_widget_trigger_tooltip_query (GTK_WIDGET (hover.view));
}


gboolean
lsp_hover_query_tooltip (MooEditView *view,
                         int          x,
                         int          y,
                         gboolean     keyboard_mode,
                         GtkTooltip  *tooltip)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    int line = 0, character = 0;
    int buffer_x = 0, buffer_y = 0;

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_HOVER))
        return FALSE;

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "hoverProvider"))
        return FALSE;

    buffer = moo_edit_get_buffer (doc);

    if (keyboard_mode)
    {
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
    }
    else
    {
        gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
                                               GTK_TEXT_WINDOW_WIDGET,
                                               x, y, &buffer_x, &buffer_y);
        gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter,
                                            buffer_x, buffer_y);
    }

    lsp_iter_to_position (&iter, lsp_server_get_position_encoding (server),
                          &line, &character);

    if (hover.doc == doc && hover.have_answer &&
        hover.line == line && hover.character == character)
    {
        if (!hover.text)
            return FALSE;

        gtk_tooltip_set_text (tooltip, hover.text);
        return TRUE;
    }

    /* A different position: throw the answer away and ask about this one. */
    if (hover.request)
        lsp_server_cancel (hover.server, hover.request);

    forget_hover_target ();

    g_free (hover.text);
    hover.text = NULL;
    hover.have_answer = FALSE;
    hover.doc = doc;
    hover.view = view;
    hover.line = line;
    hover.character = character;
    hover.server = server;
    g_object_add_weak_pointer (G_OBJECT (doc), (gpointer*) &hover.doc);
    g_object_add_weak_pointer (G_OBJECT (view), (gpointer*) &hover.view);

    hover.request = lsp_server_call (server, "textDocument/hover",
                                     position_params (ldoc, line, character),
                                     hover_reply, NULL, NULL);

    return FALSE;
}
