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
#include "mooutils/moofilewriter.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"

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
    g_test_add_func ("/mooutils/file-writer", test_file_writer);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
