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
 * from newer ones; a place is a Location from one server and a LocationLink
 * from another; a rename is a WorkspaceEdit written as "changes" or as
 * "documentChanges", and its edits have to come out in the order they can be
 * applied in. A UI test can drive one shape per run, and all of them are here
 * in a millisecond.
 *
 * No display is needed for any of it: GtkTextBuffer and GtkTreeStore are
 * objects rather than widgets.
 */

#include "plugins/lsp/lsp-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "plugins/lsp/lsp-completion.h"
#include "plugins/lsp/lsp-config.h"
#include "plugins/lsp/lsp-client.h"
#include "plugins/lsp/lsp-diagnostics.h"
#include "plugins/lsp/lsp-doc.h"
#include "plugins/lsp/lsp-highlight.h"
#include "plugins/lsp/lsp-json.h"
#include "plugins/lsp/lsp-navigate.h"
#include "plugins/lsp/lsp-references.h"
#include "plugins/lsp/lsp-edits.h"
#include "plugins/lsp/lsp-signature.h"
#include "plugins/lsp/lsp-symbols.h"
#include "mooutils/moomarkup.h"
#include "mooutils/mooi18n.h"


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
test_position_to_server (void)
{
    GtkTextBuffer *buffer = buffer_with (EMOJI_LINE);
    GtkTextIter iter;
    int line, character;

    /* The space after the emoji is character 7, UTF-8 byte 10, and UTF-16
       unit 8. Test the direction used when sending edits and diagnostics. */
    gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 7);

    lsp_iter_to_position (&iter, LSP_POSITION_ENCODING_UTF32, &line, &character);
    g_assert_cmpint (line, ==, 0);
    g_assert_cmpint (character, ==, 7);

    lsp_iter_to_position (&iter, LSP_POSITION_ENCODING_UTF8, &line, &character);
    g_assert_cmpint (line, ==, 0);
    g_assert_cmpint (character, ==, 10);

    lsp_iter_to_position (&iter, LSP_POSITION_ENCODING_UTF16, &line, &character);
    g_assert_cmpint (line, ==, 0);
    g_assert_cmpint (character, ==, 8);

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


static void
test_json_malformed (void)
{
    GError *error = NULL;
    JsonNode *node;
    JsonObject *object;
    int line, character;

    node = lsp_json_parse ("{\"string\":\"ok\",\"number\":3,"
                           "\"boolean\":true,\"null\":null,"
                           "\"object\":{},\"array\":[],"
                           "\"nested\":{\"value\":7},"
                           "\"provider-object\":{},"
                           "\"provider-false\":false}", -1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (node);
    object = json_node_get_object (node);

    g_assert_true (lsp_json_has (object, "string"));
    g_assert_false (lsp_json_has (object, "null"));
    g_assert_false (lsp_json_has (object, "missing"));
    g_assert_cmpstr (lsp_json_get_string (object, "string"), ==, "ok");
    g_assert_null (lsp_json_get_string (object, "number"));
    g_assert_cmpint (lsp_json_get_int (object, "number", 9), ==, 3);
    g_assert_cmpint (lsp_json_get_int (object, "string", 9), ==, 9);
    g_assert_true (lsp_json_get_bool (object, "boolean", FALSE));
    g_assert_false (lsp_json_get_bool (object, "string", FALSE));
    g_assert_nonnull (lsp_json_get_object (object, "object"));
    g_assert_nonnull (lsp_json_get_array (object, "array"));
    g_assert_cmpint (lsp_json_lookup_int (object, "nested/value", 9), ==, 7);
    g_assert_cmpint (lsp_json_lookup_int (object, "nested/missing", 9), ==, 9);
    g_assert_true (lsp_json_get_provider (object, "provider-object"));
    g_assert_false (lsp_json_get_provider (object, "provider-false"));
    g_assert_false (lsp_json_get_provider (object, "string"));
    g_assert_false (lsp_json_get_provider (object, "number"));
    g_assert_false (lsp_json_get_provider (object, "null"));

    json_node_unref (node);

    node = lsp_json_parse ("{\"line\":1,\"character\":2}", -1, &error);
    g_assert_no_error (error);
    object = json_node_get_object (node);
    g_assert_true (lsp_json_get_position (object, &line, &character));
    g_assert_cmpint (line, ==, 1);
    g_assert_cmpint (character, ==, 2);
    json_node_unref (node);

    /* A field with the wrong JSON type is not a position with a default zero. */
    node = lsp_json_parse ("{\"line\":\"one\",\"character\":2}", -1, &error);
    g_assert_no_error (error);
    g_assert_false (lsp_json_get_position (json_node_get_object (node),
                                            &line, &character));
    json_node_unref (node);

    node = lsp_json_parse ("{\"start\":{\"line\":0,\"character\":0}}",
                           -1, &error);
    g_assert_no_error (error);
    g_assert_false (lsp_json_get_range (json_node_get_object (node),
                                        &line, &character, NULL, NULL));
    json_node_unref (node);

    node = lsp_json_parse ("{", -1, &error);
    g_assert_null (node);
    g_assert_nonnull (error);
    g_clear_error (&error);
}


static void
test_json_parse_length (void)
{
    static const char data[] = "{\"ok\":1}trailing";
    const gsize json_len = strlen ("{\"ok\":1}");
    GError *error = NULL;
    JsonNode *node;

    node = lsp_json_parse (data, json_len, &error);
    g_assert_no_error (error);
    g_assert_nonnull (node);
    json_node_unref (node);

    node = lsp_json_parse (data, -1, &error);
    g_assert_null (node);
    g_assert_nonnull (error);
    g_clear_error (&error);

    node = lsp_json_parse ("", 0, &error);
    g_assert_null (node);
    g_clear_error (&error);
}


static void
test_json_position_bounds (void)
{
    static const char *invalid[] = {
        "{\"line\": 1.5, \"character\": 0}",
        "{\"line\": -1, \"character\": 0}",
        "{\"line\": 2147483648, \"character\": 0}",
        NULL
    };
    GError *error = NULL;
    JsonNode *node;
    guint i;
    int line, character;

    for (i = 0; invalid[i]; ++i)
    {
        node = lsp_json_parse (invalid[i], -1, &error);
        g_assert_no_error (error);
        g_assert_false (lsp_json_get_position (json_node_get_object (node),
                                                &line, &character));
        json_node_unref (node);
    }

    node = lsp_json_parse ("{\"line\": 1.0, \"character\": 2.0}", -1, &error);
    g_assert_no_error (error);
    g_assert_true (lsp_json_get_position (json_node_get_object (node),
                                          &line, &character));
    g_assert_cmpint (line, ==, 1);
    g_assert_cmpint (character, ==, 2);
    json_node_unref (node);
}


static void
test_json_accessors (void)
{
    static const char *strings[] = { "one", "два", NULL };
    JsonObject *object = json_object_new ();
    JsonObject *nested = json_object_new ();
    JsonObject *range;
    JsonArray *array;
    char *text;
    gsize len;
    int start_line, start_character, end_line, end_character;

    lsp_json_set_string (object, "string", "value");
    lsp_json_set_string (object, "null-string", NULL);
    lsp_json_set_int (object, "integer", 42);
    lsp_json_set_bool (object, "boolean", TRUE);
    lsp_json_set_object (object, "nested", nested);
    lsp_json_set_array (object, "array", lsp_json_string_array (strings));
    lsp_json_set_null (object, "explicit-null");
    lsp_json_set_object (object, "null-object", NULL);
    lsp_json_set_array (object, "null-array", NULL);
    lsp_json_set_node (object, "null-node", NULL);

    g_assert_cmpstr (lsp_json_get_string (object, "string"), ==, "value");
    g_assert_null (lsp_json_get_string (object, "null-string"));
    g_assert_cmpint (lsp_json_get_int (object, "integer", 0), ==, 42);
    g_assert_true (lsp_json_get_bool (object, "boolean", FALSE));
    g_assert_null (lsp_json_get_object (object, "null-object"));
    g_assert_null (lsp_json_get_array (object, "null-array"));
    g_assert_true (JSON_NODE_HOLDS_NULL (lsp_json_get_node (object, "null-node")));
    g_assert_cmpstr (lsp_json_lookup_string (object, "nested/missing"), ==, NULL);
    g_assert_cmpint (lsp_json_lookup_int (object, "nested/missing", 7), ==, 7);
    g_assert_false (lsp_json_lookup_bool (object, "nested/missing", FALSE));
    g_assert_true (lsp_json_get_array (object, "array") != NULL);
    array = lsp_json_get_array (object, "array");
    g_assert_cmpuint (json_array_get_length (array), ==, 2);
    g_assert_cmpstr (json_array_get_string_element (array, 1), ==, "два");

    range = lsp_json_range (2, 3, 4, 5);
    g_assert_true (lsp_json_get_range (range,
                                       &start_line, &start_character,
                                       &end_line, &end_character));
    g_assert_cmpint (start_line, ==, 2);
    g_assert_cmpint (start_character, ==, 3);
    g_assert_cmpint (end_line, ==, 4);
    g_assert_cmpint (end_character, ==, 5);
    g_assert_true (lsp_json_get_range (range, NULL, NULL, NULL, NULL));
    json_object_unref (range);

    text = lsp_json_object_to_string (object, &len);
    g_assert_nonnull (text);
    g_assert_cmpuint (len, ==, strlen (text));
    g_assert_nonnull (strstr (text, "\"string\":\"value\""));
    g_free (text);
    json_object_unref (object);
}


static void
test_json_null_object (void)
{
    g_assert_false (lsp_json_has (NULL, "member"));
    g_assert_null (lsp_json_get_node (NULL, "member"));
    g_assert_null (lsp_json_get_string (NULL, "member"));
    g_assert_cmpint (lsp_json_get_int (NULL, "member", 11), ==, 11);
    g_assert_true (lsp_json_get_bool (NULL, "member", TRUE));
    g_assert_null (lsp_json_get_object (NULL, "member"));
    g_assert_null (lsp_json_get_array (NULL, "member"));
    g_assert_null (lsp_json_lookup_object (NULL, "nested/member"));
    g_assert_null (lsp_json_lookup_string (NULL, "nested/member"));
    g_assert_cmpint (lsp_json_lookup_int (NULL, "nested/member", 13), ==, 13);
    g_assert_false (lsp_json_lookup_bool (NULL, "nested/member", FALSE));
    g_assert_false (lsp_json_get_provider (NULL, "member"));
}


static void
test_json_lookup_paths (void)
{
    JsonObject *object = json_object_new ();
    JsonObject *nested = json_object_new ();

    lsp_json_set_object (object, "nested", nested);
    lsp_json_set_int (nested, "value", 42);

    g_assert_cmpint (lsp_json_lookup_int (object, "nested/value", 0), ==, 42);
    g_assert_cmpint (lsp_json_lookup_int (object, "", 7), ==, 7);
    g_assert_cmpint (lsp_json_lookup_int (object, "nested/", 8), ==, 8);
    g_assert_cmpint (lsp_json_lookup_int (object, "/nested", 9), ==, 9);
    g_assert_cmpint (lsp_json_lookup_int (object, "nested//value", 10), ==, 10);
    g_assert_cmpint (lsp_json_lookup_int (object, "nested/value/member", 11), ==, 11);
    g_assert_null (lsp_json_lookup_object (object, "nested/value"));

    json_object_unref (object);
}


static void
test_json_string_array_edges (void)
{
    static const char *strings[] = { "", "привет", NULL };
    JsonArray *array;

    array = lsp_json_string_array (NULL);
    g_assert_nonnull (array);
    g_assert_cmpuint (json_array_get_length (array), ==, 0);
    json_array_unref (array);

    array = lsp_json_string_array (strings);
    g_assert_cmpuint (json_array_get_length (array), ==, 2);
    g_assert_cmpstr (json_array_get_string_element (array, 0), ==, "");
    g_assert_cmpstr (json_array_get_string_element (array, 1), ==, "привет");
    json_array_unref (array);
}


static void
test_json_serialization_edges (void)
{
    JsonNode *node;
    char *text;
    gsize len;

    node = json_node_new (JSON_NODE_NULL);
    text = lsp_json_to_string (node, &len);
    g_assert_cmpstr (text, ==, "null");
    g_assert_cmpuint (len, ==, 4);
    g_free (text);
    json_node_free (node);

    node = json_node_new (JSON_NODE_VALUE);
    json_node_set_string (node, "text");
    text = lsp_json_to_string (node, NULL);
    g_assert_cmpstr (text, ==, "\"text\"");
    g_free (text);
    json_node_free (node);
}


static void
test_hover_text_shapes (void)
{
    const char *replies[] = {
        "{\"contents\":\"plain\"}",
        "{\"contents\":{\"kind\":\"markdown\",\"value\":\"marked\"}}",
        "{\"contents\":[\"one\",{\"language\":\"c\",\"value\":\"two\"},\"\",3]}",
        "{\"contents\":null}",
        "{\"contents\":{\"value\":\"\"}}",
        "{\"contents\":[]}",
        "{}"
    };
    const char *expected[] = { "plain", "marked", "one\ntwo", NULL, NULL, NULL, NULL };

    for (guint i = 0; i < G_N_ELEMENTS (replies); ++i)
    {
        GError *error = NULL;
        JsonNode *node = lsp_json_parse (replies[i], -1, &error);
        char *text;

        g_assert_no_error (error);
        g_assert_nonnull (node);
        text = lsp_hover_text (node);
        g_assert_cmpstr (text, ==, expected[i]);
        g_free (text);
        json_node_unref (node);
    }
}


static void
test_completion_item_edges (void)
{
    const char *json = "[{\"label\":\"ok\"},{\"label\":\"\"},"
                       "{\"label\":3},{\"detail\":\"missing label\"},"
                       "{\"label\":\"edit\",\"textEdit\":{\"newText\":3}}]";
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);

    g_assert_no_error (error);
    g_assert_nonnull (node);
    g_assert_cmpuint (_lsp_completion_item_count (node), ==, 2);
    json_node_unref (node);
}


static void
test_completion_item_limit (void)
{
    GString *json = g_string_new ("[");
    GError *error = NULL;
    JsonNode *node;
    guint i;

    for (i = 0; i < 201; ++i)
    {
        if (i)
            g_string_append_c (json, ',');
        g_string_append_printf (json, "{\"label\":\"item%u\"}", i);
    }
    g_string_append_c (json, ']');

    node = lsp_json_parse (json->str, -1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (node);
    g_assert_cmpuint (_lsp_completion_item_count (node), ==, 200);

    json_node_unref (node);
    g_string_free (json, TRUE);
}


static void
test_completion_item_fields (void)
{
    const char *json =
        "[{\"label\":\"second\",\"insertText\":\"fallback\","
        "  \"filterText\":\"needle\",\"sortText\":\"2\"},"
        " {\"label\":\"first\",\"insertText\":\"insert\","
        "  \"sortText\":\"1\",\"textEdit\":{\"newText\":\"edit\","
        "  \"range\":{\"start\":{\"line\":3,\"character\":4},"
        "  \"end\":{\"line\":3,\"character\":8}}}}]";
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    char *summary;

    g_assert_no_error (error);
    g_assert_nonnull (node);
    summary = _lsp_completion_item_summary (node);
    g_assert_cmpstr (summary, ==,
                     "first|edit|first|1|3:4-3:8\n"
                     "second|fallback|needle|2|-1:-1--1:-1");
    g_free (summary);
    json_node_unref (node);
}


static void
test_completion_item_ranges (void)
{
    const char *json =
        "[{\"label\":\"replace\",\"textEdit\":{\"newText\":\"r\","
        "  \"replace\":{\"start\":{\"line\":1,\"character\":2},"
        "  \"end\":{\"line\":1,\"character\":5}}}},"
        " {\"label\":\"insert\",\"insertText\":\"i\","
        "  \"textEdit\":{\"range\":{\"start\":{\"line\":\"bad\","
        "  \"character\":0},\"end\":{\"line\":0,\"character\":1}}}}]";
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    char *summary;

    g_assert_no_error (error);
    g_assert_nonnull (node);
    summary = _lsp_completion_item_summary (node);
    g_assert_cmpstr (summary, ==,
                     "insert|i|insert||-1:-1--1:-1\n"
                     "replace|r|replace||1:2-1:5");
    g_free (summary);
    json_node_unref (node);
}


static void
test_completion_item_empty_text (void)
{
    const char *json =
        "[{\"label\":\"empty-edit\",\"insertText\":\"fallback\","
        "  \"textEdit\":{\"newText\":\"\"}},"
        " {\"label\":\"empty-insert\",\"insertText\":\"\"}]";
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    char *summary;

    g_assert_no_error (error);
    g_assert_nonnull (node);
    summary = _lsp_completion_item_summary (node);
    g_assert_cmpstr (summary, ==,
                     "empty-edit||empty-edit||-1:-1--1:-1\n"
                     "empty-insert||empty-insert||-1:-1--1:-1");
    g_free (summary);
    json_node_unref (node);
}


static void
test_completion_item_label_whitespace (void)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse ("[{\"label\":\"  spaced  \"}]",
                                     -1, &error);
    char *summary;

    g_assert_no_error (error);
    g_assert_nonnull (node);
    summary = _lsp_completion_item_summary (node);
    g_assert_cmpstr (summary, ==, "spaced|spaced|spaced||-1:-1--1:-1");
    g_free (summary);
    json_node_unref (node);
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


static void
test_malformed_replies (void)
{
    GError *error = NULL;
    JsonNode *node;
    GSList *diagnostics;
    LspDiagnostic *diagnostic;
    GArray *rows;

    node = lsp_json_parse (
        "[{\"message\":\"bad range\","
        "  \"range\": {\"start\": {\"line\":\"zero\",\"character\":0},"
        "              \"end\": {\"line\":0,\"character\":1}}},"
        " {\"message\":\"valid\","
        "  \"range\": {\"start\": {\"line\":0,\"character\":0},"
        "              \"end\": {\"line\":0,\"character\":1}},"
        "  \"severity\":\"warning\",\"code\":true},"
        " {\"message\":7,"
        "  \"range\": {\"start\": {\"line\":0,\"character\":0},"
        "              \"end\": {\"line\":0,\"character\":1}}}]",
        -1, &error);
    g_assert_no_error (error);
    diagnostics = lsp_diagnostics_parse (json_node_get_array (node));
    g_assert_cmpuint (g_slist_length (diagnostics), ==, 1);
    diagnostic = (LspDiagnostic*) diagnostics->data;
    g_assert_cmpstr (diagnostic->message, ==, "valid");
    g_assert_cmpint (diagnostic->severity, ==, LSP_SEVERITY_ERROR);
    g_assert_null (diagnostic->code);
    lsp_diagnostics_free (diagnostics);
    json_node_unref (node);

    /* Missing and malformed ranges do not create symbol rows. */
    rows = symbols_of (
        "[{\"name\":\"missing\"},"
        " {\"name\":\"bad\",\"range\":"
        "  {\"start\": {\"line\":\"zero\",\"character\":0},"
        "   \"end\": {\"line\":0,\"character\":1}}},"
        " {\"name\":\"good\",\"range\":"
        "  {\"start\": {\"line\":0,\"character\":0},"
        "   \"end\": {\"line\":0,\"character\":1}}}]",
        "good\n", LSP_POSITION_ENCODING_UTF16);
    g_assert_cmpuint (rows->len, ==, 1);
    g_assert_cmpstr (g_array_index (rows, SymbolRow, 0).name, ==, "good");
    free_rows (rows);
}


/* -------------------------------------------------------------------------
 * lsp.xml, and the walk that finds the root of a project
 */

static char *
write_temp (const char *dir,
            const char *name,
            const char *contents)
{
    char *path = g_build_filename (dir, name, nullptr);
    GError *error = NULL;

    g_assert_true (g_file_set_contents (path, contents, -1, &error));
    g_assert_no_error (error);

    return path;
}


static void
test_config_parse (void)
{
    static const char *xml =
        "<?xml version=\"1.0\"?>\n"
        "<medit-lsp version=\"1.0\">\n"
        "  <server id=\"first\">\n"
        "    <filter>langs:c,cpp</filter>\n"
        "    <command>clangd --background-index</command>\n"
        "    <root>compile_commands.json;.git</root>\n"
        "    <env>PATH_EXTRA=/opt/bin</env>\n"
        "    <env>QUIET=1</env>\n"
        "    <initialization-options>{ \"a\": 1 }</initialization-options>\n"
        "  </server>\n"
        "  <server id=\"off\" enabled=\"false\">\n"
        "    <filter>globs:*.txt</filter>\n"
        "    <command>nothing</command>\n"
        "  </server>\n"
        "</medit-lsp>\n";

    char *dir = g_dir_make_tmp ("medit-unit-XXXXXX", NULL);
    char *path = write_temp (dir, "lsp.xml", xml);
    GSList *list = lsp_config_parse_file (path);
    LspServerConfig *first, *off;

    g_assert_cmpuint (g_slist_length (list), ==, 2);

    first = (LspServerConfig*) list->data;
    off = (LspServerConfig*) list->next->data;

    /* In file order, which is what makes "the first match wins" a rule. */
    g_assert_cmpstr (first->id, ==, "first");
    g_assert_cmpstr (off->id, ==, "off");

    g_assert_true (first->enabled);
    g_assert_false (off->enabled);

    g_assert_cmpstr (first->filter, ==, "langs:c,cpp");

    /* The command is split the way a shell would, and kept as written for
       the messages. */
    g_assert_cmpstr (first->command, ==, "clangd --background-index");
    g_assert_cmpstr (first->argv[0], ==, "clangd");
    g_assert_cmpstr (first->argv[1], ==, "--background-index");
    g_assert_null (first->argv[2]);

    /* Semicolons, because a file name may contain a comma. */
    g_assert_cmpstr (first->root_markers[0], ==, "compile_commands.json");
    g_assert_cmpstr (first->root_markers[1], ==, ".git");
    g_assert_null (first->root_markers[2]);

    g_assert_cmpstr (first->env[0], ==, "PATH_EXTRA=/opt/bin");
    g_assert_cmpstr (first->env[1], ==, "QUIET=1");
    g_assert_null (first->env[2]);

    /* Ordinary text rather than CDATA, which MooMarkup would read as a
       comment and hand back as nothing. */
    g_assert_nonnull (first->init_options);
    g_assert_nonnull (strstr (first->init_options, "\"a\": 1"));

    lsp_config_list_free (list);

    g_remove (path);
    g_rmdir (dir);
    g_free (path);
    g_free (dir);
}


static void
test_config_parse_bad (void)
{
    char *dir = g_dir_make_tmp ("medit-unit-XXXXXX", NULL);
    char *path = write_temp (dir, "lsp.xml", "<medit-lsp><server id=\"x\">");

    /* Malformed: a warning and an empty list, not a crash and not half a
       server. The warning is the point of the g_test_expect_message. */
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*could not parse*");
    g_assert_null (lsp_config_parse_file (path));
    g_test_assert_expected_messages ();

    g_remove (path);
    g_rmdir (dir);
    g_free (path);
    g_free (dir);
}


static void
test_markup_rejects_invalid_comment (void)
{
    GError *error = NULL;
    MooMarkupDoc *doc;

    /* XML comments may not contain two consecutive hyphens. GMarkup accepts
       this input, but the configuration file is XML and must be portable to
       a conforming parser. */
    doc = moo_markup_parse_memory ("<root><!-- clangd --background-index --></root>",
                                   -1, &error);

    g_assert_null (doc);
    g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE);
    g_clear_error (&error);
}


static void
test_config_skips_invalid_servers (void)
{
    static const char *xml =
        "<medit-lsp>"
        "  <server><filter>langs:c</filter><command>missing-id</command></server>"
        "  <server id=\"missing-command\"><filter>langs:c</filter></server>"
        "  <server id=\"bad-command\"><filter>langs:c</filter>"
        "    <command>'unterminated</command></server>"
        "  <server id=\"missing-filter\"><command>missing-filter</command></server>"
        "  <server id=\"valid\"><filter>langs:c</filter>"
        "    <command>clangd --query-driver='a b'</command>"
        "    <env>   </env><root> ; , </root></server>"
        "</medit-lsp>";
    char *dir = g_dir_make_tmp ("medit-unit-XXXXXX", NULL);
    char *path = write_temp (dir, "lsp.xml", xml);
    GSList *list;
    LspServerConfig *config;

    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*without an id*");
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*no <command>*");
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*unparsable*");
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*no <filter>*");
    list = lsp_config_parse_file (path);
    g_test_assert_expected_messages ();

    g_assert_cmpuint (g_slist_length (list), ==, 1);
    config = (LspServerConfig*) list->data;
    g_assert_cmpstr (config->id, ==, "valid");
    g_assert_cmpstr (config->argv[0], ==, "clangd");
    g_assert_cmpstr (config->argv[1], ==, "--query-driver=a b");
    g_assert_null (config->argv[2]);
    g_assert_null (config->env);
    g_assert_null (config->root_markers);

    lsp_config_list_free (list);
    g_remove (path);
    g_rmdir (dir);
    g_free (path);
    g_free (dir);
}


/*
 * The markers here are made up rather than the ".git" and "go.mod" a real
 * lsp.xml names, and that is the whole point: the walk goes up from a directory
 * under the temp directory, through it and to the root, so a marker with a real
 * name anywhere above -- a stray /tmp/.git, which is a thing that happens --
 * would be found and the last case here would report that directory instead.
 * The names below cannot be up there.
 */
static void
test_config_root (void)
{
    char *dir = g_dir_make_tmp ("medit-unit-XXXXXX", NULL);
    char *sub = g_build_filename (dir, "a", "b", nullptr);
    char *marker;
    char *markers[] = { (char*) ".medit-unit-root", (char*) "medit-unit-root.mod", NULL };
    char *none[] = { NULL };
    char *root;

    g_assert_cmpint (g_mkdir_with_parents (sub, 0700), ==, 0);
    marker = write_temp (dir, ".medit-unit-root", "whatever a marker holds\n");

    /* Two directories down, and the marker at the top: the top is the root. */
    root = lsp_config_find_root (sub, markers);
    g_assert_cmpstr (root, ==, dir);
    g_free (root);

    /* The directory holding the marker is its own root. */
    root = lsp_config_find_root (dir, markers);
    g_assert_cmpstr (root, ==, dir);
    g_free (root);

    /* No markers at all: the file's own directory, without a walk. */
    root = lsp_config_find_root (sub, none);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    root = lsp_config_find_root (sub, NULL);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    /* Nothing matches anywhere above, and the walk stops at / rather than
       going round for ever. */
    g_remove (marker);
    root = lsp_config_find_root (sub, markers);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    g_rmdir (sub);
    g_free (sub);
    sub = g_build_filename (dir, "a", nullptr);
    g_rmdir (sub);
    g_rmdir (dir);

    g_free (marker);
    g_free (sub);
    g_free (dir);
}


/* -------------------------------------------------------------------------
 * The bracketed detail the pane puts after a message
 */

static void
check_detail (const char *source,
              const char *code,
              const char *expected)
{
    LspDiagnostic diagnostic;
    char *detail;

    memset (&diagnostic, 0, sizeof diagnostic);
    diagnostic.source = (char*) source;
    diagnostic.code = (char*) code;

    detail = lsp_diagnostic_detail (&diagnostic);

    if (!expected)
        g_assert_null (detail);
    else
        g_assert_cmpstr (detail, ==, expected);

    g_free (detail);
}


static void
test_diagnostic_detail (void)
{
    /* All four of them, which is three more than the pane test can drive in
       one run: a server may send either, both or neither. */
    check_detail ("clangd", "E42", "  [clangd E42]");
    check_detail ("clangd", NULL, "  [clangd]");
    check_detail (NULL, "E42", "  [E42]");
    check_detail (NULL, NULL, NULL);
}


static void
test_diagnostic_fields (void)
{
    GError *error = NULL;
    JsonNode *node;
    GSList *diagnostics;
    LspDiagnostic *diagnostic;

    node = lsp_json_parse (
        "[{\"message\":\"string\",\"code\":\"E1\","
        "  \"range\": {\"start\": {\"line\":0,\"character\":0},"
        "              \"end\": {\"line\":0,\"character\":1}}},"
        " {\"message\":\"integer\",\"code\":42,"
        "  \"range\": {\"start\": {\"line\":1,\"character\":0},"
        "              \"end\": {\"line\":1,\"character\":1}},"
        "  \"severity\":2},"
        " {\"message\":\"number\",\"code\":42.5,"
        "  \"range\": {\"start\": {\"line\":2,\"character\":0},"
        "              \"end\": {\"line\":2,\"character\":1}},"
        "  \"severity\":4}]", -1, &error);
    g_assert_no_error (error);
    diagnostics = lsp_diagnostics_parse (json_node_get_array (node));

    g_assert_cmpuint (g_slist_length (diagnostics), ==, 3);
    diagnostic = (LspDiagnostic*) diagnostics->data;
    g_assert_cmpstr (diagnostic->code, ==, "E1");
    g_assert_cmpint (diagnostic->severity, ==, LSP_SEVERITY_ERROR);
    diagnostic = (LspDiagnostic*) diagnostics->next->data;
    g_assert_cmpstr (diagnostic->code, ==, "42");
    g_assert_cmpint (diagnostic->severity, ==, LSP_SEVERITY_WARNING);
    diagnostic = (LspDiagnostic*) diagnostics->next->next->data;
    g_assert_cmpstr (diagnostic->code, ==, "42");
    g_assert_cmpint (diagnostic->severity, ==, LSP_SEVERITY_HINT);

    lsp_diagnostics_free (diagnostics);
    json_node_unref (node);

    g_assert_cmpstr (lsp_severity_name (LSP_SEVERITY_ERROR), ==,
                     C_ ("diagnostic severity", "error"));
    g_assert_cmpstr (lsp_severity_name (LSP_SEVERITY_WARNING), ==,
                     C_ ("diagnostic severity", "warning"));
    g_assert_cmpstr (lsp_severity_name (LSP_SEVERITY_INFORMATION), ==,
                     C_ ("diagnostic severity", "information"));
    g_assert_cmpstr (lsp_severity_name (LSP_SEVERITY_HINT), ==,
                     C_ ("diagnostic severity", "hint"));
    g_assert_cmpstr (lsp_severity_name (99), ==,
                     C_ ("diagnostic severity", "error"));
}


/* -------------------------------------------------------------------------
 * Where the word being completed starts
 */

static int
word_start_of (const char *text,
               int         cursor)
{
    GtkTextBuffer *buffer = buffer_with (text);
    GtkTextIter iter, start;
    int offset;

    gtk_text_buffer_get_iter_at_offset (buffer, &iter, cursor);
    gtk_text_buffer_place_cursor (buffer, &iter);

    lsp_completion_word_start (buffer, &start);
    offset = gtk_text_iter_get_offset (&start);

    g_object_unref (buffer);

    return offset;
}


static void
test_completion_word_start (void)
{
    /* In the middle of a word, and at the end of one. */
    g_assert_cmpint (word_start_of ("alpha beta", 10), ==, 6);
    g_assert_cmpint (word_start_of ("alpha beta", 8), ==, 6);

    /* Just after a space there is no word yet, so the prefix is empty and the
       server's answer is not narrowed at all -- which is why a test that means
       to check the narrowing has to put the cursor at the end of a word. */
    g_assert_cmpint (word_start_of ("alpha beta", 6), ==, 6);

    /* Digits and the underscore are part of it, a dot is not: that is what
       makes completion after "obj." offer everything and after "obj.fi"
       offer what starts with "fi". */
    g_assert_cmpint (word_start_of ("foo_bar9", 8), ==, 0);
    g_assert_cmpint (word_start_of ("obj.field", 9), ==, 4);

    /* The line is the limit, so a word does not reach into the one above. */
    g_assert_cmpint (word_start_of ("alpha\nbeta", 10), ==, 6);
    g_assert_cmpint (word_start_of ("alpha\nbeta", 6), ==, 6);

    /* Letters are letters in any alphabet: g_unichar_isalnum, not isalpha of
       the C locale. */
    g_assert_cmpint (word_start_of ("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", 6), ==, 0);
}


/* -------------------------------------------------------------------------
 * The places a question about a position is answered with
 */

static GSList *
locations_of (const char *json)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    GSList *found;

    g_assert_no_error (error);
    g_assert_nonnull (node);

    found = lsp_locations_parse (node, LSP_POSITION_ENCODING_UTF16);

    json_node_unref (node);

    return found;
}


static void
check_location (GSList     *locations,
                guint       index,
                const char *path,
                int         line,
                int         character)
{
    LspLocation *location = (LspLocation*) g_slist_nth_data (locations, index);

    g_assert_nonnull (location);
    g_assert_cmpstr (location->path, ==, path);
    g_assert_cmpint (location->line, ==, line);
    g_assert_cmpint (location->character, ==, character);
}


static void
test_locations_array (void)
{
    /* What a references reply is: an array of Location, in the server's own
       order, which is the order the pane lists them in. */
    static const char *reply =
        "[{\"uri\": \"file:///tmp/a.txt\","
        "  \"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "              \"end\": {\"line\": 0, \"character\": 5}}},"
        " {\"uri\": \"file:///tmp/b.txt\","
        "  \"range\": {\"start\": {\"line\": 2, \"character\": 4},"
        "              \"end\": {\"line\": 2, \"character\": 9}}}]";

    GSList *found = locations_of (reply);

    g_assert_cmpuint (g_slist_length (found), ==, 2);
    check_location (found, 0, "/tmp/a.txt", 0, 0);
    check_location (found, 1, "/tmp/b.txt", 2, 4);

    lsp_locations_free (found);
}


static void
test_locations_single (void)
{
    /* A definition reply is often one object rather than an array of one, and
       it is the same thing: one place. */
    static const char *reply =
        "{\"uri\": \"file:///tmp/a.txt\","
        " \"range\": {\"start\": {\"line\": 3, \"character\": 2},"
        "             \"end\": {\"line\": 3, \"character\": 7}}}";

    GSList *found = locations_of (reply);

    g_assert_cmpuint (g_slist_length (found), ==, 1);
    check_location (found, 0, "/tmp/a.txt", 3, 2);

    lsp_locations_free (found);
}


static void
test_locations_link (void)
{
    /* A LocationLink, which is what linkSupport asks servers for: the target
       is named differently, and the selection range -- the name itself -- is
       what to go to, the whole range being the entire definition. */
    static const char *reply =
        "[{\"targetUri\": \"file:///tmp/a.txt\","
        "  \"targetRange\": {\"start\": {\"line\": 4, \"character\": 0},"
        "                    \"end\": {\"line\": 9, \"character\": 1}},"
        "  \"targetSelectionRange\": {\"start\": {\"line\": 4, \"character\": 6},"
        "                             \"end\": {\"line\": 4, \"character\": 11}}}]";

    GSList *found = locations_of (reply);

    g_assert_cmpuint (g_slist_length (found), ==, 1);
    check_location (found, 0, "/tmp/a.txt", 4, 6);

    lsp_locations_free (found);
}


static void
test_locations_nothing (void)
{
    /* Null is what a server that found nothing answers, an empty array is
       what another one answers, and a uri that names no file on this machine
       is a place nothing can be done with. */
    GSList *found = locations_of ("null");

    g_assert_null (found);

    found = locations_of ("[]");
    g_assert_null (found);

    found = locations_of ("[{\"uri\": \"untitled:Untitled-1\","
                          "  \"range\": {\"start\": {\"line\": 0, \"character\": 0},"
                          "              \"end\": {\"line\": 0, \"character\": 1}}}]");
    g_assert_null (found);
}


static void
test_location_uri_edges (void)
{
    GSList *found = locations_of (
        "[{\"uri\":\"file:///tmp/a%20b.txt\","
        "  \"range\":{\"start\":{\"line\":0,\"character\":0},"
        "             \"end\":{\"line\":0,\"character\":1}}},"
        " {\"uri\":\"untitled:Scratch\","
        "  \"range\":{\"start\":{\"line\":0,\"character\":0},"
        "             \"end\":{\"line\":0,\"character\":1}}}]");

    g_assert_cmpuint (g_slist_length (found), ==, 1);
    check_location (found, 0, "/tmp/a b.txt", 0, 0);
    lsp_locations_free (found);
}


static void
test_locations_malformed (void)
{
    GSList *found = locations_of (
        "[null, {},"
        " {\"uri\":\"file:///tmp/bad.txt\",\"range\":"
        "  {\"start\":{\"line\":2,\"character\":0},"
        "   \"end\":{\"line\":1,\"character\":0}}},"
        " {\"uri\":\"file:///tmp/good.txt\",\"range\":"
        "  {\"start\":{\"line\":1,\"character\":2},"
        "   \"end\":{\"line\":1,\"character\":3}}}]");

    g_assert_cmpuint (g_slist_length (found), ==, 1);
    check_location (found, 0, "/tmp/good.txt", 1, 2);
    lsp_locations_free (found);
}


/* -------------------------------------------------------------------------
 * The edits a rename comes back as
 */

static GSList *
edits_of (const char *json)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    GSList *edits;

    g_assert_no_error (error);
    g_assert_nonnull (node);

    edits = lsp_workspace_edit_parse (node);

    json_node_unref (node);

    return edits;
}


static void
check_edit (GSList     *edits,
            guint       index,
            const char *path,
            int         line,
            int         character,
            const char *new_text)
{
    LspTextEdit *edit = (LspTextEdit*) g_slist_nth_data (edits, index);

    g_assert_nonnull (edit);
    g_assert_cmpstr (edit->path, ==, path);
    g_assert_cmpint (edit->start_line, ==, line);
    g_assert_cmpint (edit->start_character, ==, character);
    g_assert_cmpstr (edit->new_text, ==, new_text);
}


static void
test_workspace_edit_order (void)
{
    /*
     * Three edits of one file, listed by the server in the order it found
     * them, which is the order they must not be applied in: replacing the
     * first name on a line moves every range after it on that line, and the
     * second edit would then land in the wrong place -- or, with a longer
     * name, over the text that follows.
     */
    static const char *reply =
        "{\"changes\": {\"file:///tmp/a.txt\": ["
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "               \"end\": {\"line\": 0, \"character\": 5}},"
        "   \"newText\": \"omega\"},"
        "  {\"range\": {\"start\": {\"line\": 2, \"character\": 3},"
        "               \"end\": {\"line\": 2, \"character\": 8}},"
        "   \"newText\": \"omega\"},"
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 11},"
        "               \"end\": {\"line\": 0, \"character\": 16}},"
        "   \"newText\": \"omega\"}]}}";

    GSList *edits = edits_of (reply);

    g_assert_cmpuint (g_slist_length (edits), ==, 3);
    check_edit (edits, 0, "/tmp/a.txt", 2, 3, "omega");
    check_edit (edits, 1, "/tmp/a.txt", 0, 11, "omega");
    check_edit (edits, 2, "/tmp/a.txt", 0, 0, "omega");

    lsp_text_edits_free (edits);
}


static void
test_workspace_edit_files (void)
{
    /* Two files, whose edits have to come out grouped: each file is opened
       once and changed in one undo step, and that is only true of a list
       where a file's edits are together. */
    static const char *reply =
        "{\"changes\": {"
        "  \"file:///tmp/b.txt\": ["
        "    {\"range\": {\"start\": {\"line\": 1, \"character\": 0},"
        "                 \"end\": {\"line\": 1, \"character\": 5}},"
        "     \"newText\": \"omega\"}],"
        "  \"file:///tmp/a.txt\": ["
        "    {\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "                 \"end\": {\"line\": 0, \"character\": 5}},"
        "     \"newText\": \"omega\"},"
        "    {\"range\": {\"start\": {\"line\": 3, \"character\": 2},"
        "                 \"end\": {\"line\": 3, \"character\": 7}},"
        "     \"newText\": \"omega\"}]}}";

    GSList *edits = edits_of (reply);

    g_assert_cmpuint (g_slist_length (edits), ==, 3);
    check_edit (edits, 0, "/tmp/a.txt", 3, 2, "omega");
    check_edit (edits, 1, "/tmp/a.txt", 0, 0, "omega");
    check_edit (edits, 2, "/tmp/b.txt", 1, 0, "omega");

    lsp_text_edits_free (edits);
}


static void
test_workspace_edit_document_changes (void)
{
    /*
     * The other shape. medit asks for "changes" -- it does nothing with the
     * file operations documentChanges can carry -- and servers send this one
     * anyway, so it is read too. A textDocument here also carries a version,
     * which is ignored: the document medit would check it against is the one
     * on screen, and it has not changed since the request went out.
     */
    static const char *reply =
        "{\"documentChanges\": ["
        "  {\"textDocument\": {\"uri\": \"file:///tmp/a.txt\", \"version\": 3},"
        "   \"edits\": ["
        "     {\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "                  \"end\": {\"line\": 0, \"character\": 5}},"
        "      \"newText\": \"omega\"},"
        "     {\"range\": {\"start\": {\"line\": 0, \"character\": 11},"
        "                  \"end\": {\"line\": 0, \"character\": 16}},"
        "      \"newText\": \"omega\"}]},"
        "  {\"kind\": \"rename\", \"oldUri\": \"file:///tmp/a.txt\","
        "   \"newUri\": \"file:///tmp/c.txt\"}]}";

    GSList *edits = edits_of (reply);

    /* The file operation is not one of them. */
    g_assert_cmpuint (g_slist_length (edits), ==, 2);
    check_edit (edits, 0, "/tmp/a.txt", 0, 11, "omega");
    check_edit (edits, 1, "/tmp/a.txt", 0, 0, "omega");

    lsp_text_edits_free (edits);
}


static void
test_workspace_edit_nothing (void)
{
    /* A server that cannot rename what was asked about answers null, and one
       that can rename it into itself answers an edit with nothing in it. */
    g_assert_null (edits_of ("null"));
    g_assert_null (edits_of ("{}"));
    g_assert_null (edits_of ("{\"changes\": {}}"));
    g_assert_null (edits_of ("{\"documentChanges\": []}"));
}


static void
test_workspace_edit_malformed (void)
{
    static const char *reply =
        "{\"changes\": {\"file:///tmp/a.txt\": ["
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "               \"end\": {\"line\": 0, \"character\": 1}}},"
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 1},"
        "               \"end\": {\"line\": 0, \"character\": 2}},"
        "   \"newText\": 7},"
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 2},"
        "               \"end\": {\"line\": 0, \"character\": 3}},"
        "   \"newText\": \"ok\"}]}}";
    GSList *edits = edits_of (reply);

    /* TextEdit.newText is required to be a string. A missing or malformed
       replacement must not become an edit containing NULL text. */
    g_assert_cmpuint (g_slist_length (edits), ==, 1);
    check_edit (edits, 0, "/tmp/a.txt", 0, 2, "ok");

    lsp_text_edits_free (edits);
}


static void
test_workspace_edit_reversed_range (void)
{
    static const char *reply =
        "{\"changes\": {\"file:///tmp/a.txt\": ["
        "  {\"range\": {\"start\": {\"line\": 2, \"character\": 0},"
        "               \"end\": {\"line\": 1, \"character\": 0}},"
        "   \"newText\": \"bad\"},"
        "  {\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "               \"end\": {\"line\": 0, \"character\": 1}},"
        "   \"newText\": \"ok\"}]}}";
    GSList *edits = edits_of (reply);

    /* LSP Range.start must not be after Range.end. */
    g_assert_cmpuint (g_slist_length (edits), ==, 1);
    check_edit (edits, 0, "/tmp/a.txt", 0, 0, "ok");

    lsp_text_edits_free (edits);
}


/* -------------------------------------------------------------------------
 * The signature of a call, as the popup shows it
 */

static char *
markup_of (const char *json)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (json, -1, &error);
    char *markup;

    g_assert_no_error (error);
    g_assert_nonnull (node);

    markup = lsp_signature_markup (node);

    json_node_unref (node);

    return markup;
}


static void
test_signature_active_parameter (void)
{
    /*
     * The parameter being typed is emboldened inside the signature, and the
     * line under it is that parameter's own documentation. Which parameter it
     * is comes from the server: the client sends a position and is told.
     */
    static const char *reply =
        "{\"signatures\": [{\"label\": \"add(alpha: int, beta: int) -> int\","
        "  \"documentation\": \"Adds two numbers\","
        "  \"parameters\": [{\"label\": \"alpha: int\","
        "                    \"documentation\": \"the number to start from\"},"
        "                   {\"label\": \"beta: int\","
        "                    \"documentation\": \"the number to add\"}]}],"
        " \"activeSignature\": 0, \"activeParameter\": 1}";

    char *markup = markup_of (reply);

    g_assert_cmpstr (markup, ==,
                     /* "&gt;" because everything the server sent is escaped:
                        what pango is handed is markup, and a signature full
                        of arrows and templates is not. */
                     "add(alpha: int, <b>beta: int</b>) -&gt; int\n"
                     "<span foreground=\"#888888\">the number to add</span>");

    g_free (markup);
}


static void
test_signature_offsets (void)
{
    /*
     * A parameter label may be a pair of offsets into the signature instead of
     * the text of it, and the offsets are UTF-16 code units like every other
     * position the protocol sends. The emoji in the name is one character and
     * two of those, so a client that counted characters would embolden one
     * character too few and cut the label in half.
     */
    static const char *reply =
        "{\"signatures\": [{\"label\": \"f(\xf0\x9f\x98\x80x: int, y: int)\","
        "  \"parameters\": [{\"label\": [2, 10]}, {\"label\": [12, 18]}]}],"
        " \"activeParameter\": 1}";

    char *markup = markup_of (reply);

    g_assert_cmpstr (markup, ==, "f(\xf0\x9f\x98\x80x: int, <b>y: int</b>)");

    g_free (markup);
}


static void
test_signature_escaping (void)
{
    /* A C++ signature is mostly punctuation pango would read as markup. */
    static const char *reply =
        "{\"signatures\": [{\"label\": \"sort(std::vector<int>& v, bool a && b)\","
        "  \"parameters\": [{\"label\": \"std::vector<int>& v\"}]}],"
        " \"activeParameter\": 0}";

    char *markup = markup_of (reply);

    g_assert_cmpstr (markup, ==,
                     "sort(<b>std::vector&lt;int&gt;&amp; v</b>, bool a &amp;&amp; b)");

    g_free (markup);
}


static void
test_signature_choices (void)
{
    /*
     * Three things a server decides and the client only reports: which of
     * several overloads is the active one, that a signature's own
     * activeParameter wins over the one for the whole reply, and that
     * documentation may arrive as a MarkupContent rather than as a string.
     */
    static const char *reply =
        "{\"signatures\": ["
        "   {\"label\": \"f(a)\", \"parameters\": [{\"label\": \"a\"}]},"
        "   {\"label\": \"f(a, b)\", \"activeParameter\": 1,"
        "    \"documentation\": {\"kind\": \"plaintext\", \"value\": \"two of them\"},"
        "    \"parameters\": [{\"label\": \"a\"}, {\"label\": \"b\"}]}],"
        " \"activeSignature\": 1, \"activeParameter\": 0}";

    char *markup = markup_of (reply);

    /* The second signature, its own second parameter, and the signature's
       documentation, that parameter having none of its own. */
    g_assert_cmpstr (markup, ==,
                     "f(a, <b>b</b>)\n"
                     "<span foreground=\"#888888\">two of them</span>");

    g_free (markup);
}


static void
test_signature_nothing (void)
{
    char *markup;

    /* A server with nothing to say answers one of these three, and none of
       them is a popup. */
    g_assert_null (markup_of ("null"));
    g_assert_null (markup_of ("{\"signatures\": []}"));
    g_assert_null (markup_of ("{\"signatures\": [{\"label\": \"\"}]}"));

    /* An activeParameter that names no parameter -- past the end of the list,
       or a call with none -- is a signature with nothing emboldened, not a
       reason to show nothing. */
    markup = markup_of ("{\"signatures\": [{\"label\": \"f(a)\","
                        "  \"parameters\": [{\"label\": \"a\"}]}],"
                        " \"activeParameter\": 7}");

    g_assert_cmpstr (markup, ==, "f(a)");
    g_free (markup);

    markup = markup_of ("{\"signatures\": [{\"label\": \"f()\"}]}");

    g_assert_cmpstr (markup, ==, "f()");
    g_free (markup);
}


static void
test_signature_malformed (void)
{
    char *markup = markup_of (
        "{\"signatures\": [null, {\"label\": 7},"
        " {\"label\": \"f(x)\","
        "  \"parameters\": [null, {\"label\": [2]}]}],"
        " \"activeSignature\": 2, \"activeParameter\": 1}");

    /* Bad signatures and parameter ranges are ignored while the usable label
       remains displayable. */
    g_assert_cmpstr (markup, ==, "f(x)");
    g_free (markup);
}


/* -------------------------------------------------------------------------
 * The other uses of what the cursor is in
 */

static void
test_highlight_kinds (void)
{
    /*
     * The kind is what tells a place a symbol is written to from a place it is
     * read from, and the two are marked differently. A server that sends none
     * means Text, which is the specification's own default rather than a
     * guess made here.
     */
    static const char *reply =
        "[{\"range\": {\"start\": {\"line\": 0, \"character\": 0},"
        "             \"end\": {\"line\": 0, \"character\": 5}}, \"kind\": 2},"
        " {\"range\": {\"start\": {\"line\": 1, \"character\": 6},"
        "             \"end\": {\"line\": 1, \"character\": 11}}, \"kind\": 3},"
        " {\"range\": {\"start\": {\"line\": 2, \"character\": 0},"
        "             \"end\": {\"line\": 2, \"character\": 5}}}]";

    GError *error = NULL;
    JsonNode *node = lsp_json_parse (reply, -1, &error);
    GSList *found;
    LspHighlight *third;

    g_assert_no_error (error);
    found = lsp_highlight_parse (node);

    g_assert_cmpuint (g_slist_length (found), ==, 3);
    g_assert_cmpint (((LspHighlight*) found->data)->kind, ==, 2);
    g_assert_cmpint (((LspHighlight*) found->data)->start_character, ==, 0);
    g_assert_cmpint (((LspHighlight*) found->next->data)->kind, ==, 3);

    third = (LspHighlight*) found->next->next->data;
    g_assert_cmpint (third->kind, ==, 1);
    g_assert_cmpint (third->start_line, ==, 2);
    g_assert_cmpint (third->end_character, ==, 5);

    lsp_highlight_free (found);
    json_node_unref (node);
}


static void
test_highlight_tags (void)
{
    /*
     * Which tag a kind is drawn with: a place a symbol is written to is not a
     * place it is read from, and the two are marked differently.
     *
     * The names rather than the tags, because the tags cannot be made here.
     * These tests run before gtk_init() and need no display -- a GtkTextBuffer
     * and a GtkTreeStore are objects, and both are built above -- but a
     * GtkTextTag is not: its class installs properties of gdk's colour types,
     * and without gdk initialised that is a fatal critical from
     * g_param_spec_boxed(). Where the tags land is what the UI test sees.
     */
    g_assert_cmpstr (lsp_highlight_tag_name (2), ==, LSP_HIGHLIGHT_TAG_READ);
    g_assert_cmpstr (lsp_highlight_tag_name (3), ==, LSP_HIGHLIGHT_TAG_WRITE);

    /* Text, and a kind no version of the protocol has: neither is a write. */
    g_assert_cmpstr (lsp_highlight_tag_name (1), ==, LSP_HIGHLIGHT_TAG_READ);
    g_assert_cmpstr (lsp_highlight_tag_name (99), ==, LSP_HIGHLIGHT_TAG_READ);
}


static void
test_highlight_malformed (void)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse (
        "[null, {\"range\": {\"start\": {\"line\": 2, \"character\": 0},"
        "  \"end\": {\"line\": 1, \"character\": 0}}},"
        " {\"range\": {\"start\": {\"line\": 0, \"character\": 1},"
        "  \"end\": {\"line\": 0, \"character\": 2}}}]", -1, &error);
    GSList *found;

    g_assert_no_error (error);
    found = lsp_highlight_parse (node);
    g_assert_cmpuint (g_slist_length (found), ==, 1);
    g_assert_cmpint (((LspHighlight*) found->data)->start_line, ==, 0);
    g_assert_cmpint (((LspHighlight*) found->data)->start_character, ==, 1);
    lsp_highlight_free (found);
    json_node_unref (node);
}


static void
test_highlight_nothing (void)
{
    GError *error = NULL;
    JsonNode *node = lsp_json_parse ("null", -1, &error);

    g_assert_no_error (error);
    g_assert_null (lsp_highlight_parse (node));
    json_node_unref (node);

    node = lsp_json_parse ("[]", -1, &error);
    g_assert_no_error (error);
    g_assert_null (lsp_highlight_parse (node));
    json_node_unref (node);

    /* A range that is not one is a highlight nothing can be put on. */
    node = lsp_json_parse ("[{\"kind\": 2}]", -1, &error);
    g_assert_no_error (error);
    g_assert_null (lsp_highlight_parse (node));
    json_node_unref (node);
}


static void
test_client_framing (void)
{
    const char *first = "Content-Length: 7\r\nX-Test: yes\r\n\r\n{\"x\":1}";
    const char *second = "content-length: 7\n\n{\"y\":2}";
    const char *malformed = "X-Test: yes\r\n\r\n{}";
    gsize body_offset;
    gsize body_len;
    gssize total;
    char *stream;

    total = _lsp_client_find_message ((const guint8*) first, strlen (first) - 2,
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, 0);

    total = _lsp_client_find_message ((const guint8*) first, strlen (first),
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, (gssize) strlen (first));
    g_assert_cmpuint (body_len, ==, 7);
    g_assert_cmpmem (first + body_offset, body_len, "{\"x\":1}", 7);

    stream = g_strconcat (first, second, nullptr);
    total = _lsp_client_find_message ((const guint8*) stream, strlen (stream),
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, (gssize) strlen (first));
    total = _lsp_client_find_message ((const guint8*) stream + total,
                                      strlen (stream) - total,
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, (gssize) strlen (second));
    g_assert_cmpuint (body_len, ==, 7);
    g_assert_cmpmem (stream + strlen (first) + body_offset, body_len,
                     "{\"y\":2}", 7);
    g_free (stream);

    total = _lsp_client_find_message ((const guint8*) malformed,
                                      strlen (malformed),
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, -1);
}


static void
test_client_framing_edges (void)
{
    const char *empty = "Content-Length: 0\r\n\r\n";
    const char *incomplete = "Content-Length: 4\r\n\r\nxy";
    const char *not_number = "Content-Length: nope\r\n\r\n";
    const char *negative = "Content-Length: -1\r\n\r\n";
    const char *too_large = "Content-Length: 100000001\r\n\r\n";
    const char *duplicate = "Content-Length: 1\r\nContent-Length: 1\r\n\r\nx";
    gsize body_offset = 0;
    gsize body_len = 0;

    g_assert_cmpint (_lsp_client_find_message ((const guint8*) empty,
                                                strlen (empty),
                                                &body_offset, &body_len),
                     ==, (gssize) strlen (empty));
    g_assert_cmpuint (body_offset, ==, strlen (empty));
    g_assert_cmpuint (body_len, ==, 0);

    g_assert_cmpint (_lsp_client_find_message ((const guint8*) incomplete,
                                                strlen (incomplete),
                                                &body_offset, &body_len),
                     ==, 0);

    g_assert_cmpint (_lsp_client_find_message ((const guint8*) not_number,
                                                strlen (not_number),
                                                &body_offset, &body_len),
                     ==, -1);
    g_assert_cmpint (_lsp_client_find_message ((const guint8*) negative,
                                                strlen (negative),
                                                &body_offset, &body_len),
                     ==, -1);
    g_assert_cmpint (_lsp_client_find_message ((const guint8*) too_large,
                                                strlen (too_large),
                                                &body_offset, &body_len),
                     ==, -1);
    g_assert_cmpint (_lsp_client_find_message ((const guint8*) duplicate,
                                                strlen (duplicate),
                                                &body_offset, &body_len),
                     ==, -1);
}


static void
test_client_framing_binary_body (void)
{
    static const guint8 message[] = "Content-Length: 3\r\n\r\n\0aX";
    gsize body_offset = 0;
    gsize body_len = 0;
    gssize total;

    total = _lsp_client_find_message (message, sizeof (message) - 1,
                                      &body_offset, &body_len);
    g_assert_cmpint (total, ==, (gssize) (sizeof (message) - 1));
    g_assert_cmpuint (body_len, ==, 3);
    g_assert_cmpmem (message + body_offset, body_len, "\0aX", 3);
}


static void
test_client_framing_header_whitespace (void)
{
    static const char message[] =
        "X-Test: ignored\r\nContent-Length:   0  \r\n\r\n";
    gsize body_offset = 0;
    gsize body_len = 99;

    g_assert_cmpint (_lsp_client_find_message ((const guint8*) message,
                                                strlen (message),
                                                &body_offset, &body_len),
                     ==, (gssize) strlen (message));
    g_assert_cmpuint (body_offset, ==, strlen (message));
    g_assert_cmpuint (body_len, ==, 0);
}


static void
test_client_framing_number_syntax (void)
{
    static const char *valid = "Content-Length: 0003\r\n\r\nabc";
    static const char *plus = "Content-Length: +3\r\n\r\nabc";
    static const char *split_sign = "Content-Length: + 3\r\n\r\nabc";
    gsize body_offset = 0;
    gsize body_len = 0;

    g_assert_cmpint (_lsp_client_find_message ((const guint8*) valid,
                                                strlen (valid),
                                                &body_offset, &body_len),
                     ==, (gssize) strlen (valid));
    g_assert_cmpuint (body_len, ==, 3);
    g_assert_cmpint (_lsp_client_find_message ((const guint8*) plus,
                                                strlen (plus),
                                                &body_offset, &body_len),
                     ==, -1);
    g_assert_cmpint (_lsp_client_find_message ((const guint8*) split_sign,
                                                strlen (split_sign),
                                                &body_offset, &body_len),
                     ==, -1);
}


static void
test_provider_name (void)
{
    char *provider;

    provider = _lsp_provider_name ("textDocument/definition");
    g_assert_cmpstr (provider, ==, "definitionProvider");
    g_free (provider);

    provider = _lsp_provider_name ("customMethod");
    g_assert_cmpstr (provider, ==, "customMethodProvider");
    g_free (provider);

    provider = _lsp_provider_name ("textDocument/formatting");
    g_assert_cmpstr (provider, ==, "formattingProvider");
    g_free (provider);
}


static void
test_language_and_uri_helpers (void)
{
    char *path;

    g_assert_cmpstr (lsp_language_id (NULL), ==, "plaintext");
    g_assert_cmpstr (lsp_language_id (""), ==, "plaintext");
    g_assert_cmpstr (lsp_language_id (MOO_LANG_NONE), ==, "plaintext");
    g_assert_cmpstr (lsp_language_id ("chdr"), ==, "c");
    g_assert_cmpstr (lsp_language_id ("python3"), ==, "python");
    g_assert_cmpstr (lsp_language_id ("plain-custom"), ==, "plain-custom");

    g_assert_null (lsp_path_from_uri (NULL));
    g_assert_null (lsp_path_from_uri (""));
    path = lsp_path_from_uri ("file:///tmp/a%20b.c");
    g_assert_cmpstr (path, ==, "/tmp/a b.c");
    g_free (path);
    g_assert_null (lsp_path_from_uri ("https://example.com/a.c"));
}


void
_moo_lsp_add_unit_tests (void)
{
    g_test_add_func ("/lsp/json/malformed", test_json_malformed);
    g_test_add_func ("/lsp/json/parse-length", test_json_parse_length);
    g_test_add_func ("/lsp/json/position-bounds", test_json_position_bounds);
    g_test_add_func ("/lsp/json/accessors", test_json_accessors);
    g_test_add_func ("/lsp/json/null-object", test_json_null_object);
    g_test_add_func ("/lsp/json/lookup-paths", test_json_lookup_paths);
    g_test_add_func ("/lsp/json/string-array-edges", test_json_string_array_edges);
    g_test_add_func ("/lsp/json/serialization-edges", test_json_serialization_edges);
    g_test_add_func ("/lsp/hover/text-shapes", test_hover_text_shapes);
    g_test_add_func ("/lsp/completion/item-edges", test_completion_item_edges);
    g_test_add_func ("/lsp/completion/item-limit", test_completion_item_limit);
    g_test_add_func ("/lsp/completion/item-fields", test_completion_item_fields);
    g_test_add_func ("/lsp/completion/item-ranges", test_completion_item_ranges);
    g_test_add_func ("/lsp/completion/item-empty-text", test_completion_item_empty_text);
    g_test_add_func ("/lsp/completion/item-label-whitespace",
                     test_completion_item_label_whitespace);
    g_test_add_func ("/lsp/completion/word-start", test_completion_word_start);
    g_test_add_func ("/lsp/diagnostics/detail", test_diagnostic_detail);
    g_test_add_func ("/lsp/diagnostics/fields", test_diagnostic_fields);

    g_test_add_func ("/lsp/config/parse", test_config_parse);
    g_test_add_func ("/lsp/config/malformed", test_config_parse_bad);
    g_test_add_func ("/lsp/config/invalid-comment", test_markup_rejects_invalid_comment);
    g_test_add_func ("/lsp/config/invalid-servers", test_config_skips_invalid_servers);
    g_test_add_func ("/lsp/config/root", test_config_root);

    g_test_add_func ("/lsp/position/utf16", test_position_utf16);
    g_test_add_func ("/lsp/position/utf8", test_position_utf8);
    g_test_add_func ("/lsp/position/utf32", test_position_utf32);
    g_test_add_func ("/lsp/position/to-server", test_position_to_server);
    g_test_add_func ("/lsp/position/out-of-range", test_position_out_of_range);

    g_test_add_func ("/lsp/symbols/nested", test_symbols_nested);
    g_test_add_func ("/lsp/symbols/flat", test_symbols_flat);
    g_test_add_func ("/lsp/symbols/positions", test_symbols_positions);
    g_test_add_func ("/lsp/symbols/empty", test_symbols_empty);
    g_test_add_func ("/lsp/replies/malformed", test_malformed_replies);
    g_test_add_func ("/lsp/client/framing-edges", test_client_framing_edges);
    g_test_add_func ("/lsp/client/framing-binary-body", test_client_framing_binary_body);
    g_test_add_func ("/lsp/client/framing-header-whitespace",
                     test_client_framing_header_whitespace);
    g_test_add_func ("/lsp/client/framing-number-syntax",
                     test_client_framing_number_syntax);
    g_test_add_func ("/lsp/client/provider-name", test_provider_name);
    g_test_add_func ("/lsp/document/language-and-uri", test_language_and_uri_helpers);

    g_test_add_func ("/lsp/locations/array", test_locations_array);
    g_test_add_func ("/lsp/locations/single", test_locations_single);
    g_test_add_func ("/lsp/locations/link", test_locations_link);
    g_test_add_func ("/lsp/locations/nothing", test_locations_nothing);
    g_test_add_func ("/lsp/locations/uri-edges", test_location_uri_edges);
    g_test_add_func ("/lsp/locations/malformed", test_locations_malformed);

    g_test_add_func ("/lsp/rename/order", test_workspace_edit_order);
    g_test_add_func ("/lsp/rename/files", test_workspace_edit_files);
    g_test_add_func ("/lsp/rename/document-changes", test_workspace_edit_document_changes);
    g_test_add_func ("/lsp/rename/nothing", test_workspace_edit_nothing);
    g_test_add_func ("/lsp/rename/malformed", test_workspace_edit_malformed);
    g_test_add_func ("/lsp/rename/reversed-range", test_workspace_edit_reversed_range);

    g_test_add_func ("/lsp/signature/active-parameter", test_signature_active_parameter);
    g_test_add_func ("/lsp/signature/offsets", test_signature_offsets);
    g_test_add_func ("/lsp/signature/escaping", test_signature_escaping);
    g_test_add_func ("/lsp/signature/choices", test_signature_choices);
    g_test_add_func ("/lsp/signature/nothing", test_signature_nothing);
    g_test_add_func ("/lsp/signature/malformed", test_signature_malformed);

    g_test_add_func ("/lsp/highlight/kinds", test_highlight_kinds);
    g_test_add_func ("/lsp/highlight/tags", test_highlight_tags);
    g_test_add_func ("/lsp/highlight/nothing", test_highlight_nothing);
    g_test_add_func ("/lsp/highlight/malformed", test_highlight_malformed);
    g_test_add_func ("/lsp/client/framing", test_client_framing);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
