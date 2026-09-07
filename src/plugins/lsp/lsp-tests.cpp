/*
 *   plugins/lsp/lsp-tests.cpp
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
 * What the UI tests cannot say anything about.
 *
 * The protocol counts a character in UTF-16 code units and medit counts it in
 * characters, so every position that crosses that boundary is arithmetic --
 * and every UI test in tests/lsp is written in ASCII, where all three
 * encodings agree and the arithmetic cannot be wrong. The cases that matter
 * are an emoji, which is one character and two UTF-16 units, and Cyrillic,
 * which is one character and two UTF-8 bytes.
 *
 * The other half is the shape of a reply. documentSymbol comes back as a flat
 * SymbolInformation list from older servers and a nested DocumentSymbol tree
 * from newer ones; a UI test can drive one of those per run, and both are here
 * in a millisecond.
 *
 * No display is needed for any of it: GtkTextBuffer and GtkTreeStore are
 * objects rather than widgets.
 */

#include "plugins/lsp/lsp-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "plugins/lsp/lsp-doc.h"
#include "plugins/lsp/lsp-json.h"
#include "plugins/lsp/lsp-symbols.h"

#include <gtk/gtk.h>
#include <string.h>

/* "alpha ", an emoji, " beta" -- the emoji is one character and two UTF-16
   code units, which is the whole point of it. */
#define EMOJI_LINE "alpha \xf0\x9f\x98\x80 beta\ngamma\n"

/* Cyrillic: one character, two UTF-8 bytes each. */
#define CYRILLIC_LINE "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 \xd0\xbc\xd0\xb8\xd1\x80\n"


static GtkTextBuffer *
buffer_with (const char *text)
{
    GtkTextBuffer *buffer = gtk_text_buffer_new (NULL);

    gtk_text_buffer_set_text (buffer, text, -1);

    return buffer;
}


/* Where a position lands, as the character offset medit counts in. */
static int
offset_of (GtkTextBuffer       *buffer,
           int                  line,
           int                  character,
           LspPositionEncoding  encoding)
{
    GtkTextIter iter;

    lsp_position_to_iter (buffer, line, character, encoding, &iter);

    return gtk_text_iter_get_line_offset (&iter);
}


static void
test_position_utf16 (void)
{
    GtkTextBuffer *buffer = buffer_with (EMOJI_LINE);

    g_assert_cmpint (offset_of (buffer, 0, 0, LSP_POSITION_ENCODING_UTF16), ==, 0);

    /* The emoji itself: six characters before it, six units before it. */
    g_assert_cmpint (offset_of (buffer, 0, 6, LSP_POSITION_ENCODING_UTF16), ==, 6);

    /* And after it the two counts part company: the space that follows is the
       eighth UTF-16 unit and the seventh character. */
    g_assert_cmpint (offset_of (buffer, 0, 8, LSP_POSITION_ENCODING_UTF16), ==, 7);
    g_assert_cmpint (offset_of (buffer, 0, 9, LSP_POSITION_ENCODING_UTF16), ==, 8);

    g_assert_cmpint (offset_of (buffer, 1, 3, LSP_POSITION_ENCODING_UTF16), ==, 3);

    g_object_unref (buffer);
}


static void
test_position_utf8 (void)
{
    GtkTextBuffer *buffer = buffer_with (CYRILLIC_LINE);

    /* "привет" is six characters and twelve bytes, so the space after it is
       byte 12 and character 6, and the word after that starts at byte 13. */
    g_assert_cmpint (offset_of (buffer, 0, 12, LSP_POSITION_ENCODING_UTF8), ==, 6);
    g_assert_cmpint (offset_of (buffer, 0, 13, LSP_POSITION_ENCODING_UTF8), ==, 7);

    g_object_unref (buffer);
}


static void
test_position_utf32 (void)
{
    GtkTextBuffer *buffer = buffer_with (EMOJI_LINE);

    /* A code point is a character, so the emoji costs one of each and the
       counts never part company. */
    g_assert_cmpint (offset_of (buffer, 0, 7, LSP_POSITION_ENCODING_UTF32), ==, 7);
    g_assert_cmpint (offset_of (buffer, 0, 8, LSP_POSITION_ENCODING_UTF32), ==, 8);

    g_object_unref (buffer);
}


static void
test_position_out_of_range (void)
{
    GtkTextBuffer *buffer = buffer_with (EMOJI_LINE);
    GtkTextIter iter;

    /* A line the document does not have: the end of it, and said so. */
    g_assert_false (lsp_position_to_iter (buffer, 99, 0,
                                          LSP_POSITION_ENCODING_UTF16, &iter));
    g_assert_true (gtk_text_iter_is_end (&iter));

    /* A character past the end of the line is the end of that line, not the
       start of the next one. */
    g_assert_true (lsp_position_to_iter (buffer, 1, 99,
                                         LSP_POSITION_ENCODING_UTF16, &iter));
    g_assert_cmpint (gtk_text_iter_get_line (&iter), ==, 1);
    g_assert_true (gtk_text_iter_ends_line (&iter));

    /* Negative is nowhere, and nowhere is the start. */
    g_assert_cmpint (offset_of (buffer, 0, -5, LSP_POSITION_ENCODING_UTF16), ==, 0);
    g_assert_cmpint (offset_of (buffer, -1, 2, LSP_POSITION_ENCODING_UTF16), ==, 2);

    g_object_unref (buffer);
}


/* -------------------------------------------------------------------------
 * documentSymbol, in both of the shapes a server may answer with
 */

typedef struct {
    char *markup;
    char *name;
    int   line;
    int   character;
    int   depth;
} SymbolRow;


static void
collect_rows (GtkTreeModel *model,
              GtkTreeIter  *parent,
              int           depth,
              GArray       *rows)
{
    GtkTreeIter iter;
    gboolean ok;

    for (ok = gtk_tree_model_iter_children (model, &iter, parent);
         ok;
         ok = gtk_tree_model_iter_next (model, &iter))
    {
        SymbolRow row;

        row.depth = depth;
        gtk_tree_model_get (model, &iter,
                            LSP_SYMBOL_COLUMN_MARKUP, &row.markup,
                            LSP_SYMBOL_COLUMN_NAME, &row.name,
                            LSP_SYMBOL_COLUMN_LINE, &row.line,
                            LSP_SYMBOL_COLUMN_CHARACTER, &row.character,
                            -1);

        g_array_append_val (rows, row);

        collect_rows (model, &iter, depth + 1, rows);
    }
}


/* The store filled from one reply, flattened depth-first for comparison. */
static GArray *
symbols_of (const char          *json,
            const char          *text,
            LspPositionEncoding  encoding)
{
    GtkTreeStore *store = lsp_symbols_new_store ();
    GtkTextBuffer *buffer = buffer_with (text);
    GArray *rows = g_array_new (FALSE, TRUE, sizeof (SymbolRow));
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);

    g_assert_no_error (error);
    g_assert_nonnull (node);

    lsp_symbols_fill (store, node, buffer, encoding);
    collect_rows (GTK_TREE_MODEL (store), NULL, 0, rows);

    json_node_unref (node);
    g_object_unref (buffer);
    g_object_unref (store);

    return rows;
}


static void
free_rows (GArray *rows)
{
    guint i;

    for (i = 0; i < rows->len; ++i)
    {
        SymbolRow *row = &g_array_index (rows, SymbolRow, i);

        g_free (row->markup);
        g_free (row->name);
    }

    g_array_free (rows, TRUE);
}


static void
test_symbols_nested (void)
{
    static const char *reply =
        "[{\"name\": \"alpha\", \"kind\": 12,"
        "  \"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "              \"end\": {\"line\": 0, \"character\": 5}},"
        "  \"selectionRange\": {\"start\": {\"line\": 0, \"character\": 0},"
        "                       \"end\": {\"line\": 0, \"character\": 5}},"
        "  \"children\": [{\"name\": \"beta\", \"kind\": 13, \"detail\": \"int\","
        "    \"range\": {\"start\": {\"line\": 1, \"character\": 0},"
        "                \"end\": {\"line\": 1, \"character\": 4}},"
        "    \"selectionRange\": {\"start\": {\"line\": 1, \"character\": 2},"
        "                         \"end\": {\"line\": 1, \"character\": 4}}}]}]";

    GArray *rows = symbols_of (reply, "alpha beta\ngamma\n",
                               LSP_POSITION_ENCODING_UTF16);
    SymbolRow *first, *child;

    g_assert_cmpuint (rows->len, ==, 2);

    first = &g_array_index (rows, SymbolRow, 0);
    child = &g_array_index (rows, SymbolRow, 1);

    g_assert_cmpstr (first->name, ==, "alpha");
    g_assert_cmpint (first->depth, ==, 0);
    g_assert_cmpint (first->line, ==, 0);
    g_assert_cmpint (first->character, ==, 0);

    /* No detail, so the kind is what the row shows beside the name. Compared
       against lsp_symbol_kind_name() rather than against the English of it:
       the name is translated, and a test that spelled it out would pass in
       the C locale and fail on the developer's machine. */
    g_assert_nonnull (strstr (first->markup, "alpha"));
    g_assert_nonnull (strstr (first->markup, lsp_symbol_kind_name (12)));

    /* The child is a child, and it is the selectionRange -- the name itself --
       that says where the cursor goes, not the range of the whole symbol. */
    g_assert_cmpstr (child->name, ==, "beta");
    g_assert_cmpint (child->depth, ==, 1);
    g_assert_cmpint (child->line, ==, 1);
    g_assert_cmpint (child->character, ==, 2);

    /* A detail replaces the kind rather than joining it. */
    g_assert_nonnull (strstr (child->markup, "int"));
    g_assert_null (strstr (child->markup, lsp_symbol_kind_name (13)));

    free_rows (rows);
}


static void
test_symbols_flat (void)
{
    /* SymbolInformation: the range lives inside a location, there is no
       selectionRange, and there are no children. This is what older servers
       answer with, and it is the shape no UI test drives. */
    static const char *reply =
        "[{\"name\": \"alpha\", \"kind\": 12,"
        "  \"location\": {\"uri\": \"file:///x\","
        "                 \"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "                             \"end\": {\"line\": 0, \"character\": 5}}}},"
        " {\"name\": \"gamma\", \"kind\": 5,"
        "  \"location\": {\"uri\": \"file:///x\","
        "                 \"range\": {\"start\": {\"line\": 1, \"character\": 2},"
        "                             \"end\": {\"line\": 1, \"character\": 5}}}}]";

    GArray *rows = symbols_of (reply, "alpha beta\ngamma\n",
                               LSP_POSITION_ENCODING_UTF16);

    g_assert_cmpuint (rows->len, ==, 2);

    g_assert_cmpstr (g_array_index (rows, SymbolRow, 0).name, ==, "alpha");
    g_assert_cmpint (g_array_index (rows, SymbolRow, 0).depth, ==, 0);
    g_assert_cmpint (g_array_index (rows, SymbolRow, 1).line, ==, 1);
    g_assert_cmpint (g_array_index (rows, SymbolRow, 1).character, ==, 2);
    g_assert_cmpint (g_array_index (rows, SymbolRow, 1).depth, ==, 0);

    free_rows (rows);
}


static void
test_symbols_positions (void)
{
    /* The same crossing as the position tests, arriving through a reply: the
       server counts the emoji as two, the store holds where the cursor goes. */
    static const char *reply =
        "[{\"name\": \"beta\", \"kind\": 13,"
        "  \"range\": {\"start\": {\"line\": 0, \"character\": 9},"
        "              \"end\": {\"line\": 0, \"character\": 13}},"
        "  \"selectionRange\": {\"start\": {\"line\": 0, \"character\": 9},"
        "                       \"end\": {\"line\": 0, \"character\": 13}}}]";

    GArray *rows = symbols_of (reply, EMOJI_LINE, LSP_POSITION_ENCODING_UTF16);

    g_assert_cmpuint (rows->len, ==, 1);
    g_assert_cmpint (g_array_index (rows, SymbolRow, 0).character, ==, 8);

    free_rows (rows);
}


static void
test_symbols_empty (void)
{
    /* A server with nothing to say answers with an empty array, and one that
       does not support the request answers null; neither is a reason to put a
       row in the tree. */
    GArray *rows = symbols_of ("[]", "alpha\n", LSP_POSITION_ENCODING_UTF16);

    g_assert_cmpuint (rows->len, ==, 0);
    free_rows (rows);

    rows = symbols_of ("null", "alpha\n", LSP_POSITION_ENCODING_UTF16);

    g_assert_cmpuint (rows->len, ==, 0);
    free_rows (rows);
}


void
_moo_lsp_add_unit_tests (void)
{
    g_test_add_func ("/lsp/position/utf16", test_position_utf16);
    g_test_add_func ("/lsp/position/utf8", test_position_utf8);
    g_test_add_func ("/lsp/position/utf32", test_position_utf32);
    g_test_add_func ("/lsp/position/out-of-range", test_position_out_of_range);

    g_test_add_func ("/lsp/symbols/nested", test_symbols_nested);
    g_test_add_func ("/lsp/symbols/flat", test_symbols_flat);
    g_test_add_func ("/lsp/symbols/positions", test_symbols_positions);
    g_test_add_func ("/lsp/symbols/empty", test_symbols_empty);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
