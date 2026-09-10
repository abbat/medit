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
#include "mooedit/mootext-private.h"
#include "mooedit/mootextbuffer.h"
#include "mooedit/mootextsearch.h"
#include "gtksourceview/gtksourcecontextengine.h"
#include "gtksourceview/gtksourceengine.h"
#include "mooutils/mooundo.h"

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


/* Search positions are character offsets, not UTF-8 byte offsets. Each case
   gets its own ctest entry and timeout, including the zero-width replacements
   which would otherwise hang the whole unit suite on a progress regression. */
struct SearchCase
{
    const char *name;
    const char *text;
    const char *pattern;
    MooTextSearchFlags flags;
    gboolean backward;
    int start;
    int limit;
    int match_start;
    int match_end;
};


static const SearchCase search_cases[] = {
    {"forward-first", "cat cat", "cat", MooTextSearchFlags (0), FALSE, 0, -1, 0, 3},
    {"forward-offset", "cat cat", "cat", MooTextSearchFlags (0), FALSE, 1, -1, 4, 7},
    {"backward-last", "cat cat", "cat", MooTextSearchFlags (0), TRUE, 7, -1, 4, 7},
    {"backward-offset", "cat cat", "cat", MooTextSearchFlags (0), TRUE, 4, -1, 0, 3},
    {"forward-limit", "cat dog cat", "cat", MooTextSearchFlags (0), FALSE, 4, 7, -1, -1},
    {"backward-limit", "cat dog cat", "cat", MooTextSearchFlags (0), TRUE, 7, 4, -1, -1},
    {"whole-word", "scatter cat", "cat", MOO_TEXT_SEARCH_WHOLE_WORDS, FALSE, 0, -1, 8, 11},
    {"unicode", "я😀 кот кот", "кот", MooTextSearchFlags (0), FALSE, 0, -1, 3, 6},
    {"unicode-backward", "я😀 кот кот", "кот", MooTextSearchFlags (0), TRUE,
     10, -1, 7, 10},
    {"caseless", "CAT cat", "cat", MOO_TEXT_SEARCH_CASELESS, FALSE, 0, -1, 0, 3},
    {"regex-forward", "я😀 кот кот", "к.т", MOO_TEXT_SEARCH_REGEX, FALSE,
     0, -1, 3, 6},
    {"regex-backward", "я😀 кот кот", "к.т", MOO_TEXT_SEARCH_REGEX, TRUE,
     10, -1, 7, 10},
    {"regex-forward-limit", "cat dog cat", "c.t", MOO_TEXT_SEARCH_REGEX, FALSE, 4, 7, -1, -1},
    {"regex-backward-limit", "cat dog cat", "c.t", MOO_TEXT_SEARCH_REGEX, TRUE, 7, 4, -1, -1},
    {"regex-line-start", "one\ntwo", "^", MOO_TEXT_SEARCH_REGEX, FALSE, 1, -1, 4, 4},
    {"regex-line-end", "one\ntwo", "$", MOO_TEXT_SEARCH_REGEX, FALSE, 0, -1, 3, 3},
    {"regex-invalid", "cat", "[", MOO_TEXT_SEARCH_REGEX, FALSE, 0, -1, -1, -1},
    {"empty-buffer", "", "cat", MooTextSearchFlags (0), FALSE, 0, -1, -1, -1}
};


static void
test_search (gconstpointer data)
{
    const SearchCase *test = static_cast<const SearchCase*> (data);
    GtkTextBuffer *buffer = gtk_text_buffer_new (nullptr);
    GtkTextIter start, limit, match_start, match_end;
    gboolean found;

    gtk_text_buffer_set_text (buffer, test->text, -1);
    gtk_text_buffer_get_iter_at_offset (buffer, &start, test->start);
    if (test->limit >= 0)
        gtk_text_buffer_get_iter_at_offset (buffer, &limit, test->limit);

    if (test->backward)
        found = moo_text_search_backward (&start, test->pattern, test->flags,
                                          &match_start, &match_end,
                                          test->limit < 0 ? nullptr : &limit);
    else
        found = moo_text_search_forward (&start, test->pattern, test->flags,
                                         &match_start, &match_end,
                                         test->limit < 0 ? nullptr : &limit);

    g_assert_cmpint (found, ==, test->match_start >= 0);
    if (found)
    {
        g_assert_cmpint (gtk_text_iter_get_offset (&match_start), ==, test->match_start);
        g_assert_cmpint (gtk_text_iter_get_offset (&match_end), ==, test->match_end);
    }
    g_object_unref (buffer);
}


struct ReplaceCase
{
    const char *name;
    const char *text;
    const char *pattern;
    const char *replacement;
    MooTextSearchFlags flags;
    int start;
    int limit;
    int count;
    const char *expected;
};


static const ReplaceCase replace_cases[] = {
    {"literal-all", "cat cat", "cat", "dog", MooTextSearchFlags (0), 0, -1, 2, "dog dog"},
    {"literal-delete", "cat cat", "cat", "", MooTextSearchFlags (0), 0, -1, 2, " "},
    {"literal-no-match", "cat", "dog", "x", MooTextSearchFlags (0), 0, -1, 0, "cat"},
    {"literal-range", "cat cat cat", "cat", "kitten", MooTextSearchFlags (0),
     4, 7, 1, "cat kitten cat"},
    {"regex-range", "cat cat cat", "c.t", "kitten", MOO_TEXT_SEARCH_REGEX,
     4, 7, 1, "cat kitten cat"},
    {"regex-delete", "a12b34", "[0-9]+", "", MOO_TEXT_SEARCH_REGEX, 0, -1, 2, "ab"},
    {"regex-groups", "cat:12 dog:34", "([a-z]+):([0-9]+)", "\\2=\\1",
     MOO_TEXT_SEARCH_REGEX, 0, -1, 2, "12=cat 34=dog"},
    {"regex-literal-replacement", "cat", "(cat)", "\\1",
     MooTextSearchFlags (MOO_TEXT_SEARCH_REGEX | MOO_TEXT_SEARCH_REPL_LITERAL),
     0, -1, 1, "\\1"},
    {"unicode", "я😀 кот кот", "кот", "犬", MooTextSearchFlags (0),
     0, -1, 2, "я😀 犬 犬"},
    {"regex-unicode-groups", "я😀 кот", "(😀) (кот)", "\\2\\1",
     MOO_TEXT_SEARCH_REGEX, 0, -1, 1, "якот😀"},
    {"zero-line-start", "one\ntwo", "^", ">", MOO_TEXT_SEARCH_REGEX, 0, -1, 2, ">one\n>two"},
    {"zero-line-end", "one\ntwo", "$", "!", MOO_TEXT_SEARCH_REGEX, 0, -1, 2, "one!\ntwo!"},
    {"zero-lookahead", "яя", "(?=я)", "!", MOO_TEXT_SEARCH_REGEX, 0, -1, 2, "!я!я"},
    {"regex-invalid", "cat", "[", "dog", MOO_TEXT_SEARCH_REGEX, 0, -1, 0, "cat"},
    /* An empty match replaced with nothing is not an edit and is not counted. */
    {"zero-empty-replacement", "яя", "(?=я)", "", MOO_TEXT_SEARCH_REGEX, 0, -1, 0, "яя"},
    {"zero-at-eof", "one", "$", "", MOO_TEXT_SEARCH_REGEX, 0, -1, 0, "one"},
    {"empty-buffer", "", "cat", "dog", MooTextSearchFlags (0), 0, -1, 0, ""}
};


static void
test_replace (gconstpointer data)
{
    const ReplaceCase *test = static_cast<const ReplaceCase*> (data);
    GtkTextBuffer *buffer = gtk_text_buffer_new (nullptr);
    GtkTextIter start, end;
    char *text;
    int count;

    gtk_text_buffer_set_text (buffer, test->text, -1);
    gtk_text_buffer_get_iter_at_offset (buffer, &start, test->start);
    if (test->limit >= 0)
        gtk_text_buffer_get_iter_at_offset (buffer, &end, test->limit);
    count = moo_text_replace_all (&start, test->limit < 0 ? nullptr : &end,
                                  test->pattern, test->replacement, test->flags);
    g_assert_cmpint (count, ==, test->count);
    gtk_text_buffer_get_bounds (buffer, &start, &end);
    text = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
    g_assert_cmpstr (text, ==, test->expected);
    g_free (text);
    g_object_unref (buffer);
}


static void
test_corpus_missing (void)
{
    char *dir = highlight_dir ();

    g_test_message ("%s cannot be read, so there is nothing to highlight", dir);
    g_free (dir);
    g_test_fail ();
}


static MooTextBuffer *
new_text_buffer (const char *text)
{
    register_colour_type ();

    MooTextBuffer *buffer = MOO_TEXT_BUFFER (g_object_new (MOO_TYPE_TEXT_BUFFER, nullptr));

    moo_text_buffer_begin_non_undoable_action (buffer);
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
    moo_text_buffer_end_non_undoable_action (buffer);

    return buffer;
}


static char *
text_buffer_text (MooTextBuffer *buffer)
{
    GtkTextIter start, end;

    gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
    return gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);
}


static void
test_text_buffer_line_marks (void)
{
    MooTextBuffer *buffer = new_text_buffer ("a\nb\nc");
    MooLineMark *mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GSList *marks;
    GtkTextIter start;

    moo_text_buffer_add_line_mark (buffer, mark, 1);
    g_assert_cmpint (moo_line_mark_get_line (mark), ==, 1);
    marks = moo_text_buffer_get_line_marks_at_line (buffer, 1);
    g_assert_true (g_slist_find (marks, mark) != nullptr);
    g_slist_free (marks);

    /* A newline at the start of a line leaves a mark with the text that was
       on that line, rather than leaving it on the newly empty line. */
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &start, "\n", 1);
    g_assert_cmpint (moo_line_mark_get_line (mark), ==, 2);
    marks = moo_text_buffer_get_line_marks_at_line (buffer, 2);
    g_assert_true (g_slist_find (marks, mark) != nullptr);
    g_slist_free (marks);

    moo_text_buffer_delete_line_mark (buffer, mark);
    g_assert_true (moo_line_mark_get_deleted (mark));
    g_object_unref (mark);
    g_object_unref (buffer);
}


static void
test_text_buffer_moved_line_mark (void)
{
    MooTextBuffer *buffer = new_text_buffer ("a\nb\nc");
    MooLineMark *mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GtkTextIter start, end;
    char *text;

    moo_text_buffer_add_line_mark (buffer, mark, 2);

    /* Deleting from the middle of one line through the next line moves marks
       on the deleted lines to the line containing the surviving prefix. */
    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &start, 1);
    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &end, 4);
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
    g_assert_cmpint (moo_line_mark_get_line (mark), ==, 0);

    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "ac");
    g_free (text);

    moo_text_buffer_delete_line_mark (buffer, mark);
    g_object_unref (mark);
    g_object_unref (buffer);
}


static void
test_text_buffer_deleted_line_mark (void)
{
    MooTextBuffer *buffer = new_text_buffer ("a\nb\nc");
    MooLineMark *mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GtkTextIter start, end;

    moo_text_buffer_add_line_mark (buffer, mark, 1);

    /* A mark on a line removed from the beginning of a range is deleted with
       that line, rather than silently pointing at a different line. */
    gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &end, 4);
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);

    g_assert_true (moo_line_mark_get_deleted (mark));
    g_assert_null (moo_line_mark_get_buffer (mark));
    g_object_unref (mark);
    g_object_unref (buffer);
}


static void
test_text_buffer_line_edges (void)
{
    MooTextBuffer *buffer = new_text_buffer ("α😀\nβ\n");
    MooLineMark *first = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *second = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GtkTextIter start, end;
    GSList *marks;
    char *text;

    /* Two marks on one line remain attached to that line when its Unicode
       text changes. */
    moo_text_buffer_add_line_mark (buffer, first, 1);
    moo_text_buffer_add_line_mark (buffer, second, 1);
    marks = moo_text_buffer_get_line_marks_at_line (buffer, 1);
    g_assert_cmpuint (g_slist_length (marks), ==, 2);
    g_assert_true (g_slist_find (marks, first) != nullptr);
    g_assert_true (g_slist_find (marks, second) != nullptr);
    g_slist_free (marks);

    gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &start, 1);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &start, "Ж😀", -1);
    g_assert_cmpint (moo_line_mark_get_line (first), ==, 1);
    g_assert_cmpint (moo_line_mark_get_line (second), ==, 1);

    /* Deleting exactly the line's contents leaves its marks on the empty
       line; the newline itself is still outside this range. */
    gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &start, 1);
    end = start;
    gtk_text_iter_forward_to_line_end (&end);
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
    g_assert_cmpint (moo_line_mark_get_line (first), ==, 1);
    g_assert_cmpint (moo_line_mark_get_line (second), ==, 1);

    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "α😀\n\n");
    g_free (text);

    moo_text_buffer_delete_line_mark (buffer, first);
    moo_text_buffer_delete_line_mark (buffer, second);
    g_object_unref (first);
    g_object_unref (second);
    g_object_unref (buffer);
}


static void
test_text_buffer_empty_and_trailing_line (void)
{
    MooTextBuffer *empty = new_text_buffer ("");
    MooTextBuffer *trailing = new_text_buffer ("one\n");
    MooLineMark *empty_mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *trailing_mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GSList *marks;

    g_assert_false (moo_text_buffer_has_text (empty));
    g_assert_cmpint (gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (empty)), ==, 1);
    moo_text_buffer_add_line_mark (empty, empty_mark, 0);
    g_assert_cmpint (moo_line_mark_get_line (empty_mark), ==, 0);

    g_assert_true (moo_text_buffer_has_text (trailing));
    g_assert_cmpint (gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (trailing)), ==, 2);
    moo_text_buffer_add_line_mark (trailing, trailing_mark, 1);
    g_assert_cmpint (moo_line_mark_get_line (trailing_mark), ==, 1);
    marks = moo_text_buffer_get_line_marks_at_line (trailing, 1);
    g_assert_cmpuint (g_slist_length (marks), ==, 1);
    g_slist_free (marks);

    moo_text_buffer_delete_line_mark (empty, empty_mark);
    moo_text_buffer_delete_line_mark (trailing, trailing_mark);
    g_object_unref (empty_mark);
    g_object_unref (trailing_mark);
    g_object_unref (empty);
    g_object_unref (trailing);
}


static void
test_text_buffer_exact_line_delete (void)
{
    MooTextBuffer *buffer = new_text_buffer ("a\nb\nc");
    MooLineMark *deleted = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *moved = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GtkTextIter start, end;
    char *text;

    moo_text_buffer_add_line_mark (buffer, deleted, 1);
    moo_text_buffer_add_line_mark (buffer, moved, 2);

    /* Remove line 1 by selecting its text and terminating newline exactly.
       The next line survives and its mark moves onto the preceding line. */
    gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &start, 1);
    gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 2);
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);

    g_assert_true (moo_line_mark_get_deleted (deleted));
    g_assert_cmpint (moo_line_mark_get_line (moved), ==, 1);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "a\nc");
    g_free (text);

    moo_text_buffer_delete_line_mark (buffer, moved);
    g_object_unref (deleted);
    g_object_unref (moved);
    g_object_unref (buffer);
}


static void
test_line_buffer_tree_boundaries (void)
{
    LineBuffer *buffer = _moo_line_buffer_new ();
    MooTextBuffer *owner = new_text_buffer ("");
    MooLineMark *first = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *deleted = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *moved = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    MooLineMark *last = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK, nullptr));
    GSList *marks;
    GSList *moved_marks = nullptr;
    GSList *deleted_marks = nullptr;

    _moo_line_mark_set_buffer (first, owner, buffer);
    _moo_line_mark_set_buffer (deleted, owner, buffer);
    _moo_line_mark_set_buffer (moved, owner, buffer);
    _moo_line_mark_set_buffer (last, owner, buffer);

    /* Cross the leaf capacity so range traversal has to visit an internal
       B-tree node rather than a single array of lines. */
    _moo_line_buffer_split_line (buffer, 0, 39);
    _moo_line_buffer_add_mark (buffer, first, 0);
    _moo_line_buffer_add_mark (buffer, deleted, 16);
    _moo_line_buffer_add_mark (buffer, moved, 17);
    _moo_line_buffer_add_mark (buffer, last, 39);

    marks = _moo_line_buffer_get_marks_in_range (buffer, 10, 35);
    g_assert_cmpuint (g_slist_length (marks), ==, 2);
    g_assert_true (marks->data == deleted);
    g_assert_true (marks->next->data == moved);
    g_slist_free (marks);

    _moo_line_buffer_delete (buffer, 16, 2, 0, &moved_marks, &deleted_marks);
    g_assert_true (g_slist_find (moved_marks, moved) != nullptr);
    g_assert_true (g_slist_find (deleted_marks, deleted) != nullptr);
    g_assert_cmpint (moo_line_mark_get_line (moved), ==, 0);
    g_assert_cmpint (moo_line_mark_get_line (last), ==, 37);
    g_assert_cmpint (_moo_line_buffer_get_line_index (buffer,
                                                      _moo_line_mark_get_line (first)),
                     ==, 0);
    g_slist_free (moved_marks);
    g_slist_free (deleted_marks);

    _moo_line_buffer_free (buffer);
    g_object_unref (deleted);
    g_object_unref (owner);
}


static void
test_text_buffer_undo_redo (void)
{
    MooTextBuffer *buffer = new_text_buffer ("abc");
    MooUndoStack *stack = MOO_UNDO_STACK (_moo_text_buffer_get_undo_stack (buffer));
    GtkTextIter iter;
    char *text;

    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "X", 1);
    g_assert_true (moo_text_buffer_can_undo (buffer));
    g_assert_false (moo_text_buffer_can_redo (buffer));

    moo_undo_stack_undo (stack);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "abc");
    g_free (text);
    g_assert_false (moo_text_buffer_can_undo (buffer));
    g_assert_true (moo_text_buffer_can_redo (buffer));

    moo_undo_stack_redo (stack);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "aXbc");
    g_free (text);

    /* A new edit after undo invalidates the redo branch. */
    moo_undo_stack_undo (stack);
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Y", 1);
    g_assert_false (moo_text_buffer_can_redo (buffer));

    g_object_unref (buffer);
}


static void
test_text_buffer_undo_group (void)
{
    MooTextBuffer *buffer = new_text_buffer ("abc");
    MooUndoStack *stack = MOO_UNDO_STACK (_moo_text_buffer_get_undo_stack (buffer));
    GtkTextIter iter, end;
    char *text;

    gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "12", 2);
    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1);
    end = iter;
    gtk_text_iter_forward_char (&end);
    gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &end);
    gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

    moo_undo_stack_undo (stack);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "abc");
    g_free (text);
    g_assert_true (moo_text_buffer_can_redo (buffer));

    moo_undo_stack_redo (stack);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "ac12");
    g_free (text);

    moo_text_buffer_begin_non_undoable_action (buffer);
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "reset", -1);
    moo_text_buffer_end_non_undoable_action (buffer);
    g_assert_false (moo_text_buffer_can_undo (buffer));
    g_assert_false (moo_text_buffer_can_redo (buffer));

    g_object_unref (buffer);
}


static void
test_text_buffer_undo_freeze (void)
{
    MooTextBuffer *buffer = new_text_buffer ("abc");
    MooUndoStack *stack = MOO_UNDO_STACK (_moo_text_buffer_get_undo_stack (buffer));
    GtkTextIter iter;
    char *text;

    gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "X", 1);
    g_assert_true (moo_undo_stack_can_undo (stack));

    moo_undo_stack_freeze (stack);
    g_assert_true (moo_undo_stack_frozen (stack));
    g_assert_false (moo_undo_stack_can_undo (stack));
    g_assert_false (moo_undo_stack_can_redo (stack));

    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Y", 1);

    moo_undo_stack_freeze (stack);
    g_assert_true (moo_undo_stack_frozen (stack));
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Z", 1);
    moo_undo_stack_thaw (stack);
    g_assert_true (moo_undo_stack_frozen (stack));

    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Q", 1);
    g_assert_false (moo_undo_stack_can_undo (stack));

    moo_undo_stack_thaw (stack);
    g_assert_false (moo_undo_stack_frozen (stack));
    gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &iter);
    gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "R", 1);
    g_assert_true (moo_undo_stack_can_undo (stack));

    moo_undo_stack_undo (stack);
    text = text_buffer_text (buffer);
    g_assert_cmpstr (text, ==, "aXbcYZQ");
    g_free (text);

    g_object_unref (buffer);
}


static void
test_language_helpers (void)
{
    GSList *list;
    GSList *item;
    char *id;

    id = _moo_lang_id_from_name ("  PYTHON  ");
    g_assert_cmpstr (id, ==, "python");
    g_free (id);

    id = _moo_lang_id_from_name ("NoNe");
    g_assert_cmpstr (id, ==, MOO_LANG_NONE);
    g_free (id);

    list = _moo_lang_parse_string_list (" c, cpp ; python \t");
    g_assert_nonnull (list);
    g_assert_cmpstr ((const char*) list->data, ==, "c");
    item = list->next;
    g_assert_nonnull (item);
    g_assert_cmpstr ((const char*) item->data, ==, "cpp");
    item = item->next;
    g_assert_nonnull (item);
    g_assert_cmpstr ((const char*) item->data, ==, "python");
    g_assert_null (item->next);
    g_slist_free_full (list, g_free);
}


void
_moo_add_mooedit_unit_tests (void)
{
    char *dir = highlight_dir ();
    GDir *entries = g_dir_open (dir, 0, nullptr);
    GSList *ids = nullptr;
    GSList *l;
    const char *name;

    for (guint i = 0; i < G_N_ELEMENTS (search_cases); ++i)
    {
        char *path = g_strconcat ("/mooedit/search/", search_cases[i].name, nullptr);
        g_test_add_data_func (path, &search_cases[i], test_search);
        g_free (path);
    }
    for (guint i = 0; i < G_N_ELEMENTS (replace_cases); ++i)
    {
        char *path = g_strconcat ("/mooedit/replace/", replace_cases[i].name, nullptr);
        g_test_add_data_func (path, &replace_cases[i], test_replace);
        g_free (path);
    }

    g_test_add_func ("/mooedit/text-buffer/line-marks", test_text_buffer_line_marks);
    g_test_add_func ("/mooedit/text-buffer/moved-line-mark", test_text_buffer_moved_line_mark);
    g_test_add_func ("/mooedit/text-buffer/deleted-line-mark", test_text_buffer_deleted_line_mark);
    g_test_add_func ("/mooedit/text-buffer/line-edges", test_text_buffer_line_edges);
    g_test_add_func ("/mooedit/text-buffer/empty-and-trailing-line",
                     test_text_buffer_empty_and_trailing_line);
    g_test_add_func ("/mooedit/text-buffer/exact-line-delete",
                     test_text_buffer_exact_line_delete);
    g_test_add_func ("/mooedit/line-buffer/tree-boundaries",
                     test_line_buffer_tree_boundaries);
    g_test_add_func ("/mooedit/text-buffer/undo-redo", test_text_buffer_undo_redo);
    g_test_add_func ("/mooedit/text-buffer/undo-group", test_text_buffer_undo_group);
    g_test_add_func ("/mooedit/text-buffer/undo-freeze", test_text_buffer_undo_freeze);
    g_test_add_func ("/mooedit/language/helpers", test_language_helpers);

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
