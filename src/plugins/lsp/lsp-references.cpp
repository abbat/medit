/*
 *   plugins/lsp/lsp-references.cpp
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

#include "plugins/lsp/lsp-references.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-navigate.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooedit/mooeditor.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooutils-file.h"

#include <string.h>


/**********************************************************************/
/* Reading a list of places out of a reply
 */

static LspLocation *
location_new (JsonObject          *object,
              LspPositionEncoding  encoding)
{
    LspLocation *location;
    JsonObject *range;
    const char *uri;
    char *path;
    int line = 0, character = 0;

    /*
     * A LocationLink names its target differently from a Location, and has
     * two ranges: the whole definition and the name inside it. The name is
     * the place worth pointing at.
     */
    if (lsp_json_has (object, "targetUri"))
    {
        uri = lsp_json_get_string (object, "targetUri");
        range = lsp_json_get_object (object, "targetSelectionRange");

        if (!range)
            range = lsp_json_get_object (object, "targetRange");
    }
    else
    {
        uri = lsp_json_get_string (object, "uri");
        range = lsp_json_get_object (object, "range");
    }

    if (!range || !lsp_json_get_position (lsp_json_get_object (range, "start"),
                                          &line, &character))
        return NULL;

    path = lsp_path_from_uri (uri);

    if (!path)
        return NULL;

    location = g_new0 (LspLocation, 1);
    location->path = path;
    location->line = line;
    location->character = character;
    location->encoding = encoding;

    return location;
}


GSList *
lsp_locations_parse (JsonNode            *result,
                     LspPositionEncoding  encoding)
{
    GSList *found = NULL;
    JsonArray *array;
    guint i, n;

    if (!result)
        return NULL;

    /* One place, which is how most servers answer a question about a
       definition, and a list of them, which is every other answer. */
    if (JSON_NODE_HOLDS_OBJECT (result))
    {
        LspLocation *one = location_new (json_node_get_object (result), encoding);

        return one ? g_slist_prepend (NULL, one) : NULL;
    }

    if (!JSON_NODE_HOLDS_ARRAY (result))
        return NULL;

    array = json_node_get_array (result);
    n = json_array_get_length (array);

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        LspLocation *location;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        location = location_new (json_node_get_object (node), encoding);

        if (location)
            found = g_slist_prepend (found, location);
    }

    return g_slist_reverse (found);
}


void
lsp_location_free (LspLocation *location)
{
    if (!location)
        return;

    g_free (location->path);
    g_free (location->display);
    g_free (location->text);
    g_free (location);
}


void
lsp_locations_free (GSList *locations)
{
    g_slist_free_full (locations, (GDestroyNotify) lsp_location_free);
}


/**********************************************************************/
/* Turning them into lines somebody can read
 */

/*
 * The text of a file, as a buffer to resolve a position against. From the
 * open document when there is one -- what is on screen is what the user means,
 * even where it differs from the disk -- and from the disk when there is not.
 *
 * NULL for a file that cannot be read or is not text: a use in it is still
 * listed, only without the line it is on.
 */
static GtkTextBuffer *
buffer_of_file (const char *path)
{
    GFile *file = g_file_new_for_path (path);
    MooEdit *doc = moo_editor_get_doc_for_file (moo_editor_instance (), file);
    GtkTextBuffer *buffer = NULL;
    char *contents = NULL;
    gsize len = 0;

    moo_file_free (file);

    if (doc)
        return GTK_TEXT_BUFFER (g_object_ref (moo_edit_get_buffer (doc)));

    if (!g_file_get_contents (path, &contents, &len, NULL))
        return NULL;

    if (g_utf8_validate (contents, len, NULL))
    {
        buffer = gtk_text_buffer_new (NULL);
        gtk_text_buffer_set_text (buffer, contents, (int) len);
    }

    g_free (contents);

    return buffer;
}


/* The path as the pane shows it: relative to the root of the project the
   server was started for, which is what makes a list of them readable. */
static char *
display_path (const char *path,
              const char *root)
{
    GFile *root_file;
    GFile *file;
    char *relative = NULL;

    if (root && root[0])
    {
        root_file = g_file_new_for_path (root);
        file = g_file_new_for_path (path);

        relative = g_file_get_relative_path (root_file, file);

        moo_file_free (root_file);
        moo_file_free (file);
    }

    return relative ? relative : g_strdup (path);
}


static char *
line_of_buffer (GtkTextBuffer     *buffer,
                const GtkTextIter *where)
{
    GtkTextIter start = *where;
    GtkTextIter end = *where;
    char *text;

    gtk_text_iter_set_line_offset (&start, 0);

    if (!gtk_text_iter_ends_line (&end))
        gtk_text_iter_forward_to_line_end (&end);

    text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

    return g_strstrip (text);
}


void
lsp_locations_describe (GSList     *locations,
                        const char *root)
{
    /* One buffer per file rather than one per use: a file with a dozen uses
       in it would otherwise be read a dozen times. */
    GHashTable *buffers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, g_object_unref);
    GSList *l;

    for (l = locations; l != NULL; l = l->next)
    {
        LspLocation *location = (LspLocation*) l->data;
        GtkTextBuffer *buffer;
        GtkTextIter iter;
        char *name;

        buffer = (GtkTextBuffer*) g_hash_table_lookup (buffers, location->path);

        if (!buffer)
        {
            buffer = buffer_of_file (location->path);

            if (buffer)
                g_hash_table_insert (buffers, g_strdup (location->path), buffer);
        }

        name = display_path (location->path, root);

        g_free (location->display);
        g_free (location->text);
        location->text = NULL;

        /*
         * The character the pane shows is resolved against the text, because
         * the server counted it in UTF-16 code units and nothing else in medit
         * does. A file that could not be read leaves it in the server's
         * counting -- and says nothing about the line, having none to say.
         */
        if (buffer && lsp_position_to_iter (buffer, location->line, location->character,
                                            location->encoding, &iter))
        {
            location->display = g_strdup_printf ("%s:%d:%d", name,
                                                 gtk_text_iter_get_line (&iter) + 1,
                                                 gtk_text_iter_get_line_offset (&iter) + 1);
            location->text = line_of_buffer (buffer, &iter);
        }
        else
        {
            location->display = g_strdup_printf ("%s:%d:%d", name,
                                                 location->line + 1,
                                                 location->character + 1);
        }

        g_free (name);
    }

    g_hash_table_destroy (buffers);
}


/**********************************************************************/
/* Asking
 */

typedef struct {
    MooEditWindow      *window;     /* weak */
    LspPositionEncoding encoding;   /* of the server that was asked */
    char               *root;       /* the paths in the pane are relative to it */
} LspReferencesRequest;


static void
references_request_free (gpointer data)
{
    LspReferencesRequest *request = (LspReferencesRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);

    g_free (request->root);
    g_free (request);
}


static void
references_reply (JsonNode   *result,
                  JsonObject *error,
                  gpointer    data)
{
    LspReferencesRequest *request = (LspReferencesRequest*) data;
    MooEditWindow *window = request->window;
    GSList *found;

    /* The window can be gone: a server may take its time, and the reply
       outlives what asked for it. */
    if (!window || !MOO_IS_EDIT_WINDOW (window))
        return;

    if (error)
    {
        const char *message = lsp_json_get_string (error, "message");

        _moo_lsp_show_references (window, NULL,
                                  message && message[0] ? message
                                                        : _("The server refused the question"));
        return;
    }

    found = lsp_locations_parse (result, request->encoding);
    lsp_locations_describe (found, request->root);

    /*
     * An empty answer is shown as a sentence rather than as an empty pane: a
     * pane with nothing in it is what a question that was never asked looks
     * like, and the two are worth telling apart.
     */
    _moo_lsp_show_references (window, found, _("No references found"));
}


void
lsp_find_references (MooEditWindow *window,
                     MooEditView   *view)
{
    LspDoc *ldoc = NULL;
    LspServer *server;
    LspReferencesRequest *request;
    JsonObject *params;
    JsonObject *context;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));

    if (!lsp_can_ask (window, "textDocument/references"))
        return;

    if (!lsp_ask_position (window, view, &ldoc, NULL, &line, &character))
        return;

    server = lsp_doc_get_server (ldoc);

    /* The server has to have the text the position refers to. */
    lsp_doc_flush (ldoc);

    params = lsp_position_params (ldoc, line, character);
    context = json_object_new ();

    /*
     * The declaration counts as a use of the name. Leaving it out would put a
     * hole in the list exactly where the thing being looked for is defined,
     * and the answer is what a jump to the definition would have given anyway.
     */
    lsp_json_set_bool (context, "includeDeclaration", TRUE);
    lsp_json_set_object (params, "context", context);

    request = g_new0 (LspReferencesRequest, 1);
    request->window = window;
    request->encoding = lsp_server_get_position_encoding (server);
    request->root = g_strdup (lsp_server_get_root (server));
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &request->window);

    lsp_server_call (server, "textDocument/references", params,
                     references_reply, request, references_request_free);
}
