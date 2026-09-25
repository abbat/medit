/*
 *   mooutils/mooutils-tests.cpp
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
 * Three of these are the suites the fork used to have. moo_test_mooaccel(),
 * moo_test_mooutils_misc() and moo_test_moo_file_writer() went with the lua
 * interpreter that ran them -- "remove tests (assume all current tests
 * passed)" -- and the functions they were about are all still here, unchanged
 * and until now unwatched.
 *
 * The fourth is new, and is the reason the file exists at all: the "file.c:42"
 * splitting was inside a static function in main.cpp, where nothing could
 * reach it, and it had been returning a null filename since January.
 */

#include "mooutils/mooutils-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include <string.h>

#include "mooutils/mooaccel.h"
#include "mooutils/moobigpaned.h"
#include "mooutils/moofileindex.h"
#include "mooutils/moofuzzy.h"
#include "mooutils/moo-mime.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooutils-enums.h"
#include "mooutils/moouixml.h"
#include "plugins/support/moooutputfilter.h"
#include "mooutils/moohistorylist.h"
#include "mooutils/mooregion.h"
#include "mooutils/mooutils-gobject.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"
#if GTK_CHECK_VERSION(3,0,0)
#include "plugins/terminal/terminal-colors.h"
#endif



/* -------------------------------------------------------------------------
 * "file.c:42", the way a compiler names a place and medit's command line
 * takes one
 */

static void
check_file_line (const char *argument,
                 const char *expected_path,
                 int         expected_line)
{
    char *path = NULL;
    int line = -1;

    if (!expected_path)
    {
        g_assert_false (_moo_parse_file_line (argument, &path, &line));
        g_assert_null (path);
        return;
    }

    g_assert_true (_moo_parse_file_line (argument, &path, &line));
    g_assert_cmpstr (path, ==, expected_path);
    g_assert_cmpint (line, ==, expected_line);

    g_free (path);
}


static void
test_file_line (void)
{
    /* The two spellings, and the one that regressed: for eight months this
       answered with a null path, which the caller then handed to
       g_filename_to_uri(). */
    check_file_line ("/tmp/foo.c:42", "/tmp/foo.c", 42);
    check_file_line ("/tmp/foo.c(100)", "/tmp/foo.c", 100);

    /* A separator with no number is still a place: the file, at no line. */
    check_file_line ("/tmp/foo.c:", "/tmp/foo.c", 0);

    /* Nothing to split. */
    check_file_line ("/tmp/foo.c", NULL, 0);
    check_file_line ("", NULL, 0);

    /* The last one wins, which is what a path containing a colon needs --
       the caller only asks about names that are not files anyway. */
    check_file_line ("/tmp/a:1/foo.c:42", "/tmp/a:1/foo.c", 42);

    /* Not a line number, so not a line. */
    check_file_line ("/tmp/foo.c:abc", NULL, 0);
    check_file_line ("/tmp/foo.c(abc)", NULL, 0);
}


static void
test_mime_system_behaviour (void)
{
    const char *mime;

    /* Every assertion below is an answer from the host's shared-mime-info
       database rather than from anything in this tree, and a system without
       one -- a bare build container, typically -- answers nothing at all.
       Skip there instead of reporting the missing database as a failure. */
    if (!g_content_type_is_a ("text/x-csrc", "text/plain"))
    {
        g_test_skip ("no shared-mime-info database on this system");
        return;
    }

    mime = moo_get_mime_type_for_filename ("source.c");
    g_assert_cmpstr (mime, ==, "text/x-csrc");

    mime = moo_get_mime_type_for_filename ("file.without-a-known-extension");
    g_assert_cmpstr (mime, ==, MOO_MIME_TYPE_UNKNOWN);

    g_assert_true (moo_mime_type_is_subclass ("text/x-csrc", "text/plain"));
    g_assert_true (moo_mime_type_is_subclass ("text/x-csrc", "text/*"));
    g_assert_true (moo_mime_type_is_subclass ("text/x-csrc",
                                              "application/octet-stream"));
    g_assert_false (moo_mime_type_is_subclass ("text/plain", "image/*"));
}


/* -------------------------------------------------------------------------
 * moo_splitlines(), which the removed moo_test_mooutils_misc() covered
 */

static void
check_lines (const char *string,
             const char *first,
             ...)
{
    char **lines = moo_splitlines (string);
    va_list args;
    const char *expected = first;
    guint i = 0;

    va_start (args, first);

    while (expected)
    {
        g_assert_nonnull (lines);
        g_assert_cmpstr (lines[i], ==, expected);
        expected = va_arg (args, const char*);
        i += 1;
    }

    va_end (args);

    if (lines)
        g_assert_null (lines[i]);

    g_strfreev (lines);
}


static void
test_splitlines (void)
{
    /* Nothing at all rather than one empty line, which is the one case a
       caller has to handle itself -- mooedit-script.cpp does. */
    g_assert_null (moo_splitlines (""));

    check_lines ("one", "one", NULL);
    check_lines ("one\ntwo", "one", "two", NULL);

    /*
     * A terminator ends a line and starts another, so text that ends with one
     * has an empty line after it. That is split-on-separator and not what the
     * name suggests -- python's splitlines() answers ["one"] here -- and it is
     * what the callers get: moocmdview writes every element into the output
     * pane, so a chunk of output ending in a newline draws a blank line. Pinned
     * rather than changed: it is what the tree does today.
     */
    check_lines ("one\n", "one", "", NULL);
    check_lines ("one\ntwo\n", "one", "two", "", NULL);
    check_lines ("one\n\ntwo\n", "one", "", "two", "", NULL);

    /* Every line ending medit reads files with, counted the same way. */
    check_lines ("one\r\ntwo\r\n", "one", "two", "", NULL);
    check_lines ("one\rtwo\r", "one", "two", "", NULL);
}


/* -------------------------------------------------------------------------
 * Accelerators, which the removed moo_test_mooaccel() covered
 */

static void
check_accel (const char      *accel,
             guint            expected_key,
             GdkModifierType  expected_mods)
{
    guint key = 0;
    GdkModifierType mods = GdkModifierType (0);

    g_assert_true (_moo_accel_parse (accel, &key, &mods));
    g_assert_cmpuint (key, ==, expected_key);
    g_assert_cmpuint ((guint) mods, ==, (guint) expected_mods);
}


static void
test_accel_parse (void)
{
    check_accel ("<Control>s", GDK_KEY_s, GDK_CONTROL_MASK);
    check_accel ("<Shift><Control>s", GDK_KEY_s,
                 GdkModifierType (GDK_SHIFT_MASK | GDK_CONTROL_MASK));
    check_accel ("F5", GDK_KEY_F5, GdkModifierType (0));

    /* One character is itself: this is the branch that makes "s" an
       accelerator without any angle brackets. */
    check_accel ("s", GDK_KEY_s, GdkModifierType (0));

    /* And nonsense is refused rather than turned into something. */
    guint key = 0;
    GdkModifierType mods = GdkModifierType (0);

    g_assert_false (_moo_accel_parse ("<Nonsense>s", &key, &mods));
    g_assert_false (_moo_accel_parse ("<Control>", &key, &mods));
    g_assert_false (_moo_accel_parse ("Control+", &key, &mods));
    g_assert_false (_moo_accel_parse ("Control++s", &key, &mods));
}


/* -------------------------------------------------------------------------
 * The text in the accelerator column of a menu item
 *
 * GTK+2 let any string be written there. On GTK+3 the field is private and
 * the public setter takes a key and modifiers, so the strings medit passes --
 * a key name, or modifier names joined by "+" -- have to be turned into that
 * pair. What they are drawn as afterwards is GTK's business and is measured
 * in the comment beside the function.
 */

static void
check_accel_label (const char      *label,
                   guint            expected_key,
                   GdkModifierType  expected_mods)
{
    guint key = 99;
    GdkModifierType mods = GdkModifierType (99);

    _moo_menu_item_parse_accel_label (label, &key, &mods);

    g_assert_cmpuint (key, ==, expected_key);
    g_assert_cmpuint ((guint) mods, ==, (guint) expected_mods);
}


static void
test_accel_label_parse (void)
{
    /* A key name, which is what the Cancel item of the drop menu carries. */
    check_accel_label ("Escape", GDK_KEY_Escape, GdkModifierType (0));

    /* Modifier names, which is what the other three carry: they say which
       modifier chooses that action, and there is no key. */
    check_accel_label ("Shift", 0, GDK_SHIFT_MASK);
    check_accel_label ("Control", 0, GDK_CONTROL_MASK);
    check_accel_label ("Control+Shift", 0,
                       GdkModifierType (GDK_CONTROL_MASK | GDK_SHIFT_MASK));

    /* Spelt the way an accelerator is spelt everywhere else, since the first
       thing tried is the ordinary parse. */
    check_accel_label ("<Control>s", GDK_KEY_s, GDK_CONTROL_MASK);

    /* Nothing, and nonsense, leave the column empty rather than guessing. */
    check_accel_label ("", 0, GdkModifierType (0));
    check_accel_label ("Nonsense", 0, GdkModifierType (0));
}


/* -------------------------------------------------------------------------
 * MooMarkup trees built entirely in memory
 */

static void
test_markup_memory (void)
{
    GError *error = NULL;
    MooMarkupDoc *doc;
    MooMarkupNode *root;
    MooMarkupNode *child;

    doc = moo_markup_parse_memory (
        "<root answer=\"a&amp;b\"><child>one &amp; two</child>"
        "<!-- ignored --><empty/></root>", -1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (doc);

    root = moo_markup_get_root_element (doc, "root");
    g_assert_nonnull (root);
    g_assert_cmpstr (moo_markup_get_prop (root, "answer"), ==, "a&b");

    child = moo_markup_get_element (root, "child");
    g_assert_nonnull (child);
    g_assert_cmpstr (moo_markup_get_content (child), ==, "one & two");
    g_assert_nonnull (moo_markup_get_element (root, "empty"));
    g_assert_null (moo_markup_get_element (root, "missing"));

    moo_markup_set_prop (root, "answer", "changed");
    g_assert_cmpstr (moo_markup_get_prop (root, "answer"), ==, "changed");

    moo_markup_set_content (child, "replacement");
    g_assert_cmpstr (moo_markup_get_content (child), ==, "replacement");
    g_assert_null (child->children->next);

    moo_markup_doc_unref (doc);
}


static void
test_markup_mutation_modified (void)
{
    MooMarkupDoc *doc = moo_markup_doc_new ("memory");
    MooMarkupNode *root;
    MooMarkupNode *child;
    char *serialized;

    root = moo_markup_create_root_element (doc, "root");
    child = moo_markup_create_element (root, "child");
    _moo_markup_set_modified (doc, FALSE);

    moo_markup_set_content (child, "value");
    g_assert_true (_moo_markup_get_modified (doc));
    g_assert_cmpstr (moo_markup_get_content (child), ==, "value");

    _moo_markup_set_modified (doc, FALSE);
    moo_markup_set_prop (child, "name", "value");
    g_assert_true (_moo_markup_get_modified (doc));

    moo_markup_set_prop (root, "special", "a&b<\"");
    serialized = moo_markup_node_get_string (MOO_MARKUP_NODE (doc));
    g_assert_cmpstr (serialized, ==,
                     "<root special=\"a&amp;b&lt;&quot;\"><child name=\"value\">value</child></root>");
    g_free (serialized);

    moo_markup_set_content (child, "");
    g_assert_cmpstr (moo_markup_get_content (child), ==, "");
    moo_markup_set_content (child, NULL);
    g_assert_null (moo_markup_get_content (child));

    _moo_markup_set_modified (doc, FALSE);
    moo_markup_delete_node (child);
    g_assert_true (_moo_markup_get_modified (doc));
    g_assert_null (moo_markup_get_element (root, "child"));

    moo_markup_doc_unref (doc);
}


static void
test_markup_property_edges (void)
{
    MooMarkupDoc *doc = moo_markup_doc_new ("memory");
    MooMarkupNode *root = moo_markup_create_root_element (doc, "root");

    g_assert_null (moo_markup_get_prop (root, "missing"));
    moo_markup_set_prop (root, "first", "one");
    moo_markup_set_prop (root, "second", "two");
    moo_markup_set_prop (root, "first", NULL);
    g_assert_null (moo_markup_get_prop (root, "first"));
    g_assert_cmpstr (moo_markup_get_prop (root, "second"), ==, "two");
    moo_markup_set_prop (root, "second", NULL);
    g_assert_null (moo_markup_get_prop (root, "second"));

    moo_markup_doc_unref (doc);
}


static void
test_markup_type_edges (void)
{
    MooMarkupDoc *doc = moo_markup_doc_new ("memory");
    MooMarkupNode *root = moo_markup_create_root_element (doc, "root");

    moo_markup_set_prop (root, "int", "-42");
    moo_markup_set_prop (root, "uint", "42");
    moo_markup_set_prop (root, "true", "YeS");
    moo_markup_set_prop (root, "false", "0");
    moo_markup_set_prop (root, "invalid", "not-a-number");

    g_assert_cmpint (moo_markup_int_prop (root, "int", 7), ==, -42);
    g_assert_cmpuint (moo_markup_uint_prop (root, "uint", 7), ==, 42);
    g_assert_true (moo_markup_bool_prop (root, "true", FALSE));
    g_assert_false (moo_markup_bool_prop (root, "false", TRUE));
    g_assert_cmpint (moo_markup_int_prop (root, "missing", 7), ==, 7);

    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*could not convert*");
    g_assert_cmpint (moo_markup_uint_prop (root, "invalid", 7), ==, 7);
    g_test_assert_expected_messages ();

    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*could not convert*");
    g_assert_true (moo_markup_bool_prop (root, "invalid", TRUE));
    g_test_assert_expected_messages ();

    moo_markup_doc_unref (doc);
}


static void
test_markup_sibling_order (void)
{
    MooMarkupDoc *doc = moo_markup_doc_new ("memory");
    MooMarkupNode *root = moo_markup_create_root_element (doc, "root");
    MooMarkupNode *first = moo_markup_create_element (root, "first");
    MooMarkupNode *second = moo_markup_create_element (root, "second");
    char *serialized;

    moo_markup_delete_node (first);
    g_assert_nonnull (moo_markup_create_element (root, "replacement"));

    serialized = moo_markup_node_get_string (MOO_MARKUP_NODE (doc));
    g_assert_cmpstr (serialized, ==, "<root><second/><replacement/></root>");
    g_free (serialized);

    moo_markup_delete_node (second);
    serialized = moo_markup_node_get_string (MOO_MARKUP_NODE (doc));
    g_assert_cmpstr (serialized, ==, "<root><replacement/></root>");
    g_free (serialized);
    moo_markup_doc_unref (doc);
}


static void
test_markup_nested_creation (void)
{
    MooMarkupDoc *doc = moo_markup_doc_new ("memory");
    MooMarkupNode *root = moo_markup_create_root_element (doc, "root");
    char *serialized;

    g_assert_nonnull (moo_markup_create_element (root, "menu/items/item"));
    g_assert_nonnull (moo_markup_create_element (root, "menu/items/item"));

    serialized = moo_markup_node_get_string (MOO_MARKUP_NODE (doc));
    g_assert_cmpstr (serialized, ==,
                     "<root><menu><items><item/><item/></items></menu></root>");
    g_free (serialized);
    moo_markup_doc_unref (doc);
}


static void
test_markup_round_trip_edges (void)
{
    GError *error = NULL;
    MooMarkupDoc *doc;
    MooMarkupNode *root;
    MooMarkupNode *child;
    char *serialized;
    MooMarkupDoc *round_trip;
    MooMarkupNode *round_child;
    const char *round_content;

    doc = moo_markup_parse_memory (
        "<root><![CDATA[a < b & c]]><!-- note --><empty/>"
        "<child>one<![CDATA[ & two]]></child></root>", -1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (doc);

    root = moo_markup_get_root_element (doc, "root");
    child = moo_markup_get_element (root, "child");
    g_assert_cmpstr (moo_markup_get_content (root), ==, "a < b & c");
    g_assert_cmpstr (moo_markup_get_content (child), ==, "one & two");

    serialized = moo_markup_node_get_string (MOO_MARKUP_NODE (doc));
    g_assert_cmpstr (serialized, ==,
                     "<root>a &lt; b &amp; c<!-- note --><empty/>"
                     "<child>one &amp; two</child></root>");

    round_trip = moo_markup_parse_memory (serialized, -1, &error);
    g_assert_no_error (error);
    g_assert_nonnull (round_trip);
    round_child = moo_markup_get_element (MOO_MARKUP_NODE (round_trip),
                                          "root/child");
    round_content = moo_markup_get_content (round_child);
    g_assert_cmpstr (round_content, ==, "one & two");

    moo_markup_doc_unref (round_trip);
    g_free (serialized);
    moo_markup_doc_unref (doc);

    doc = moo_markup_parse_memory ("<root><!-- invalid -- comment --></root>",
                                   -1, &error);
    g_assert_null (doc);
    g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE);
    g_clear_error (&error);
}


static void
test_ui_xml_memory (void)
{
    MooUiXml *xml;
    MooUiNode *node;
    guint merge_id;
    char *path;

    xml = moo_ui_xml_new ();
    moo_ui_xml_add_ui_from_string (
        xml,
        "<ui><object name=\"main\"><widget name=\"menu\">"
        "<item name=\"open\" action=\"Open\"/>"
        "<placeholder name=\"slots\"/>"
        "</widget></object></ui>",
        -1);

    node = moo_ui_xml_get_node (xml, "main/menu/open");
    g_assert_nonnull (node);
    path = moo_ui_node_get_path (node);
    g_assert_cmpstr (path, ==, "main/menu/open");
    g_free (path);
    g_assert_nonnull (moo_ui_xml_find_placeholder (xml, "slots"));

    merge_id = moo_ui_xml_new_merge_id (xml);
    node = moo_ui_xml_add_item (xml, merge_id, "main/menu", NULL, "Save", -1);
    g_assert_nonnull (node);
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/Save"));

    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*can't add item*");
    node = moo_ui_xml_add_item (xml, merge_id, "main", "invalid", "Invalid", -1);
    g_test_assert_expected_messages ();
    g_assert_null (node);
    g_assert_null (moo_ui_xml_get_node (xml, "main/invalid"));

    moo_ui_xml_insert_markup_before (xml, merge_id, "main/menu", "open",
                                     "<item name=\"before\" action=\"Before\"/>"
                                     "<item name=\"before-two\" action=\"BeforeTwo\"/>");
    moo_ui_xml_insert_markup_after (xml, merge_id, "main/menu", "open",
                                    "<item name=\"after\" action=\"After\"/>");
    moo_ui_xml_insert_markup (xml, merge_id, "main/menu", 1,
                              "<item name=\"middle\" action=\"Middle\"/>"
                              "<item name=\"middle-two\" action=\"MiddleTwo\"/>");

    node = moo_ui_xml_get_node (xml, "main/menu");
    {
        const char *expected[] = {
            "before", "middle", "middle-two", "before-two", "open", "after", "slots", "Save"
        };
        GSList *children = node->children;

        for (guint i = 0; i < G_N_ELEMENTS (expected); ++i)
        {
            g_assert_nonnull (children);
            g_assert_cmpstr (((MooUiNode*) children->data)->name, ==, expected[i]);
            children = children->next;
        }
        g_assert_null (children);
    }

    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/before"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/middle"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/middle-two"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/before-two"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/open"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/after"));

    moo_ui_xml_remove_ui (xml, merge_id);
    g_assert_null (moo_ui_xml_get_node (xml, "main/menu/Save"));
    g_assert_null (moo_ui_xml_get_node (xml, "main/menu/before"));
    g_assert_null (moo_ui_xml_get_node (xml, "main/menu/middle"));
    g_assert_null (moo_ui_xml_get_node (xml, "main/menu/after"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/open"));
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main/menu/slots"));

    g_object_unref (xml);
}


static void
test_output_filter_memory (void)
{
    MooOutputFilter *filter;
    MooFileLineData *data;
    MooFileLineData *copy;
    char *dirs[] = { (char*) "src", (char*) "tests", NULL };
    const char * const *active_dirs;

    filter = MOO_OUTPUT_FILTER (g_object_new (MOO_TYPE_OUTPUT_FILTER, NULL));

    g_assert_false (moo_output_filter_stdout_line (filter, "stdout"));
    g_assert_false (moo_output_filter_stderr_line (filter, "stderr"));
    g_assert_false (moo_output_filter_cmd_exit (filter, 0));

    moo_output_filter_cmd_start (filter, "project");
    active_dirs = moo_output_filter_get_active_dirs (filter);
    g_assert_nonnull (active_dirs);
    g_assert_cmpstr (active_dirs[0], ==, "project");
    g_assert_null (active_dirs[1]);

    moo_output_filter_add_active_dirs (filter, dirs);
    active_dirs = moo_output_filter_get_active_dirs (filter);
    g_assert_cmpstr (active_dirs[0], ==, "project");
    g_assert_cmpstr (active_dirs[1], ==, "src");
    g_assert_cmpstr (active_dirs[2], ==, "tests");
    g_assert_null (active_dirs[3]);

    moo_output_filter_set_active_file (filter, "main.c");
    g_assert_cmpstr (moo_output_filter_get_active_file (filter), ==, "main.c");
    moo_output_filter_set_active_file (filter, "other.c");
    g_assert_cmpstr (moo_output_filter_get_active_file (filter), ==, "other.c");
    moo_output_filter_set_active_file (filter, NULL);
    g_assert_null (moo_output_filter_get_active_file (filter));

    data = moo_file_line_data_new ("main.c", 12, 3);
    copy = (MooFileLineData*) g_boxed_copy (MOO_TYPE_FILE_LINE_DATA, data);
    g_assert_nonnull (copy);
    g_assert_cmpstr (copy->file, ==, "main.c");
    g_assert_cmpint (copy->line, ==, 12);
    g_assert_cmpint (copy->character, ==, 3);
    moo_file_line_data_free (copy);
    moo_file_line_data_free (data);

    data = moo_file_line_data_new (NULL, -1, -1);
    g_assert_nonnull (data);
    g_assert_null (data->file);
    g_assert_cmpint (data->line, ==, -1);
    g_assert_cmpint (data->character, ==, -1);
    moo_file_line_data_free (data);

    data = moo_file_line_data_new ("", 0, 0);
    g_assert_nonnull (data);
    g_assert_null (data->file);
    copy = (MooFileLineData*) g_boxed_copy (MOO_TYPE_FILE_LINE_DATA, data);
    g_assert_nonnull (copy);
    g_assert_null (copy->file);
    copy->line = 7;
    copy->character = 8;
    g_assert_cmpint (data->line, ==, 0);
    g_assert_cmpint (data->character, ==, 0);
    moo_file_line_data_free (copy);
    moo_file_line_data_free (data);

    g_object_unref (filter);
}


typedef struct {
    guint stdout_lines;
    guint stderr_lines;
    guint starts;
    guint exits;
    int   last_status;
} OutputFilterSignals;


static gboolean
output_filter_stdout_line (MooOutputFilter *filter,
                           const char      *line,
                           OutputFilterSignals *signals)
{
    g_assert_true (MOO_IS_OUTPUT_FILTER (filter));
    g_assert_cmpstr (line, ==, "out");
    ++signals->stdout_lines;
    return TRUE;
}


static gboolean
output_filter_stderr_line (MooOutputFilter *filter,
                           const char      *line,
                           OutputFilterSignals *signals)
{
    g_assert_true (MOO_IS_OUTPUT_FILTER (filter));
    g_assert_cmpstr (line, ==, "err");
    ++signals->stderr_lines;
    return TRUE;
}


static void
output_filter_cmd_start (MooOutputFilter *filter,
                         OutputFilterSignals *signals)
{
    g_assert_true (MOO_IS_OUTPUT_FILTER (filter));
    ++signals->starts;
}


static gboolean
output_filter_cmd_exit (MooOutputFilter *filter,
                        int              status,
                        OutputFilterSignals *signals)
{
    g_assert_true (MOO_IS_OUTPUT_FILTER (filter));
    signals->last_status = status;
    ++signals->exits;
    return TRUE;
}


static void
test_output_filter_signals (void)
{
    MooOutputFilter *filter;
    OutputFilterSignals signals = { 0, 0, 0, 0, 0 };

    filter = MOO_OUTPUT_FILTER (g_object_new (MOO_TYPE_OUTPUT_FILTER, NULL));
    g_signal_connect (filter, "stdout-line",
                      G_CALLBACK (output_filter_stdout_line), &signals);
    g_signal_connect (filter, "stderr-line",
                      G_CALLBACK (output_filter_stderr_line), &signals);
    g_signal_connect (filter, "cmd-start",
                      G_CALLBACK (output_filter_cmd_start), &signals);
    g_signal_connect (filter, "cmd-exit",
                      G_CALLBACK (output_filter_cmd_exit), &signals);

    g_assert_true (moo_output_filter_stdout_line (filter, "out"));
    g_assert_true (moo_output_filter_stderr_line (filter, "err"));
    moo_output_filter_cmd_start (filter, "project");
    g_assert_true (moo_output_filter_cmd_exit (filter, 17));

    g_assert_cmpuint (signals.stdout_lines, ==, 1);
    g_assert_cmpuint (signals.stderr_lines, ==, 1);
    g_assert_cmpuint (signals.starts, ==, 1);
    g_assert_cmpuint (signals.exits, ==, 1);
    g_assert_cmpint (signals.last_status, ==, 17);

    g_object_unref (filter);
}


static void
test_output_filter_empty_state (void)
{
    MooOutputFilter *filter;
    char *empty_dirs[] = { NULL };
    const char * const *active_dirs;

    filter = MOO_OUTPUT_FILTER (g_object_new (MOO_TYPE_OUTPUT_FILTER, NULL));
    moo_output_filter_cmd_start (filter, "project");
    moo_output_filter_add_active_dirs (filter, empty_dirs);
    moo_output_filter_add_active_dirs (filter, NULL);
    active_dirs = moo_output_filter_get_active_dirs (filter);
    g_assert_nonnull (active_dirs);
    g_assert_cmpstr (active_dirs[0], ==, "project");
    g_assert_null (active_dirs[1]);

    moo_output_filter_cmd_start (filter, NULL);
    active_dirs = moo_output_filter_get_active_dirs (filter);
    g_assert_nonnull (active_dirs);
    g_assert_null (active_dirs[0]);

    moo_output_filter_cmd_start (filter, "other");
    active_dirs = moo_output_filter_get_active_dirs (filter);
    g_assert_cmpstr (active_dirs[0], ==, "other");
    g_assert_null (active_dirs[1]);

    g_object_unref (filter);
}


static void
test_ui_xml_rejects_invalid_nodes (void)
{
    MooUiXml *xml;

    xml = moo_ui_xml_new ();
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*unknown element*");
    moo_ui_xml_add_ui_from_string (xml, "<ui><unknown name=\"bad\"/></ui>", -1);
    g_test_assert_expected_messages ();
    g_assert_null (moo_ui_xml_get_node (xml, "bad"));

    moo_ui_xml_add_ui_from_string (xml, "<ui><object name=\"main\"/></ui>", -1);
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main"));

    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*implement me*");
    moo_ui_xml_add_ui_from_string (xml, "<ui><object name=\"main\"/></ui>", -1);
    g_test_assert_expected_messages ();
    g_assert_nonnull (moo_ui_xml_get_node (xml, "main"));

    g_object_unref (xml);
}


static void
test_prefs_memory (void)
{
    GSList *keys;
    GFile *file;
    char *key;

    moo_prefs_new_key_bool ("unit/prefs/bool", TRUE);
    moo_prefs_new_key_int ("unit/prefs/int", 7);
    moo_prefs_new_key_string ("unit/prefs/string", "default");

    g_assert_true (moo_prefs_key_registered ("unit/prefs/bool"));
    g_assert_cmpuint (moo_prefs_get_key_type ("unit/prefs/int"), ==, G_TYPE_INT);
    g_assert_true (moo_prefs_get_bool ("unit/prefs/bool"));
    g_assert_cmpint (moo_prefs_get_int ("unit/prefs/int"), ==, 7);
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/string"), ==, "default");
    g_test_expect_message ("Moo", G_LOG_LEVEL_WARNING, "*not registered*");
    g_test_expect_message ("Moo", G_LOG_LEVEL_CRITICAL, "*val != NULL*");
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/missing"), ==, NULL);
    g_test_assert_expected_messages ();

    moo_prefs_set_bool ("unit/prefs/bool", FALSE);
    moo_prefs_set_int ("unit/prefs/int", -12);
    moo_prefs_set_string ("unit/prefs/string", "changed");
    g_assert_false (moo_prefs_get_bool ("unit/prefs/bool"));
    g_assert_cmpint (moo_prefs_get_int ("unit/prefs/int"), ==, -12);
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/string"), ==, "changed");

    file = g_file_new_for_uri ("file:///tmp/unit-prefs.txt");
    moo_prefs_new_key_string ("unit/prefs/file", NULL);
    moo_prefs_set_file ("unit/prefs/file", file);
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/file"), ==,
                     "file:///tmp/unit-prefs.txt");
    g_object_unref (moo_prefs_get_file ("unit/prefs/file"));
    g_object_unref (file);

    key = moo_prefs_make_key ("unit", "prefs", "composed", nullptr);
    g_assert_cmpstr (key, ==, "unit/prefs/composed");
    g_free (key);

    keys = moo_prefs_list_keys (MOO_PREFS_RC);
    g_assert_nonnull (g_slist_find_custom (keys, "unit/prefs/bool", (GCompareFunc) strcmp));
    g_assert_nonnull (g_slist_find_custom (keys, "unit/prefs/file", (GCompareFunc) strcmp));
    g_slist_free_full (keys, g_free);

    moo_prefs_delete_key ("unit/prefs/bool");
    g_assert_false (moo_prefs_key_registered ("unit/prefs/bool"));
    moo_prefs_delete_key ("unit/prefs/int");
    moo_prefs_delete_key ("unit/prefs/string");
    moo_prefs_delete_key ("unit/prefs/file");
}


static void
test_prefs_overwrite (void)
{
    moo_prefs_new_key_string ("unit/prefs/overwrite", "initial");

    moo_prefs_set_string ("unit/prefs/overwrite", "first");
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/overwrite"), ==, "first");

    moo_prefs_set_string ("unit/prefs/overwrite", "second");
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/overwrite"), ==, "second");

    moo_prefs_delete_key ("unit/prefs/overwrite");
}


static void
test_prefs_delete_reregister (void)
{
    moo_prefs_new_key_string ("unit/prefs/re-register", "first");
    moo_prefs_set_string ("unit/prefs/re-register", "changed");
    moo_prefs_delete_key ("unit/prefs/re-register");
    g_assert_false (moo_prefs_key_registered ("unit/prefs/re-register"));

    moo_prefs_new_key_string ("unit/prefs/re-register", "second");
    g_assert_true (moo_prefs_key_registered ("unit/prefs/re-register"));
    g_assert_cmpstr (moo_prefs_get_string ("unit/prefs/re-register"), ==, "second");
    g_assert_cmpstr (g_value_get_string (moo_prefs_get_default ("unit/prefs/re-register")),
                     ==, "second");

    moo_prefs_delete_key ("unit/prefs/re-register");
}


static void
test_path_utilities (void)
{
    GError *error = NULL;
    char *uri;
    char *path;
    char *normalized;

    g_assert_true (_moo_path_is_absolute ("/tmp/file"));
    g_assert_false (_moo_path_is_absolute ("relative/file"));

    normalized = _moo_normalize_file_path ("/tmp/a/../b/./c");
    g_assert_cmpstr (normalized, ==, "/tmp/b/c");
    g_free (normalized);

    normalized = _moo_normalize_file_path ("/tmp/..");
    g_assert_cmpstr (normalized, ==, "/");
    g_free (normalized);

    uri = _moo_filename_to_uri ("/tmp/a file/Ð¿ÑÐ¸Ð²ÐµÑ.txt", &error);
    g_assert_no_error (error);
    g_assert_nonnull (uri);
    path = g_filename_from_uri (uri, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpstr (path, ==, "/tmp/a file/Ð¿ÑÐ¸Ð²ÐµÑ.txt");

    g_free (path);
    g_free (uri);

    uri = _moo_filename_to_uri ("/tmp/a#b% c.txt", &error);
    g_assert_no_error (error);
    g_assert_nonnull (uri);
    path = g_filename_from_uri (uri, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpstr (path, ==, "/tmp/a#b% c.txt");
    g_free (path);
    g_free (uri);
}


static void
test_path_boundaries (void)
{
    GError *error = NULL;
    char *cwd = g_get_current_dir ();
    char *uri;
    char *path;
    char *normalized;
    char *expected;

    normalized = _moo_normalize_file_path ("");
    g_assert_cmpstr (normalized, ==, "");
    g_free (normalized);

    normalized = _moo_normalize_file_path (".");
    g_assert_cmpstr (normalized, ==, cwd);
    g_free (normalized);

    normalized = _moo_normalize_file_path ("/tmp/a///");
    g_assert_cmpstr (normalized, ==, "/tmp/a");
    g_free (normalized);

    normalized = _moo_normalize_file_path ("a///b/../../c/");
    expected = g_build_filename (cwd, "c", nullptr);
    g_assert_cmpstr (normalized, ==, expected);
    g_free (expected);
    g_free (normalized);

    uri = _moo_filename_to_uri ("relative file.txt", &error);
    g_assert_no_error (error);
    g_assert_nonnull (uri);
    path = g_filename_from_uri (uri, NULL, &error);
    g_assert_no_error (error);
    expected = g_build_filename (cwd, "relative file.txt", nullptr);
    g_assert_cmpstr (path, ==, expected);
    g_free (expected);

    g_free (path);
    g_free (uri);
    g_free (cwd);
}


static guint history_changed_count;


static void
history_changed (MooHistoryList *list)
{
    g_assert_true (MOO_IS_HISTORY_LIST (list));
    ++history_changed_count;
}


static void
count_notify (guint *counter)
{
    ++*counter;
}


static void
test_history_list_memory (void)
{
    MooHistoryList *list = moo_history_list_new (NULL);
    MooHistoryListItem *item;
    MooHistoryListItem *copy;
    GtkTreeIter iter;
    char *last;

    g_signal_connect (list, "changed", G_CALLBACK (history_changed), NULL);
    history_changed_count = 0;

    item = moo_history_list_item_new ("data", "display", TRUE);
    copy = moo_history_list_item_copy (item);
    g_assert_nonnull (copy);
    g_assert_cmpstr (copy->data, ==, "data");
    g_assert_cmpstr (copy->display, ==, "display");
    g_assert_true (copy->builtin);
    g_free (copy->data);
    copy->data = g_strdup ("changed");
    g_assert_cmpstr (item->data, ==, "data");
    moo_history_list_item_free (copy);
    moo_history_list_item_free (item);

    g_assert_true (moo_history_list_is_empty (list));
    moo_history_list_add (list, "one");
    g_assert_cmpuint (history_changed_count, ==, 1);
    moo_history_list_add (list, "two");
    moo_history_list_add (list, "one");
    g_assert_cmpuint (history_changed_count, ==, 3);
    g_assert_false (moo_history_list_is_empty (list));
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);

    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "one");
    g_free (last);

    g_assert_true (moo_history_list_find (list, "two", &iter));
    g_assert_false (moo_history_list_find (list, "missing", &iter));

    moo_history_list_set_max_entries (list, 2);
    moo_history_list_add (list, "three");
    g_assert_cmpuint (history_changed_count, ==, 4);
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);
    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "three");
    g_free (last);

    moo_history_list_set_max_entries (list, 1);
    g_assert_cmpuint (history_changed_count, ==, 5);
    moo_history_list_add (list, "four");
    g_assert_cmpuint (history_changed_count, ==, 6);
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 1);
    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "four");
    g_free (last);

    moo_history_list_set_max_entries (list, 2);
    moo_history_list_add (list, "two");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);
    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "two");
    g_free (last);

    moo_history_list_add (list, "");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);

    {
        MooHistoryList *many = moo_history_list_new (NULL);

        g_signal_connect (many, "changed", G_CALLBACK (history_changed), NULL);
        history_changed_count = 0;
        moo_history_list_add (many, "one");
        moo_history_list_add (many, "two");
        moo_history_list_add (many, "three");
        history_changed_count = 0;
        moo_history_list_set_max_entries (many, 1);
        g_assert_cmpuint (history_changed_count, ==, 1);
        g_assert_cmpuint (moo_history_list_n_user_entries (many), ==, 1);
        g_object_unref (many);
    }

    g_object_unref (list);
}


static void
test_history_list_clear (void)
{
    MooHistoryList *list = moo_history_list_new (NULL);
    GtkTreeIter iter;
    guint notified = 0;

    g_signal_connect (list, "changed", G_CALLBACK (history_changed), NULL);
    g_signal_connect_swapped (list, "notify::empty",
                              G_CALLBACK (count_notify), &notified);

    /* nothing to forget, so nothing happens */
    history_changed_count = 0;
    moo_history_list_clear (list);
    g_assert_cmpuint (history_changed_count, ==, 0);
    g_assert_cmpuint (notified, ==, 0);

    moo_history_list_add (list, "one");
    moo_history_list_add (list, "two");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);

    history_changed_count = 0;
    notified = 0;
    moo_history_list_clear (list);
    g_assert_cmpuint (history_changed_count, ==, 1);
    g_assert_cmpuint (notified, ==, 1);
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 0);
    g_assert_true (moo_history_list_is_empty (list));
    g_assert_false (moo_history_list_find (list, "one", &iter));
    g_assert_null (moo_history_list_get_last_item (list));
    g_assert_false (gtk_tree_model_get_iter_first (moo_history_list_get_model (list), &iter));

    /* and the list still works afterwards */
    moo_history_list_add (list, "three");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 1);
    g_assert_true (moo_history_list_find (list, "three", &iter));

    g_object_unref (list);
}


/* The saved copy of the history goes with it: what is cleared must not come
   back on the next run. */
static void
test_history_list_clear_prefs (void)
{
    MooHistoryList *list = moo_history_list_new ("unit/history/find");

    moo_history_list_add (list, "one");
    moo_history_list_add (list, "two");
    g_assert_nonnull (moo_markup_get_element (moo_prefs_get_markup (MOO_PREFS_STATE),
                                              "unit/history/find/recent-items"));

    moo_history_list_clear (list);
    g_assert_null (moo_markup_get_element (moo_prefs_get_markup (MOO_PREFS_STATE),
                                           "unit/history/find/recent-items"));

    g_object_unref (list);
}


static void
test_history_list_limit_noop (void)
{
    MooHistoryList *list = moo_history_list_new (NULL);

    history_changed_count = 0;
    g_signal_connect (list, "changed", G_CALLBACK (history_changed), NULL);
    moo_history_list_set_max_entries (list, 1);
    g_assert_cmpuint (history_changed_count, ==, 0);
    g_assert_true (moo_history_list_is_empty (list));
    g_object_unref (list);
}


#if GTK_CHECK_VERSION(3,0,0)
/* -------------------------------------------------------------------------
 * The shape of the drop indicator
 *
 * While a pane is being carried to another edge of the window, MooBigPaned
 * shows a shaped window over the place it would land: an outline around the
 * drop area and another around the button that would carry it. The shape is
 * the whole of it -- the window has nothing in it but two frames -- so
 * whether it is a frame or a slab is decided here rather than by anything
 * drawn.
 */

static void
test_drop_mask (void)
{
    GdkRectangle button = {20, 30, 40, 10};
    cairo_region_t *mask = _moo_big_paned_drop_mask (200, 100, &button);

    /* Two pixels of border around the whole thing, and the inside of it
       left alone -- what is under the indicator has to stay visible. */
    g_assert_true (cairo_region_contains_point (mask, 0, 0));
    g_assert_true (cairo_region_contains_point (mask, 1, 1));
    g_assert_false (cairo_region_contains_point (mask, 2, 2));
    g_assert_true (cairo_region_contains_point (mask, 199, 99));
    g_assert_false (cairo_region_contains_point (mask, 100, 50));

    /* And the same around the button, whose far edge is at x + width and
       y + height: the GTK+2 call this replaced outlined through them. */
    g_assert_true (cairo_region_contains_point (mask, 20, 30));
    g_assert_true (cairo_region_contains_point (mask, 21, 31));
    g_assert_false (cairo_region_contains_point (mask, 22, 32));
    g_assert_true (cairo_region_contains_point (mask, 60, 40));
    g_assert_false (cairo_region_contains_point (mask, 40, 35));

    cairo_region_destroy (mask);
}
#endif


/* -------------------------------------------------------------------------
 * MooFileWriter, which the removed moo_test_moo_file_writer() covered
 */

static char *
temp_dir (void)
{
    GError *error = NULL;
    char *dir = g_dir_make_tmp ("medit-unit-XXXXXX", &error);

    g_assert_no_error (error);
    g_assert_nonnull (dir);

    return dir;
}


static void
test_file_writer (void)
{
    char *dir = temp_dir ();
    char *path = g_build_filename (dir, "written.txt", nullptr);
    char *back = NULL;
    GError *error = NULL;
    MooFileWriter *writer;

    writer = moo_file_writer_new (path, MooFileWriterFlags (0), &error);
    g_assert_no_error (error);
    g_assert_nonnull (writer);

    g_assert_true (moo_file_writer_write (writer, "alpha\n", -1));
    g_assert_true (moo_file_writer_printf (writer, "%s %d\n", "beta", 42));
    g_assert_true (moo_file_writer_close (writer, &error));
    g_assert_no_error (error);

    g_assert_true (g_file_get_contents (path, &back, NULL, &error));
    g_assert_no_error (error);
    g_assert_cmpstr (back, ==, "alpha\nbeta 42\n");

    g_free (back);

    /* Written again, with a backup this time: the new content is in place and
       the old one is beside it. */
    writer = moo_file_writer_new (path, MOO_FILE_WRITER_SAVE_BACKUP, &error);
    g_assert_no_error (error);
    g_assert_nonnull (writer);

    g_assert_true (moo_file_writer_write (writer, "gamma\n", -1));
    g_assert_true (moo_file_writer_close (writer, &error));
    g_assert_no_error (error);

    g_assert_true (g_file_get_contents (path, &back, NULL, &error));
    g_assert_cmpstr (back, ==, "gamma\n");
    g_free (back);

    {
        char *backup = g_strdup_printf ("%s~", path);

        g_assert_true (g_file_get_contents (backup, &back, NULL, &error));
        g_assert_cmpstr (back, ==, "alpha\nbeta 42\n");

        g_remove (backup);
        g_free (backup);
        g_free (back);
    }

    g_remove (path);
    g_rmdir (dir);

    g_free (path);
    g_free (dir);
}


static void
test_line_reader_edges (void)
{
    static const char text[] = "one\r\ntwo\rthree\nfour\xe2\x80\xa9" "five";
    static const gsize lengths[] = { 3, 3, 5, 4, 4 };
    static const gsize terminators[] = { 2, 1, 1, 3, 0 };
    MooLineReader reader;
    const char *line;
    gsize line_len, lt_len;
    guint i;

    moo_line_reader_init (&reader, text, sizeof text - 1);
    for (i = 0; i < G_N_ELEMENTS (lengths); ++i)
    {
        line = moo_line_reader_get_line (&reader, &line_len, &lt_len);
        g_assert_nonnull (line);
        g_assert_cmpuint (line_len, ==, lengths[i]);
        g_assert_cmpuint (lt_len, ==, terminators[i]);
    }
    g_assert_null (moo_line_reader_get_line (&reader, &line_len, &lt_len));

    g_assert_true (moo_find_line_end ("abc\r\ndef", 7, &line_len, &lt_len));
    g_assert_cmpuint (line_len, ==, 3);
    g_assert_cmpuint (lt_len, ==, 2);
    g_assert_false (moo_find_line_end ("abc", 3, &line_len, &lt_len));
    g_assert_cmpuint (line_len, ==, 3);
    g_assert_cmpuint (lt_len, ==, 0);
}


static void
test_strv_reverse (void)
{
    char **strv = g_new (char *, 4);

    strv[0] = g_strdup ("one");
    strv[1] = g_strdup ("two");
    strv[2] = g_strdup ("three");
    strv[3] = NULL;
    g_assert_true (_moo_strv_reverse (strv) == strv);
    g_assert_cmpstr (strv[0], ==, "three");
    g_assert_cmpstr (strv[1], ==, "two");
    g_assert_cmpstr (strv[2], ==, "one");
    g_strfreev (strv);
}


static void
test_value_and_data_memory (void)
{
    GValue source = G_VALUE_INIT;
    GValue converted = G_VALUE_INIT;
    GValue stored = G_VALUE_INIT;
    MooData *data;
    static const char key[] = "answer";

    g_value_init (&source, G_TYPE_INT);
    g_value_set_int (&source, 42);
    g_value_init (&converted, G_TYPE_STRING);
    g_assert_true (_moo_value_convert (&source, &converted));
    g_assert_cmpstr (g_value_get_string (&converted), ==, "42");
    g_assert_true (_moo_value_equal (&source, &source));
    g_assert_true (_moo_value_change_type (&source, G_TYPE_STRING));
    g_assert_cmpstr (g_value_get_string (&source), ==, "42");
    g_assert_true (_moo_value_convert_from_string ("TRUE", &converted));
    g_assert_cmpstr (g_value_get_string (&converted), ==, "TRUE");
    g_assert_true (_moo_convert_string_to_bool ("true", FALSE));
    g_assert_cmpint (_moo_convert_string_to_int ("-7", 0), ==, -7);
    g_assert_cmpuint (_moo_convert_string_to_uint ("9", 0), ==, 9);
    g_assert_cmpstr (_moo_convert_bool_to_string (FALSE), ==, "FALSE");

    data = _moo_data_new (g_str_hash, g_str_equal, NULL);
    g_assert_nonnull (data);
    _moo_data_insert_value (data, (gpointer) key, &source);
    g_assert_true (_moo_data_get_value (data, (gpointer) key, &stored));
    g_assert_cmpstr (g_value_get_string (&stored), ==, "42");
    g_value_unset (&stored);
    _moo_data_remove (data, (gpointer) key);
    g_assert_false (_moo_data_get_value (data, (gpointer) key, NULL));
    _moo_data_unref (data);
    g_value_unset (&source);
    g_value_unset (&converted);
}


/* -------------------------------------------------------------------------
 * _moo_value_convert(): the string a prefs value is stored as, and back
 */

/*
 * The two directions of the conversion are not symmetric -- a boolean goes out
 * as "TRUE" but comes back from any of 1/yes/true, an enum goes out as its
 * nick but comes back by name, by nick or as a number, flags go out as a
 * number but come back from a "first|second" list -- so each direction is
 * checked on its own rather than by round-tripping values through both.
 */

/*
 * MooUiNodeFlags has a single member, which cannot exercise the list the
 * reader accepts, so the flags cases use a type registered here.
 */
static GType
test_convert_flags_type (void)
{
    static GType type;

    if (G_UNLIKELY (!type))
    {
        static const GFlagsValue values[] = {
            { 1 << 0, (char*) "MOO_TEST_CONVERT_FIRST", (char*) "first" },
            { 1 << 1, (char*) "MOO_TEST_CONVERT_SECOND", (char*) "second" },
            { 1 << 2, (char*) "MOO_TEST_CONVERT_THIRD", (char*) "third" },
            { 0, NULL, NULL }
        };

        type = g_flags_register_static ("MooTestConvertFlags", values);
    }

    return type;
}

static void
check_to_string (const GValue *src,
                 const char   *expected)
{
    GValue dest = G_VALUE_INIT;

    g_value_init (&dest, G_TYPE_STRING);
    g_assert_true (_moo_value_convert (src, &dest));
    g_assert_cmpstr (g_value_get_string (&dest), ==, expected);
    g_value_unset (&dest);
}

static gboolean
convert_from_string (const char *string,
                     GValue     *dest)
{
    GValue src = G_VALUE_INIT;
    gboolean ok;

    g_value_init (&src, G_TYPE_STRING);
    g_value_set_string (&src, string);
    ok = _moo_value_convert (&src, dest);
    g_value_unset (&src);

    return ok;
}

static void
test_value_to_string (void)
{
    GValue src = G_VALUE_INIT;
    GdkColor color;

    g_value_init (&src, G_TYPE_BOOLEAN);
    g_value_set_boolean (&src, TRUE);
    check_to_string (&src, "TRUE");
    g_value_set_boolean (&src, FALSE);
    check_to_string (&src, "FALSE");
    g_value_unset (&src);

    g_value_init (&src, G_TYPE_INT);
    g_value_set_int (&src, -7);
    check_to_string (&src, "-7");
    g_value_unset (&src);

    g_value_init (&src, G_TYPE_UINT);
    g_value_set_uint (&src, 9);
    check_to_string (&src, "9");
    g_value_unset (&src);

    /* A colour is written as the eight bit "#rrggbb" gdk_color_parse() reads,
       so the low byte of each sixteen bit channel is dropped. */
    g_assert_true (gdk_color_parse ("#123456", &color));
    g_value_init (&src, GDK_TYPE_COLOR);
    g_value_set_boxed (&src, &color);
    check_to_string (&src, "#123456");
    g_value_set_boxed (&src, NULL);
    check_to_string (&src, NULL);
    g_value_unset (&src);

    g_value_init (&src, MOO_TYPE_UI_NODE_TYPE);
    g_value_set_enum (&src, MOO_UI_NODE_PLACEHOLDER);
    check_to_string (&src, "placeholder");
    /* A value the enum does not have is written as the number it is: the enum
       may have grown a member since the file was written, and a number
       survives that where an invented nick would not. */
    g_value_set_enum (&src, 99);
    check_to_string (&src, "99");
    g_value_unset (&src);

    g_value_init (&src, test_convert_flags_type ());
    g_value_set_flags (&src, 5);
    check_to_string (&src, "5");
    g_value_set_flags (&src, 0);
    check_to_string (&src, "");
    g_value_unset (&src);
}

static void
test_value_from_string (void)
{
    GValue dest = G_VALUE_INIT;
    const GdkColor *color;

    /* An absent or empty string is not a parse failure for any type: it is
       what a key that was never written reads as, and every type answers it
       with its own zero. */

    g_value_init (&dest, G_TYPE_BOOLEAN);
    g_assert_true (convert_from_string ("yes", &dest));
    g_assert_true (g_value_get_boolean (&dest));
    g_assert_true (convert_from_string ("TrUe", &dest));
    g_assert_true (g_value_get_boolean (&dest));
    g_assert_true (convert_from_string ("1", &dest));
    g_assert_true (g_value_get_boolean (&dest));
    g_assert_true (convert_from_string ("no", &dest));
    g_assert_false (g_value_get_boolean (&dest));
    g_assert_true (convert_from_string ("", &dest));
    g_assert_false (g_value_get_boolean (&dest));
    g_assert_false (convert_from_string ("maybe", &dest));
    g_value_unset (&dest);

    g_value_init (&dest, G_TYPE_INT);
    g_assert_true (convert_from_string ("-7", &dest));
    g_assert_cmpint (g_value_get_int (&dest), ==, -7);
    g_assert_true (convert_from_string ("", &dest));
    g_assert_cmpint (g_value_get_int (&dest), ==, 0);
    g_assert_false (convert_from_string ("7 apples", &dest));
    g_value_unset (&dest);

    g_value_init (&dest, G_TYPE_UINT);
    g_assert_true (convert_from_string ("9", &dest));
    g_assert_cmpuint (g_value_get_uint (&dest), ==, 9);
    g_assert_false (convert_from_string ("nine", &dest));
    g_value_unset (&dest);

    g_value_init (&dest, G_TYPE_DOUBLE);
    g_assert_true (convert_from_string ("0.5", &dest));
    g_assert_cmpfloat (g_value_get_double (&dest), ==, 0.5);
    g_assert_true (convert_from_string ("", &dest));
    g_assert_cmpfloat (g_value_get_double (&dest), ==, 0.);
    g_assert_false (convert_from_string ("half", &dest));
    g_value_unset (&dest);

    g_value_init (&dest, GDK_TYPE_COLOR);
    g_assert_true (convert_from_string ("#123456", &dest));
    color = (const GdkColor*) g_value_get_boxed (&dest);
    g_assert_nonnull (color);
    g_assert_cmpuint (color->red >> 8, ==, 0x12);
    g_assert_cmpuint (color->green >> 8, ==, 0x34);
    g_assert_cmpuint (color->blue >> 8, ==, 0x56);
    g_assert_true (convert_from_string ("", &dest));
    g_assert_null (g_value_get_boxed (&dest));
    g_value_unset (&dest);

    g_value_init (&dest, MOO_TYPE_UI_NODE_TYPE);
    g_assert_true (convert_from_string ("placeholder", &dest));
    g_assert_cmpint (g_value_get_enum (&dest), ==, MOO_UI_NODE_PLACEHOLDER);
    g_assert_true (convert_from_string ("MOO_UI_NODE_ITEM", &dest));
    g_assert_cmpint (g_value_get_enum (&dest), ==, MOO_UI_NODE_ITEM);
    g_assert_true (convert_from_string ("2", &dest));
    g_assert_cmpint (g_value_get_enum (&dest), ==, 2);
    g_assert_true (convert_from_string ("", &dest));
    g_assert_cmpint (g_value_get_enum (&dest), ==, 0);
    g_assert_false (convert_from_string ("no-such-node", &dest));
    g_value_unset (&dest);

    g_value_init (&dest, test_convert_flags_type ());
    g_assert_true (convert_from_string ("first|third", &dest));
    g_assert_cmpuint (g_value_get_flags (&dest), ==, 5);
    g_assert_true (convert_from_string ("MOO_TEST_CONVERT_SECOND", &dest));
    g_assert_cmpuint (g_value_get_flags (&dest), ==, 2);
    g_assert_true (convert_from_string ("5", &dest));
    g_assert_cmpuint (g_value_get_flags (&dest), ==, 5);
    g_assert_true (convert_from_string ("", &dest));
    g_assert_cmpuint (g_value_get_flags (&dest), ==, 0);
    g_assert_false (convert_from_string ("first|nonsense", &dest));
    g_value_unset (&dest);
}

static void
test_value_convert_number (void)
{
    GValue src = G_VALUE_INIT;
    GValue dest = G_VALUE_INIT;

    /* An enum and a set of flags are an int underneath and cross without
       looking at the class, so a value outside either passes through. */

    g_value_init (&src, MOO_TYPE_UI_NODE_TYPE);
    g_value_set_enum (&src, MOO_UI_NODE_SEPARATOR);
    g_value_init (&dest, G_TYPE_INT);
    g_assert_true (_moo_value_convert (&src, &dest));
    g_assert_cmpint (g_value_get_int (&dest), ==, MOO_UI_NODE_SEPARATOR);
    g_value_unset (&src);
    g_value_unset (&dest);

    g_value_init (&src, G_TYPE_INT);
    g_value_set_int (&src, MOO_UI_NODE_WIDGET);
    g_value_init (&dest, MOO_TYPE_UI_NODE_TYPE);
    g_assert_true (_moo_value_convert (&src, &dest));
    g_assert_cmpint (g_value_get_enum (&dest), ==, MOO_UI_NODE_WIDGET);
    g_value_unset (&dest);

    g_value_init (&dest, test_convert_flags_type ());
    g_value_set_int (&src, 6);
    g_assert_true (_moo_value_convert (&src, &dest));
    g_assert_cmpuint (g_value_get_flags (&dest), ==, 6);
    g_value_unset (&src);

    g_value_init (&src, G_TYPE_INT);
    g_assert_true (_moo_value_convert (&dest, &src));
    g_assert_cmpint (g_value_get_int (&src), ==, 6);
    g_value_unset (&dest);

    /* int <-> double, truncating towards zero the way glib's own transform
       does -- not rounding, and not flooring negatives. */
    g_value_init (&dest, G_TYPE_DOUBLE);
    g_value_set_int (&src, -3);
    g_assert_true (_moo_value_convert (&src, &dest));
    g_assert_cmpfloat (g_value_get_double (&dest), ==, -3.);
    g_value_set_double (&dest, -1.7);
    g_assert_true (_moo_value_convert (&dest, &src));
    g_assert_cmpint (g_value_get_int (&src), ==, -1);
    g_value_set_double (&dest, 1.7);
    g_assert_true (_moo_value_convert (&dest, &src));
    g_assert_cmpint (g_value_get_int (&src), ==, 1);
    g_value_unset (&src);
    g_value_unset (&dest);
}


static void
test_region_polygon_memory (void)
{
    static const GdkPoint points[] = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    MooRegion *region = moo_region_polygon (points, G_N_ELEMENTS (points));

    g_assert_nonnull (region);
    g_assert_true (moo_region_point_in (region, 5, 5));
    g_assert_false (moo_region_point_in (region, 15, 5));
    moo_region_destroy (region);
}


/* -------------------------------------------------------------------------
 * moo_fuzzy_match() -- FuzzyMatchV2 scoring
 */

static void
test_fuzzy_no_match (void)
{
    MooFuzzyMatch match;

    g_assert_false (moo_fuzzy_match ("xyz", "abc", FALSE, &match));
    g_assert_cmpint (match.score, ==, 0);

    /* The letters are all there, just not in order. */
    g_assert_false (moo_fuzzy_match ("ba", "ab", FALSE, &match));
}

static void
test_fuzzy_pattern_longer_than_text (void)
{
    MooFuzzyMatch match;

    g_assert_false (moo_fuzzy_match ("abcd", "abc", FALSE, &match));
}

static void
test_fuzzy_empty_pattern (void)
{
    MooFuzzyMatch match;

    g_assert_true (moo_fuzzy_match ("", "anything", TRUE, &match));
    g_assert_cmpint (match.score, ==, 0);
    g_assert_cmpuint (match.n_positions, ==, 0);
}

static void
test_fuzzy_exact_match (void)
{
    MooFuzzyMatch match;

    g_assert_true (moo_fuzzy_match ("abc", "abc", TRUE, &match));
    g_assert_cmpuint (match.n_positions, ==, 3);
    g_assert_cmpuint (match.positions[0], ==, 0);
    g_assert_cmpuint (match.positions[1], ==, 1);
    g_assert_cmpuint (match.positions[2], ==, 2);
}

static void
test_fuzzy_positions (void)
{
    MooFuzzyMatch match;
    guint i;

    g_assert_true (moo_fuzzy_match ("mev", "moo-environ.h", TRUE, &match));
    g_assert_cmpuint (match.n_positions, ==, 3);
    g_assert_cmpuint (match.positions[0], ==, 0);  /* m */
    g_assert_cmpuint (match.positions[1], ==, 4);  /* e, in "environ" */
    g_assert_cmpuint (match.positions[2], ==, 6);  /* v, the only one */
    g_assert_cmpint (match.score, >, 0);

    for (i = 1; i < match.n_positions; ++i)
        g_assert_cmpuint (match.positions[i], >, match.positions[i - 1]);
}

static void
test_fuzzy_no_positions_when_not_wanted (void)
{
    MooFuzzyMatch match;

    match.n_positions = 99; /* poison: prove it is set to 0, not left alone */
    g_assert_true (moo_fuzzy_match ("abc", "abc", FALSE, &match));
    g_assert_cmpint (match.score, >, 0);
    g_assert_cmpuint (match.n_positions, ==, 0);
}

static void
test_fuzzy_word_boundary_bonus (void)
{
    MooFuzzyMatch boundary, middle;

    g_assert_true (moo_fuzzy_match ("bar", "foo/bar.c", FALSE, &boundary));
    g_assert_true (moo_fuzzy_match ("bar", "foobar.c", FALSE, &middle));

    /* Starting right after a path separator outscores the same three
       letters glued onto the end of another word. */
    g_assert_cmpint (boundary.score, >, middle.score);
}

static void
test_fuzzy_consecutive_bonus (void)
{
    MooFuzzyMatch consecutive, scattered;

    g_assert_true (moo_fuzzy_match ("abc", "abcxyz", FALSE, &consecutive));
    g_assert_true (moo_fuzzy_match ("abc", "axbxcx", FALSE, &scattered));

    g_assert_cmpint (consecutive.score, >, scattered.score);
}

static void
test_fuzzy_smart_case (void)
{
    MooFuzzyMatch match;

    /* All-lowercase pattern: case-insensitive. */
    g_assert_true (moo_fuzzy_match ("moo", "MooEdit.cpp", FALSE, &match));

    /* An uppercase letter in the pattern switches to case-sensitive. */
    g_assert_false (moo_fuzzy_match ("Moo", "moo-edit.cpp", FALSE, &match));
    g_assert_true (moo_fuzzy_match ("Moo", "MooEdit.cpp", FALSE, &match));
}

static void
test_fuzzy_utf8 (void)
{
    MooFuzzyMatch match;

    /* Positions are character offsets, not byte offsets -- every character
       here is two bytes in UTF-8. */
    g_assert_true (moo_fuzzy_match ("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2" /* прив */,
                                    "\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82.txt" /* привет.txt */,
                                    TRUE, &match));
    g_assert_cmpuint (match.n_positions, ==, 4);
    g_assert_cmpuint (match.positions[0], ==, 0);
    g_assert_cmpuint (match.positions[3], ==, 3);
}

static void
test_fuzzy_overflow_protection (void)
{
    g_autofree char *long_pattern = g_strnfill (300, 'a');
    g_autofree char *long_text = g_strnfill (5000, 'a');
    MooFuzzyMatch match;

    g_assert_false (moo_fuzzy_match (long_pattern, "abc", FALSE, &match));
    g_assert_false (moo_fuzzy_match ("a", long_text, FALSE, &match));
}


/* -------------------------------------------------------------------------
 * _moo_find_project_root() -- moved here from the LSP plugin, which was its
 * only caller, when the file-index work needed it too.
 *
 * The markers here are made up rather than the ".git" and "go.mod" a real
 * lsp.xml names, and that is the whole point: the walk goes up from a directory
 * under the temp directory, through it and to the root, so a marker with a real
 * name anywhere above -- a stray /tmp/.git, which is a thing that happens --
 * would be found and the last case here would report that directory instead.
 * The names below cannot be up there.
 */
static void
test_find_project_root (void)
{
    char *dir = temp_dir ();
    char *sub = g_build_filename (dir, "a", "b", nullptr);
    char *marker_path;
    char *markers[] = { (char*) ".medit-unit-root", (char*) "medit-unit-root.mod", NULL };
    char *none[] = { NULL };
    char *root;

    g_assert_cmpint (g_mkdir_with_parents (sub, 0700), ==, 0);
    marker_path = g_build_filename (dir, ".medit-unit-root", nullptr);
    g_assert_true (g_file_set_contents (marker_path, "", 0, NULL));

    /* Two directories down, and the marker at the top: the top is the root. */
    root = _moo_find_project_root (sub, markers, TRUE);
    g_assert_cmpstr (root, ==, dir);
    g_free (root);

    /* The directory holding the marker is its own root. */
    root = _moo_find_project_root (dir, markers, TRUE);
    g_assert_cmpstr (root, ==, dir);
    g_free (root);

    /* No markers at all: the file's own directory, without a walk. */
    root = _moo_find_project_root (sub, none, TRUE);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    root = _moo_find_project_root (sub, NULL, TRUE);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    /* Nothing matches anywhere above, and the walk stops at / rather than
       going round for ever. */
    g_remove (marker_path);
    root = _moo_find_project_root (sub, markers, TRUE);
    g_assert_cmpstr (root, ==, sub);
    g_free (root);

    /* A marker at two levels: outermost keeps climbing and returns the
       higher one -- the case that matters for a submodule's own ".git"
       nested inside a monorepo's -- while outermost=FALSE stops at the
       nearer one, which is what LSP root lookup did before it started
       asking for the outermost too. */
    g_assert_true (g_file_set_contents (marker_path, "", 0, NULL));
    char *mid = g_build_filename (dir, "a", nullptr);
    char *mid_marker_path = g_build_filename (mid, ".medit-unit-root", nullptr);
    g_assert_true (g_file_set_contents (mid_marker_path, "", 0, NULL));

    root = _moo_find_project_root (sub, markers, TRUE);
    g_assert_cmpstr (root, ==, dir);
    g_free (root);

    root = _moo_find_project_root (sub, markers, FALSE);
    g_assert_cmpstr (root, ==, mid);
    g_free (root);

    g_remove (mid_marker_path);
    g_remove (marker_path);
    g_free (mid_marker_path);
    g_free (mid);

    g_rmdir (sub);
    g_free (sub);
    sub = g_build_filename (dir, "a", nullptr);
    g_rmdir (sub);
    g_rmdir (dir);

    g_free (marker_path);
    g_free (sub);
    g_free (dir);
}


/* -------------------------------------------------------------------------
 * MooFileIndex
 */

static gboolean
run_git (const char *cwd, ...)
{
    g_autoptr(GPtrArray) argv = g_ptr_array_new ();
    va_list args;
    const char *arg;
    gboolean ok;
    gint status = 0;
    g_autoptr(GError) error = NULL;

    g_ptr_array_add (argv, (gpointer) "git");

    va_start (args, cwd);
    while ((arg = va_arg (args, const char *)) != NULL)
        g_ptr_array_add (argv, (gpointer) arg);
    va_end (args);

    g_ptr_array_add (argv, NULL);

    ok = g_spawn_sync (cwd, (char **) argv->pdata, NULL,
                       (GSpawnFlags) (G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
                       NULL, NULL, NULL, NULL, &status, &error);

    return ok && g_spawn_check_wait_status (status, NULL);
}

static gboolean
ptr_array_has_string (GPtrArray *array, const char *str)
{
    guint i;

    for (i = 0; i < array->len; ++i)
        if (strcmp ((const char *) g_ptr_array_index (array, i), str) == 0)
            return TRUE;

    return FALSE;
}

static void
test_file_index_git_tracked_and_ignored (void)
{
    g_autofree char *dir = NULL;
    g_autofree char *tracked_path = NULL;
    g_autofree char *untracked_path = NULL;
    g_autofree char *ignored_path = NULL;
    g_autofree char *gitignore_path = NULL;
    g_autoptr(GPtrArray) files = NULL;

    if (!g_find_program_in_path ("git"))
    {
        g_test_skip ("git not installed");
        return;
    }

    dir = temp_dir ();

    g_assert_true (run_git (dir, "init", "-q", NULL));
    g_assert_true (run_git (dir, "config", "user.email", "unit@test", NULL));
    g_assert_true (run_git (dir, "config", "user.name", "unit", NULL));

    tracked_path   = g_build_filename (dir, "tracked.txt", NULL);
    untracked_path = g_build_filename (dir, "untracked.txt", NULL);
    ignored_path   = g_build_filename (dir, "ignored.txt", NULL);
    gitignore_path = g_build_filename (dir, ".gitignore", NULL);

    g_assert_true (g_file_set_contents (tracked_path, "x", 1, NULL));
    g_assert_true (g_file_set_contents (untracked_path, "x", 1, NULL));
    g_assert_true (g_file_set_contents (ignored_path, "x", 1, NULL));
    g_assert_true (g_file_set_contents (gitignore_path, "ignored.txt\n", -1, NULL));

    g_assert_true (run_git (dir, "add", "tracked.txt", ".gitignore", NULL));

    files = _moo_file_index_build (dir, 0);
    g_assert_nonnull (files);
    g_assert_true (ptr_array_has_string (files, "tracked.txt"));
    g_assert_true (ptr_array_has_string (files, "untracked.txt"));
    g_assert_false (ptr_array_has_string (files, "ignored.txt"));

    _moo_remove_dir (dir, TRUE, NULL);
}

static void
test_file_index_no_git_fallback (void)
{
    g_autofree char *dir = temp_dir ();
    g_autofree char *subdir = g_build_filename (dir, "sub", NULL);
    g_autofree char *top_file = g_build_filename (dir, "top.txt", NULL);
    g_autofree char *sub_file = g_build_filename (subdir, "sub.txt", NULL);
    g_autoptr(GPtrArray) files = NULL;

    g_assert_cmpint (g_mkdir_with_parents (subdir, 0700), ==, 0);
    g_assert_true (g_file_set_contents (top_file, "x", 1, NULL));
    g_assert_true (g_file_set_contents (sub_file, "x", 1, NULL));

    files = _moo_file_index_build (dir, 0);
    g_assert_nonnull (files);
    g_assert_cmpuint (files->len, ==, 2);
    g_assert_true (ptr_array_has_string (files, "top.txt"));
    g_assert_true (ptr_array_has_string (files, "sub" G_DIR_SEPARATOR_S "sub.txt"));

    _moo_remove_dir (dir, TRUE, NULL);
}

static void
test_file_index_skip_vcs_dirs (void)
{
    g_autofree char *dir = temp_dir ();
    g_autofree char *git_dir = g_build_filename (dir, ".git", NULL);
    g_autofree char *node_modules_dir = g_build_filename (dir, "node_modules", NULL);
    g_autofree char *own_file = g_build_filename (dir, "own.txt", NULL);
    g_autofree char *git_file = g_build_filename (git_dir, "config", NULL);
    g_autofree char *dep_file = g_build_filename (node_modules_dir, "dep.txt", NULL);
    g_autoptr(GPtrArray) files = NULL;

    /* An empty ".git" is not a working repository, so git ls-files fails
       here and this exercises the fallback walker same as the test above --
       what is new here is that the walker must not descend into it. */
    g_assert_cmpint (g_mkdir_with_parents (git_dir, 0700), ==, 0);
    g_assert_cmpint (g_mkdir_with_parents (node_modules_dir, 0700), ==, 0);
    g_assert_true (g_file_set_contents (own_file, "x", 1, NULL));
    g_assert_true (g_file_set_contents (git_file, "x", 1, NULL));
    g_assert_true (g_file_set_contents (dep_file, "x", 1, NULL));

    files = _moo_file_index_build (dir, 0);
    g_assert_nonnull (files);
    g_assert_cmpuint (files->len, ==, 1);
    g_assert_true (ptr_array_has_string (files, "own.txt"));

    _moo_remove_dir (dir, TRUE, NULL);
}

static void
test_file_index_symlink_not_followed (void)
{
    g_autofree char *dir = temp_dir ();
    g_autofree char *target_dir = g_build_filename (dir, "target", NULL);
    g_autofree char *target_file = g_build_filename (target_dir, "inner.txt", NULL);
    g_autofree char *real_file = g_build_filename (dir, "real.txt", NULL);
    g_autofree char *link_to_dir = g_build_filename (dir, "link-to-target", NULL);
    g_autofree char *link_to_file = g_build_filename (dir, "link-to-real.txt", NULL);
    g_autoptr(GFile) link_to_dir_file = g_file_new_for_path (link_to_dir);
    g_autoptr(GFile) link_to_file_file = g_file_new_for_path (link_to_file);
    g_autoptr(GPtrArray) files = NULL;

    g_assert_cmpint (g_mkdir_with_parents (target_dir, 0700), ==, 0);
    g_assert_true (g_file_set_contents (target_file, "x", 1, NULL));
    g_assert_true (g_file_set_contents (real_file, "x", 1, NULL));
    g_assert_true (g_file_make_symbolic_link (link_to_dir_file, "target", NULL, NULL));
    g_assert_true (g_file_make_symbolic_link (link_to_file_file, "real.txt", NULL, NULL));

    files = _moo_file_index_build (dir, 0);
    g_assert_nonnull (files);
    g_assert_cmpuint (files->len, ==, 2);
    g_assert_true (ptr_array_has_string (files, "real.txt"));
    g_assert_true (ptr_array_has_string (files, "target" G_DIR_SEPARATOR_S "inner.txt"));

    _moo_remove_dir (dir, TRUE, NULL);
}

static void
test_file_index_max_files_limit (void)
{
    g_autofree char *dir = temp_dir ();
    g_autoptr(GPtrArray) files = NULL;
    guint i;

    for (i = 0; i < 5; ++i)
    {
        g_autofree char *name = g_strdup_printf ("file-%u.txt", i);
        g_autofree char *path = g_build_filename (dir, name, NULL);
        g_assert_true (g_file_set_contents (path, "x", 1, NULL));
    }

    files = _moo_file_index_build (dir, 3);
    g_assert_nonnull (files);
    g_assert_cmpuint (files->len, ==, 3);

    _moo_remove_dir (dir, TRUE, NULL);
}

typedef struct {
    GMainLoop *loop;
    GPtrArray *files;
} FileIndexWait;

static void
on_file_index_ready (GPtrArray *files, gpointer user_data)
{
    FileIndexWait *wait = (FileIndexWait *) user_data;
    wait->files = files;
    g_main_loop_quit (wait->loop);
}

static gboolean
on_file_index_wait_timeout (gpointer user_data)
{
    g_main_loop_quit ((GMainLoop *) user_data);
    return G_SOURCE_REMOVE;
}

static void
test_file_index_cache_stale_while_revalidate (void)
{
    g_autofree char *dir = temp_dir ();
    g_autofree char *file_path = g_build_filename (dir, "a.txt", NULL);
    g_autoptr(GMainLoop) loop = g_main_loop_new (NULL, FALSE);
    FileIndexWait wait = { loop, NULL };
    guint timeout_id;

    g_assert_true (g_file_set_contents (file_path, "x", 1, NULL));

    /* Nothing cached yet: NULL right away, the build happens in the background. */
    g_assert_null (_moo_file_index_get (dir, on_file_index_ready, &wait));

    timeout_id = g_timeout_add_seconds (10, on_file_index_wait_timeout, loop);
    g_main_loop_run (loop);
    g_source_remove (timeout_id);

    g_assert_nonnull (wait.files);
    g_assert_cmpuint (wait.files->len, ==, 1);

    /* Fresh in the cache now: the same array comes back with no callback. */
    g_assert_true (_moo_file_index_get (dir, NULL, NULL) == wait.files);

    _moo_file_index_forget (dir);
    g_assert_null (_moo_file_index_get (dir, NULL, NULL));

    _moo_remove_dir (dir, TRUE, NULL);
}


#if GTK_CHECK_VERSION(3,0,0)
static void
test_terminal_color_schemes_memory (void)
{
    const MooTerminalColorScheme *schemes;
    guint n_schemes, i, j;

    schemes = _moo_terminal_color_schemes (&n_schemes);
    g_assert_nonnull (schemes);
    g_assert_cmpuint (n_schemes, >, 1);
    g_assert_true (_moo_terminal_color_scheme_lookup (NULL) == schemes);
    g_assert_true (_moo_terminal_color_scheme_lookup ("") == schemes);
    g_assert_null (_moo_terminal_color_scheme_lookup ("does-not-exist"));

    for (i = 0; i < n_schemes; ++i)
    {
        g_assert_true (_moo_terminal_color_scheme_lookup (schemes[i].name) == &schemes[i]);
        if (schemes[i].colors[0])
            for (j = 0; j < MOO_TERMINAL_SCHEME_N_COLORS; ++j)
                g_assert_nonnull (schemes[i].colors[j]);
    }
}
#endif


void
_moo_add_mooutils_unit_tests (void)
{
    g_test_add_func ("/mooutils/file-line", test_file_line);
    g_test_add_func ("/mooutils/mime/system-behaviour", test_mime_system_behaviour);
    g_test_add_func ("/mooutils/splitlines", test_splitlines);
    g_test_add_func ("/mooutils/accel/parse", test_accel_parse);
    g_test_add_func ("/mooutils/accel/label", test_accel_label_parse);
    g_test_add_func ("/mooutils/markup/memory", test_markup_memory);
    g_test_add_func ("/mooutils/markup/mutation-modified", test_markup_mutation_modified);
    g_test_add_func ("/mooutils/markup/property-edges", test_markup_property_edges);
    g_test_add_func ("/mooutils/markup/type-edges", test_markup_type_edges);
    g_test_add_func ("/mooutils/markup/sibling-order", test_markup_sibling_order);
    g_test_add_func ("/mooutils/markup/nested-creation", test_markup_nested_creation);
    g_test_add_func ("/mooutils/markup/round-trip-edges", test_markup_round_trip_edges);
    g_test_add_func ("/mooutils/ui-xml/memory", test_ui_xml_memory);
    g_test_add_func ("/mooutils/output-filter/memory", test_output_filter_memory);
    g_test_add_func ("/mooutils/output-filter/signals", test_output_filter_signals);
    g_test_add_func ("/mooutils/output-filter/empty-state", test_output_filter_empty_state);
    g_test_add_func ("/mooutils/ui-xml/rejects-invalid-nodes",
                     test_ui_xml_rejects_invalid_nodes);
    g_test_add_func ("/mooutils/prefs/memory", test_prefs_memory);
    g_test_add_func ("/mooutils/prefs/overwrite", test_prefs_overwrite);
    g_test_add_func ("/mooutils/prefs/delete-reregister", test_prefs_delete_reregister);
    g_test_add_func ("/mooutils/value/to-string", test_value_to_string);
    g_test_add_func ("/mooutils/value/from-string", test_value_from_string);
    g_test_add_func ("/mooutils/value/number", test_value_convert_number);
    g_test_add_func ("/mooutils/path/utilities", test_path_utilities);
    g_test_add_func ("/mooutils/path/boundaries", test_path_boundaries);
    g_test_add_func ("/mooutils/history-list/memory", test_history_list_memory);
    g_test_add_func ("/mooutils/history-list/limit-noop", test_history_list_limit_noop);
    g_test_add_func ("/mooutils/history-list/clear", test_history_list_clear);
    g_test_add_func ("/mooutils/history-list/clear-prefs", test_history_list_clear_prefs);
    g_test_add_func ("/mooutils/line-reader/edges", test_line_reader_edges);
    g_test_add_func ("/mooutils/strv/reverse", test_strv_reverse);
    g_test_add_func ("/mooutils/value-and-data/memory", test_value_and_data_memory);
    g_test_add_func ("/mooutils/region/polygon", test_region_polygon_memory);
    g_test_add_func ("/mooutils/fuzzy/no-match", test_fuzzy_no_match);
    g_test_add_func ("/mooutils/fuzzy/pattern-longer-than-text", test_fuzzy_pattern_longer_than_text);
    g_test_add_func ("/mooutils/fuzzy/empty-pattern", test_fuzzy_empty_pattern);
    g_test_add_func ("/mooutils/fuzzy/exact-match", test_fuzzy_exact_match);
    g_test_add_func ("/mooutils/fuzzy/positions", test_fuzzy_positions);
    g_test_add_func ("/mooutils/fuzzy/no-positions-when-not-wanted", test_fuzzy_no_positions_when_not_wanted);
    g_test_add_func ("/mooutils/fuzzy/word-boundary-bonus", test_fuzzy_word_boundary_bonus);
    g_test_add_func ("/mooutils/fuzzy/consecutive-bonus", test_fuzzy_consecutive_bonus);
    g_test_add_func ("/mooutils/fuzzy/smart-case", test_fuzzy_smart_case);
    g_test_add_func ("/mooutils/fuzzy/utf8", test_fuzzy_utf8);
    g_test_add_func ("/mooutils/fuzzy/overflow-protection", test_fuzzy_overflow_protection);
    g_test_add_func ("/mooutils/find-project-root", test_find_project_root);
    g_test_add_func ("/mooutils/file-index/git-tracked-and-ignored", test_file_index_git_tracked_and_ignored);
    g_test_add_func ("/mooutils/file-index/no-git-fallback", test_file_index_no_git_fallback);
    g_test_add_func ("/mooutils/file-index/skip-vcs-dirs", test_file_index_skip_vcs_dirs);
    g_test_add_func ("/mooutils/file-index/symlink-not-followed", test_file_index_symlink_not_followed);
    g_test_add_func ("/mooutils/file-index/max-files-limit", test_file_index_max_files_limit);
    g_test_add_func ("/mooutils/file-index/cache-stale-while-revalidate", test_file_index_cache_stale_while_revalidate);
#if GTK_CHECK_VERSION(3,0,0)
    g_test_add_func ("/mooutils/terminal/colors", test_terminal_color_schemes_memory);
    g_test_add_func ("/mooutils/paned/drop-mask", test_drop_mask);
#endif
    g_test_add_func ("/mooutils/file-writer", test_file_writer);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
