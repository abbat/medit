/*
 *   mooedit/mooedit-tests.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
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
 * What the highlighting engine makes of the language definitions.
 *
 * 179 .lang files are validated against language2.rng by
 * src/mooedit/langs/check.sh, which says they are well formed XML of the right
 * shape and nothing at all about what they highlight. This runs them: each
 * sample under tests/highlight is loaded into a buffer, highlighted to the end
 * synchronously, and the tags that came out are written down as
 * "where, which style, what text" and compared to the file beside it.
 *
 * It is the cheap half of the test suite -- no display, no sandbox, no clicks,
 * milliseconds per language -- and it guards two things at once: a change to
 * the engine that stops applying a style, and an edit to a .lang file that
 * highlights something differently than it did.
 *
 * To add a language: put tests/highlight/<lang id>.<ext> there and run
 *
 *     MOO_TEST_UPDATE_GOLDEN=1 buildu3/src/medit --unit-test /mooedit/highlight/<lang id>
 *
 * which writes tests/highlight/<lang id>.tags. Read what it wrote before
 * committing it: the file is the expectation, and a wrong one is worse than
 * none.
 */

#include "mooedit/mooedit-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "mooedit/moolang-private.h"
#include "mooedit/mootextbuffer.h"
#include "gtksourceview/gtksourcecontextengine.h"
#include "gtksourceview/gtksourceengine.h"

#include <gtk/gtk.h>
#include <string.h>


/* How much of a span's text a dumped line carries. Enough to recognise it,
   short enough that a line stays a line. */
#define SPAN_TEXT_CHARS 48


/*
 * GtkTextTag's class installs properties of gdk's colour types, and before gtk
 * has initialised those types are not registered: creating a tag then is
 * twelve g_param_spec_boxed criticals, which glib's test framework turns into a
 * failure. Naming the type registers it, and that is the whole fix -- measured
 * on GTK+3 3.24 with and without a display, and GTK+2 needs nothing at all.
 *
 * The result has to be used for something. The getter is G_GNUC_CONST, so a
 * call whose value is dropped is optimised away and the criticals come back.
 */
static void
register_colour_type (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GType type = gdk_color_get_type ();
G_GNUC_END_IGNORE_DEPRECATIONS

    g_assert (type != G_TYPE_INVALID);
}


/* Where the samples and the files they are compared to live. */
static char *
highlight_dir (void)
{
    return g_build_filename (MOO_UNIT_TEST_SOURCE_ROOT, "tests", "highlight", nullptr);
}


/* The part of a sample's name before the first dot, which is the language it
   is written in. */
static char *
sample_lang_id (const char *name)
{
    char *id = g_strdup (name);
    char *dot = strchr (id, '.');

    if (dot != nullptr)
        *dot = 0;

    return id;
}


static void
append_escaped (GString    *out,
                const char *text)
{
    const char *p;
    int n = 0;

    for (p = text; *p != 0; p = g_utf8_next_char (p))
    {
        gunichar c;

        if (n == SPAN_TEXT_CHARS)
        {
            g_string_append (out, "...");
            break;
        }

        c = g_utf8_get_char (p);
        n += 1;

        switch (c)
        {
            case '\n': g_string_append (out, "\\n"); break;
            case '\r': g_string_append (out, "\\r"); break;
            case '\t': g_string_append (out, "\\t"); break;
            case '\\': g_string_append (out, "\\\\"); break;
            case '"':  g_string_append (out, "\\\""); break;
            default:   g_string_append_unichar (out, c); break;
        }
    }
}


/*
 * Every run of text the engine tagged, in buffer order.
 *
 * Untagged runs are left out: they are most of a file and say nothing. Where
 * contexts nest the styles are all listed, outermost first, which is the order
 * gtk_text_iter_get_tags() returns tags in -- by priority, and the engine gives
 * each new tag a higher one than the last.
 */
static char *
dump_highlighting (GtkTextBuffer          *buffer,
                   GtkSourceContextEngine *engine)
{
    GString *out = g_string_new (nullptr);
    GtkTextIter iter;

    gtk_text_buffer_get_start_iter (buffer, &iter);

    while (!gtk_text_iter_is_end (&iter))
    {
        GtkTextIter next = iter;
        GSList *tags;

        if (!gtk_text_iter_forward_to_tag_toggle (&next, nullptr))
            gtk_text_buffer_get_end_iter (buffer, &next);

        tags = gtk_text_iter_get_tags (&iter);

        if (tags != nullptr)
        {
            GString *styles = g_string_new (nullptr);
            char *where;
            char *text;
            GSList *l;

            for (l = tags; l != nullptr; l = l->next)
            {
                const char *style =
                    _gtk_source_context_engine_get_tag_style (engine, GTK_TEXT_TAG (l->data));

                if (styles->len != 0)
                    g_string_append_c (styles, ' ');

                /* A tag the engine did not make cannot happen here -- nothing
                   else touches this buffer -- but a name is better than a
                   crash if it ever does. */
                g_string_append (styles, style != nullptr ? style : "<unknown>");
            }

            where = g_strdup_printf ("%d:%d-%d:%d",
                                     gtk_text_iter_get_line (&iter) + 1,
                                     gtk_text_iter_get_line_offset (&iter) + 1,
                                     gtk_text_iter_get_line (&next) + 1,
                                     gtk_text_iter_get_line_offset (&next) + 1);

            text = gtk_text_buffer_get_text (buffer, &iter, &next, FALSE);

            g_string_append_printf (out, "%-14s %-30s \"", where, styles->str);
            append_escaped (out, text);
            g_string_append (out, "\"\n");

            g_free (text);
            g_free (where);
            g_string_free (styles, TRUE);
        }

        g_slist_free (tags);
        iter = next;
    }

    return g_string_free (out, FALSE);
}


static char *
highlight_sample (const char *lang_id,
                  const char *sample_path)
{
    char *langs_dir;
    char *dirs[2];
    GtkSourceLanguageManager *manager;
    GtkSourceLanguage *lang;
    MooTextBuffer *buffer;
    GtkSourceEngine *engine;
    GtkTextIter start, end;
    char *text = nullptr;
    char *dump;
    GError *error = nullptr;

    g_assert_true (g_file_get_contents (sample_path, &text, nullptr, &error));
    g_assert_no_error (error);

    /* The definitions as they are in the tree, not as they are installed: a
       build with the unit tests is a developer's build, and the point is to
       test the files this commit changes. */
    langs_dir = g_build_filename (MOO_UNIT_TEST_SOURCE_ROOT, "src", "mooedit", "langs", nullptr);
    dirs[0] = langs_dir;
    dirs[1] = nullptr;

    manager = gtk_source_language_manager_new ();
    g_object_set (manager, "search-path", dirs, nullptr);

    lang = gtk_source_language_manager_get_language (manager, lang_id);
    g_assert_nonnull (lang);

    register_colour_type ();

    buffer = MOO_TEXT_BUFFER (g_object_new (MOO_TYPE_TEXT_BUFFER, nullptr));
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

    /* The engine directly rather than moo_text_buffer_set_lang(), because the
       dump needs the engine itself to name the styles and the buffer keeps
       its one private. The text is in place already, so nothing has to be
       forwarded to it as the buffer changes. */
    engine = _moo_lang_get_engine (MOO_LANG (lang));
    g_assert_nonnull (engine);

    _gtk_source_engine_attach_buffer (engine, GTK_TEXT_BUFFER (buffer));

    /* Synchronous: the analysis otherwise happens in an idle handler, and
       there is no main loop here to run one. */
    gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
    _gtk_source_engine_update_highlight (engine, &start, &end, TRUE);

    dump = dump_highlighting (GTK_TEXT_BUFFER (buffer), GTK_SOURCE_CONTEXT_ENGINE (engine));

    /* Before the engine goes: detaching is what removes the idle handler it
       installed and the marks it left in the buffer. */
    _gtk_source_engine_attach_buffer (engine, nullptr);

    g_object_unref (engine);
    g_object_unref (buffer);
    g_object_unref (manager);
    g_free (langs_dir);
    g_free (text);

    return dump;
}


static void
compare_to_golden (const char *golden_path,
                   const char *dump)
{
    char **expected;
    char **got;
    guint i;
    char *content = nullptr;
    GError *error = nullptr;

    if (g_getenv ("MOO_TEST_UPDATE_GOLDEN") != nullptr)
    {
        g_assert_true (g_file_set_contents (golden_path, dump, -1, &error));
        g_assert_no_error (error);
        g_test_message ("wrote %s", golden_path);
        return;
    }

    if (!g_file_get_contents (golden_path, &content, nullptr, &error))
    {
        g_test_message ("%s: %s. Write it with "
                        "MOO_TEST_UPDATE_GOLDEN=1, then read what it says.",
                        golden_path, error->message);
        g_error_free (error);
        g_test_fail ();
        return;
    }

    expected = g_strsplit (content, "\n", -1);
    got = g_strsplit (dump, "\n", -1);

    /* Line by line, and the first difference named: the whole dump is hundreds
       of lines, and two of them printed side by side is the failure a reader
       can act on. */
    for (i = 0; expected[i] != nullptr || got[i] != nullptr; ++i)
    {
        const char *e = expected[i] != nullptr ? expected[i] : "<end of file>";
        const char *g = got[i] != nullptr ? got[i] : "<end of file>";

        if (strcmp (e, g) != 0)
        {
            g_test_message ("%s line %u\n  expected: %s\n       got: %s",
                            golden_path, i + 1, e, g);
            g_test_fail ();
            break;
        }
    }

    g_strfreev (expected);
    g_strfreev (got);
    g_free (content);
}


static void
test_highlight (gconstpointer data)
{
    const char *lang_id = (const char*) data;
    char *sample_path = nullptr;
    char *golden_name;
    char *golden_path;
    char *dir;
    char *dump;
    GDir *entries;
    const char *name;

    dir = highlight_dir ();
    entries = g_dir_open (dir, 0, nullptr);
    g_assert_nonnull (entries);

    /* The sample is <lang id>.<whatever the language calls its files>, so the
       extension is the language's own and the test does not have to know it. */
    while ((name = g_dir_read_name (entries)) != nullptr)
    {
        char *stem;

        if (g_str_has_suffix (name, ".tags"))
            continue;

        stem = sample_lang_id (name);

        if (strcmp (stem, lang_id) == 0)
            sample_path = g_build_filename (dir, name, nullptr);

        g_free (stem);

        if (sample_path != nullptr)
            break;
    }

    g_dir_close (entries);
    g_assert_nonnull (sample_path);

    dump = highlight_sample (lang_id, sample_path);

    /* A language whose sample comes back with nothing tagged is the failure
       this test exists for, and an empty file would compare equal to an empty
       golden for ever after. */
    g_assert_cmpuint (strlen (dump), >, 0);

    golden_name = g_strconcat (lang_id, ".tags", nullptr);
    golden_path = g_build_filename (dir, golden_name, nullptr);

    compare_to_golden (golden_path, dump);

    g_free (golden_name);

    g_free (golden_path);
    g_free (sample_path);
    g_free (dump);
    g_free (dir);
}


static void
test_corpus_missing (void)
{
    char *dir = highlight_dir ();

    g_test_message ("%s cannot be read, so there is nothing to highlight", dir);
    g_free (dir);
    g_test_fail ();
}


void
_moo_add_mooedit_unit_tests (void)
{
    char *dir = highlight_dir ();
    GDir *entries = g_dir_open (dir, 0, nullptr);
    GSList *ids = nullptr;
    GSList *l;
    const char *name;

    if (entries == nullptr)
    {
        /* Loud rather than absent: a suite that quietly registers no tests
           reports a pass. */
        g_test_add_func ("/mooedit/highlight", test_corpus_missing);
        g_free (dir);
        return;
    }

    while ((name = g_dir_read_name (entries)) != nullptr)
    {
        char *stem;

        if (g_str_has_suffix (name, ".tags"))
            continue;

        stem = sample_lang_id (name);

        if (g_slist_find_custom (ids, stem, (GCompareFunc) strcmp) == nullptr)
            ids = g_slist_prepend (ids, stem);
        else
            g_free (stem);
    }

    g_dir_close (entries);

    /* Sorted, because g_dir_read_name() hands them over in whatever order the
       filesystem keeps them and a test list that reorders itself between
       machines is a list nobody can diff. */
    ids = g_slist_sort (ids, (GCompareFunc) strcmp);

    for (l = ids; l != nullptr; l = l->next)
    {
        char *id = (char*) l->data;
        char *path = g_strconcat ("/mooedit/highlight/", id, nullptr);

        g_test_add_data_func_full (path, id, test_highlight, g_free);
        g_free (path);
    }

    g_slist_free (ids);
    g_free (dir);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
