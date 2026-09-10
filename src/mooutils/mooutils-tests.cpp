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

#include "mooutils/mooaccel.h"
#include "mooutils/moobigpaned.h"
#include "mooutils/moofilewriter.h"
#include "mooutils/moomarkup.h"
#include "mooutils/mooprefs.h"
#include "mooutils/moouixml.h"
#include "plugins/support/moooutputfilter.h"
#include "mooutils/moohistorylist.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>


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

    _moo_markup_set_modified (doc, FALSE);
    moo_markup_delete_node (child);
    g_assert_true (_moo_markup_get_modified (doc));
    g_assert_null (moo_markup_get_element (root, "child"));

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


static void
test_history_list_memory (void)
{
    MooHistoryList *list = moo_history_list_new (NULL);
    GtkTreeIter iter;
    char *last;

    g_assert_true (moo_history_list_is_empty (list));
    moo_history_list_add (list, "one");
    moo_history_list_add (list, "two");
    moo_history_list_add (list, "one");
    g_assert_false (moo_history_list_is_empty (list));
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);

    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "one");
    g_free (last);

    g_assert_true (moo_history_list_find (list, "two", &iter));
    g_assert_false (moo_history_list_find (list, "missing", &iter));

    moo_history_list_set_max_entries (list, 2);
    moo_history_list_add (list, "three");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);
    last = moo_history_list_get_last_item (list);
    g_assert_cmpstr (last, ==, "three");
    g_free (last);

    moo_history_list_add (list, "");
    g_assert_cmpuint (moo_history_list_n_user_entries (list), ==, 2);

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


void
_moo_add_mooutils_unit_tests (void)
{
    g_test_add_func ("/mooutils/file-line", test_file_line);
    g_test_add_func ("/mooutils/splitlines", test_splitlines);
    g_test_add_func ("/mooutils/accel/parse", test_accel_parse);
    g_test_add_func ("/mooutils/accel/label", test_accel_label_parse);
    g_test_add_func ("/mooutils/markup/memory", test_markup_memory);
    g_test_add_func ("/mooutils/markup/mutation-modified", test_markup_mutation_modified);
    g_test_add_func ("/mooutils/markup/round-trip-edges", test_markup_round_trip_edges);
    g_test_add_func ("/mooutils/ui-xml/memory", test_ui_xml_memory);
    g_test_add_func ("/mooutils/output-filter/memory", test_output_filter_memory);
    g_test_add_func ("/mooutils/ui-xml/rejects-invalid-nodes",
                     test_ui_xml_rejects_invalid_nodes);
    g_test_add_func ("/mooutils/prefs/memory", test_prefs_memory);
    g_test_add_func ("/mooutils/path/utilities", test_path_utilities);
    g_test_add_func ("/mooutils/path/boundaries", test_path_boundaries);
    g_test_add_func ("/mooutils/history-list/memory", test_history_list_memory);
#if GTK_CHECK_VERSION(3,0,0)
    g_test_add_func ("/mooutils/paned/drop-mask", test_drop_mask);
#endif
    g_test_add_func ("/mooutils/file-writer", test_file_writer);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
