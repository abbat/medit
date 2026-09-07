/*
 *   plugins/lsp/lsp-highlight.cpp
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

#include "plugins/lsp/lsp-highlight.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooutils/mooprefs.h"

#include <string.h>

/* Long enough that walking through a file with the arrow keys is one request
   per stop rather than one per line, short enough not to feel late. */
#define LSP_HIGHLIGHT_DELAY 250

/*
 * Fixed colours, as the diagnostics underlines are: medit's style schemes have
 * nothing that means "another use of this", and a colour taken from the theme
 * would have to be mixed with the background to be a highlight at all.
 */
#define LSP_HIGHLIGHT_COLOR_READ  "#d7e3f4"
#define LSP_HIGHLIGHT_COLOR_WRITE "#f4ded7"

/* There is one cursor, so there is one document wearing marks. */
static struct {
    MooEdit   *doc;             /* weak */
    guint      timeout;
    gint64     request;
    LspServer *server;
} marks;


/**********************************************************************/
/* The reply
 */

GSList *
lsp_highlight_parse (JsonNode *result)
{
    JsonArray *array;
    GSList *found = NULL;
    guint i, n;

    if (!result || !JSON_NODE_HOLDS_ARRAY (result))
        return NULL;

    array = json_node_get_array (result);
    n = json_array_get_length (array);

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        JsonObject *object;
        LspHighlight *highlight;
        int start_line = 0, start_character = 0;
        int end_line = 0, end_character = 0;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        object = json_node_get_object (node);

        if (!lsp_json_get_range (lsp_json_get_object (object, "range"),
                                 &start_line, &start_character,
                                 &end_line, &end_character))
            continue;

        highlight = g_new0 (LspHighlight, 1);
        highlight->start_line = start_line;
        highlight->start_character = start_character;
        highlight->end_line = end_line;
        highlight->end_character = end_character;
        highlight->kind = (int) lsp_json_get_int (object, "kind", 1);

        found = g_slist_prepend (found, highlight);
    }

    return g_slist_reverse (found);
}


void
lsp_highlight_free (GSList *highlights)
{
    g_slist_free_full (highlights, g_free);
}


/**********************************************************************/
/* The marks
 */

const char *
lsp_highlight_tag_name (int kind)
{
    /* 3 is Write; 1 (Text), 2 (Read) and anything a later version of the
       protocol may add are marked the same way. */
    return kind == 3 ? LSP_HIGHLIGHT_TAG_WRITE : LSP_HIGHLIGHT_TAG_READ;
}


static GtkTextTag *
get_tag (GtkTextBuffer *buffer,
         int            kind)
{
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
    const char *name = lsp_highlight_tag_name (kind);
    GtkTextTag *tag = gtk_text_tag_table_lookup (table, name);

    if (tag)
        return tag;

    return gtk_text_buffer_create_tag (buffer, name, "background",
                                       kind == 3 ? LSP_HIGHLIGHT_COLOR_WRITE
                                                 : LSP_HIGHLIGHT_COLOR_READ,
                                       (const char*) NULL);
}


static void
remove_tags (MooEdit *doc)
{
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
    static const char *names[] = { LSP_HIGHLIGHT_TAG_READ, LSP_HIGHLIGHT_TAG_WRITE };
    GtkTextIter start, end;
    guint i;

    gtk_text_buffer_get_bounds (buffer, &start, &end);

    for (i = 0; i < G_N_ELEMENTS (names); ++i)
    {
        GtkTextTag *tag = gtk_text_tag_table_lookup (table, names[i]);

        if (tag)
            gtk_text_buffer_remove_tag (buffer, tag, &start, &end);
    }
}


static void
forget_doc (void)
{
    if (marks.doc)
    {
        g_object_remove_weak_pointer (G_OBJECT (marks.doc), (gpointer*) &marks.doc);
        marks.doc = NULL;
    }
}


void
lsp_highlight_clear (void)
{
    if (marks.timeout)
        g_source_remove (marks.timeout);
    marks.timeout = 0;

    if (marks.request && marks.server)
        lsp_server_cancel (marks.server, marks.request);
    marks.request = 0;
    marks.server = NULL;

    if (marks.doc)
        remove_tags (marks.doc);

    forget_doc ();
}


void
lsp_highlight_apply (GtkTextBuffer       *buffer,
                     GSList              *highlights,
                     LspPositionEncoding  encoding)
{
    GSList *l;

    for (l = highlights; l != NULL; l = l->next)
    {
        LspHighlight *highlight = (LspHighlight*) l->data;
        GtkTextIter start, end;

        if (!lsp_position_to_iter (buffer, highlight->start_line,
                                   highlight->start_character, encoding, &start) ||
            !lsp_position_to_iter (buffer, highlight->end_line,
                                   highlight->end_character, encoding, &end))
            continue;

        gtk_text_buffer_apply_tag (buffer, get_tag (buffer, highlight->kind),
                                   &start, &end);
    }
}


/**********************************************************************/
/* Asking
 */

static void
highlight_reply (JsonNode   *result,
                 JsonObject *error,
                 gpointer    data)
{
    MooEdit *doc = (MooEdit*) data;
    LspDoc *ldoc;
    GSList *highlights;

    marks.request = 0;

    /*
     * The cursor has moved on if the document being marked is no longer the
     * one this was asked about -- the weak pointer takes care of the document
     * having been closed altogether.
     */
    if (error || !marks.doc || marks.doc != doc)
        return;

    ldoc = lsp_manager_lookup_doc (doc);

    if (!ldoc)
        return;

    highlights = lsp_highlight_parse (result);

    lsp_highlight_apply (moo_edit_get_buffer (doc), highlights,
                         lsp_server_get_position_encoding (lsp_doc_get_server (ldoc)));

    lsp_highlight_free (highlights);
}


static gboolean
ask (gpointer data)
{
    MooEdit *doc = (MooEdit*) data;
    LspDoc *ldoc = lsp_manager_lookup_doc (doc);
    LspServer *server = ldoc ? lsp_doc_get_server (ldoc) : NULL;
    GtkTextBuffer *buffer;
    GtkTextIter cursor;
    JsonObject *params;
    int line = 0, character = 0;

    marks.timeout = 0;

    if (!server)
        return G_SOURCE_REMOVE;

    /* The server must have the text the position refers to. */
    lsp_doc_flush (ldoc);

    buffer = moo_edit_get_buffer (doc);
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                      gtk_text_buffer_get_insert (buffer));

    lsp_iter_to_position (&cursor, lsp_server_get_position_encoding (server),
                          &line, &character);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "position", lsp_json_position (line, character));

    marks.server = server;
    marks.request = lsp_server_call (server, "textDocument/documentHighlight",
                                     params, highlight_reply, doc, NULL);

    return G_SOURCE_REMOVE;
}


void
lsp_highlight_cursor_moved (MooEdit *doc)
{
    LspDoc *ldoc;
    LspServer *server;

    g_return_if_fail (MOO_IS_EDIT (doc));

    /*
     * Off first: the marks describe where the cursor was, and a set of them
     * left up while the next answer is on its way points at the wrong words.
     */
    lsp_highlight_clear ();

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_HIGHLIGHT))
        return;

    ldoc = lsp_manager_lookup_doc (doc);
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "documentHighlightProvider"))
        return;

    marks.doc = doc;
    g_object_add_weak_pointer (G_OBJECT (doc), (gpointer*) &marks.doc);

    marks.timeout = g_timeout_add (LSP_HIGHLIGHT_DELAY, ask, doc);
}
