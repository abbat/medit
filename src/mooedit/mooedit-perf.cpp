/*
 *   mooedit/mooedit-perf.cpp
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
 * How long the highlighting engine takes over a big file. Developer's tool, not
 * a test: nothing here asserts a time, and none of it is registered unless
 * MOO_PERF is set, so it is not in the list ctest discovers and never runs in CI.
 *
 *     cmake -S . -B buildp -DCMAKE_BUILD_TYPE=Release -DENABLE_UNIT_TESTS=ON
 *     cmake --build buildp --target perf
 *
 * A build with sanitizers is two to three times slower and unevenly so; measure
 * in one without. The inputs are generated, MOO_PERF_MB of text each (default 8).
 *
 * Also replace-all (target perf-replace) over the same kind of inputs.
 *
 * Every highlighting case prints "insert" (set_text) and "highlight" (attaching the engine
 * and highlighting the whole buffer synchronously), the median of a few runs. With
 * MOO_PERF_OUT=<file> the lines are also written there, and with
 * MOO_PERF_BASELINE=<file from an earlier run> each line says how it compares.
 * Milliseconds depend on the machine; the ratio to a baseline taken on it is
 * what means something.
 */

#include "mooedit/mooedit-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "mooedit/moolang-private.h"
#include "mooedit/mootext-private.h"
#include "mooedit/mootextsearch.h"
#include "vendor/gtksourceview/gtksourceengine.h"

#define PERF_RUNS 3


struct PerfCase
{
    const char *name;
    const char *lang;
    void (*generate) (GString *out, gsize size);
};


/* One JSON array on one line: the long-line worst case. */
static void
gen_json_one_line (GString *out,
                   gsize    size)
{
    g_string_append_c (out, '[');

    for (guint i = 0; out->len < size; ++i)
        g_string_append_printf (out,
            "%s{\"id\":%u,\"name\":\"item %u\",\"ok\":true,\"tags\":[\"a\",\"b\"],\"v\":%u.5,\"n\":null}",
            i ? "," : "", i, i, i);

    g_string_append_c (out, ']');
}


/* The same data pretty-printed: a lot of short lines. */
static void
gen_json_many_lines (GString *out,
                     gsize    size)
{
    g_string_append (out, "[\n");

    for (guint i = 0; out->len < size; ++i)
        g_string_append_printf (out,
            "%s  {\n    \"id\": %u,\n    \"name\": \"item %u\",\n    \"ok\": true,\n"
            "    \"tags\": [\n      \"a\",\n      \"b\"\n    ],\n    \"v\": %u.5,\n    \"n\": null\n  }",
            i ? ",\n" : "", i, i, i);

    g_string_append (out, "\n]\n");
}


/* Nested objects, a few hundred levels at a time, so that the context stack of
   the engine is deep rather than only the file long. */
static void
gen_json_deep (GString *out,
               gsize    size)
{
    enum { DEPTH = 300 };

    while (out->len < size)
    {
        for (int d = 0; d < DEPTH; ++d)
            g_string_append_printf (out, "%*s{\n%*s\"k%d\": ", d, "", d + 1, "", d);

        g_string_append (out, "null\n");

        for (int d = DEPTH - 1; d >= 0; --d)
            g_string_append_printf (out, "%*s}\n", d, "");
    }
}


static void
gen_c_many_lines (GString *out,
                  gsize    size)
{
    for (guint i = 0; out->len < size; ++i)
        g_string_append_printf (out,
            "/* function %u */\n"
            "static int\n"
            "func_%u (const char *s, int n)\n"
            "{\n"
            "    int sum = 0;\n"
            "    for (int i = 0; i < n; ++i)\n"
            "        sum += s[i] == '\\n' ? 1 : (int) strlen (\"text %u\");\n"
            "    return sum;\n"
            "}\n\n",
            i, i, i);
}


static const PerfCase perf_cases[] = {
    { "json-one-line",   "json", gen_json_one_line },
    { "json-many-lines", "json", gen_json_many_lines },
    { "json-deep",       "json", gen_json_deep },
    { "c-many-lines",    "c",    gen_c_many_lines },
};


static double
now_ms (void)
{
    return g_get_monotonic_time () / 1000.0;
}


static int
cmp_double (const void *a,
            const void *b)
{
    double x = *(const double*) a;
    double y = *(const double*) b;

    return (x > y) - (x < y);
}


/* A previous MOO_PERF_OUT, "<case> <phase> <ms>" a line, or nothing. */
static double
baseline_ms (const char *name,
             const char *phase)
{
    const char *path = g_getenv ("MOO_PERF_BASELINE");
    char *content = nullptr;
    char **lines;
    double result = 0;

    if (path == nullptr || !g_file_get_contents (path, &content, nullptr, nullptr))
        return 0;

    lines = g_strsplit (content, "\n", -1);

    for (guint i = 0; lines[i] != nullptr; ++i)
    {
        char n[64], p[64];
        double ms;

        if (sscanf (lines[i], "%63s %63s %lf", n, p, &ms) == 3 &&
            strcmp (n, name) == 0 && strcmp (p, phase) == 0)
            result = ms;
    }

    g_strfreev (lines);
    g_free (content);

    return result;
}


static void
report (const char *name,
        const char *phase,
        double      ms)
{
    double base = baseline_ms (name, phase);
    const char *out = g_getenv ("MOO_PERF_OUT");
    char *line = g_strdup_printf ("%s %s %.1f", name, phase, ms);

    if (base > 0)
        g_print ("# perf %-16s %-10s %9.1f ms  (%.2fx of baseline %.1f)\n",
                 name, phase, ms, ms / base, base);
    else
        g_print ("# perf %-16s %-10s %9.1f ms\n", name, phase, ms);

    if (out != nullptr)
    {
        FILE *f = fopen (out, "a");

        if (f != nullptr)
        {
            fprintf (f, "%s\n", line);
            fclose (f);
        }
    }

    g_free (line);
}


/* GtkTextTag's class needs gdk's colour types registered; see
   mooedit-tests.cpp. The value has to be used or the call is dropped. */
static void
register_gdk_types (void)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    g_assert (gdk_color_get_type () != G_TYPE_INVALID);
G_GNUC_END_IGNORE_DEPRECATIONS
}


static void
test_perf_highlight (gconstpointer data)
{
    const PerfCase *c = (const PerfCase*) data;
    const char *mb = g_getenv ("MOO_PERF_MB");
    gsize size = (mb != nullptr ? (gsize) g_ascii_strtoull (mb, nullptr, 10) : 8) << 20;
    char *langs_dir = g_build_filename (MOO_UNIT_TEST_SOURCE_ROOT, "src", "mooedit", "langs", nullptr);
    char *dirs[2] = { langs_dir, nullptr };
    GtkSourceLanguageManager *manager = gtk_source_language_manager_new ();
    GtkSourceLanguage *lang;
    GString *text = g_string_sized_new (size + 4096);
    double insert[PERF_RUNS], highlight[PERF_RUNS];

    /* The engine reports a line it gave up on as a critical, which the test
       framework makes fatal; here it is a result, not a failure. */
    g_log_set_always_fatal ((GLogLevelFlags) G_LOG_FATAL_MASK);

    g_object_set (manager, "search-path", dirs, nullptr);
    lang = gtk_source_language_manager_get_language (manager, c->lang);
    g_assert_nonnull (lang);

    register_gdk_types ();

    c->generate (text, size);
    g_print ("# perf %s: %.1f MB, %s\n", c->name, text->len / 1048576.0, c->lang);

    for (int run = 0; run < PERF_RUNS; ++run)
    {
        MooTextBuffer *buffer = MOO_TEXT_BUFFER (g_object_new (MOO_TYPE_TEXT_BUFFER, nullptr));
        GtkSourceEngine *engine = _moo_lang_get_engine (MOO_LANG (lang));
        GtkTextIter start, end;
        double t0, t1, t2;

        /* The engine is attached after the text is in, as in mooedit-tests.cpp:
           attached first, the update below finds nothing left to do. */
        t0 = now_ms ();
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text->str, text->len);
        t1 = now_ms ();
        _gtk_source_engine_attach_buffer (engine, GTK_TEXT_BUFFER (buffer));
        gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
        _gtk_source_engine_update_highlight (engine, &start, &end, TRUE);
        t2 = now_ms ();

        /* A run that tagged nothing is a fast run of nothing -- or the engine
           giving up, which is worth saying. */
        gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
        if (run == 0 && !gtk_text_iter_forward_to_tag_toggle (&start, nullptr))
            g_print ("# perf %s: nothing was highlighted\n", c->name);

        insert[run] = t1 - t0;
        highlight[run] = t2 - t1;

        _gtk_source_engine_attach_buffer (engine, nullptr);
        g_object_unref (engine);
        g_object_unref (buffer);
    }

    qsort (insert, PERF_RUNS, sizeof (double), cmp_double);
    qsort (highlight, PERF_RUNS, sizeof (double), cmp_double);

    report (c->name, "insert", insert[PERF_RUNS / 2]);
    report (c->name, "highlight", highlight[PERF_RUNS / 2]);

    g_string_free (text, TRUE);
    g_object_unref (manager);
    g_free (langs_dir);
}


struct ReplaceCase
{
    const char *name;
    const PerfCase *input;
    double max_mb;
    const char *pattern;
    const char *replacement;
    MooTextSearchFlags flags;
};


/* replace-all over the whole buffer, one user action: what Find and Replace's
   "Replace All" does. The one-line input is capped at a quarter of a megabyte
   because the time is quadratic there (1 MB took minutes: GTK splits and counts
   the one huge segment of the line on every replacement). MOO_PERF_MB may be
   fractional. */
static void
test_perf_replace (gconstpointer data)
{
    const ReplaceCase *c = (const ReplaceCase*) data;
    const char *mb = g_getenv ("MOO_PERF_MB");
    double mbytes = mb != nullptr ? g_ascii_strtod (mb, nullptr) : 8;
    gsize size = (gsize) (MIN (mbytes, c->max_mb) * 1048576);
    GString *text;
    double replace[PERF_RUNS];
    int count = 0;

    register_gdk_types ();
    text = g_string_sized_new (size + 4096);
    c->input->generate (text, size);
    g_print ("# perf %s: %.1f MB\n", c->name, text->len / 1048576.0);

    for (int run = 0; run < PERF_RUNS; ++run)
    {
        MooTextBuffer *buffer = MOO_TEXT_BUFFER (g_object_new (MOO_TYPE_TEXT_BUFFER, nullptr));
        GtkTextIter start;
        double t0;

        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text->str, text->len);
        gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);

        t0 = now_ms ();
        count = moo_text_replace_all (&start, nullptr, c->pattern, c->replacement, c->flags);
        replace[run] = now_ms () - t0;

        g_object_unref (buffer);
    }

    qsort (replace, PERF_RUNS, sizeof (double), cmp_double);
    g_print ("# perf %s: %d replacements\n", c->name, count);
    report (c->name, "replace", replace[PERF_RUNS / 2]);

    g_string_free (text, TRUE);
}


static const ReplaceCase replace_cases[] = {
    { "replace-plain-lines",  &perf_cases[1], 8, "null", "nil", (MooTextSearchFlags) 0 },
    { "replace-regex-lines",  &perf_cases[1], 8, "\"id\":\\s*(\\d+)", "\"ID\": \\1", MOO_TEXT_SEARCH_REGEX },
    { "replace-plain-oneline", &perf_cases[0], 0.25, "null", "nil", (MooTextSearchFlags) 0 },
    { "replace-regex-oneline", &perf_cases[0], 0.25, "\"id\":(\\d+)", "\"ID\":\\1", MOO_TEXT_SEARCH_REGEX },
};


void
_moo_add_mooedit_perf_tests (void)
{
    if (g_getenv ("MOO_PERF") == nullptr)
        return;

    for (guint i = 0; i < G_N_ELEMENTS (replace_cases); ++i)
    {
        char *path = g_strconcat ("/perf/replace/", replace_cases[i].name, nullptr);
        g_test_add_data_func (path, &replace_cases[i], test_perf_replace);
        g_free (path);
    }

    for (guint i = 0; i < G_N_ELEMENTS (perf_cases); ++i)
    {
        char *path = g_strconcat ("/perf/highlight/", perf_cases[i].name, nullptr);
        g_test_add_data_func (path, &perf_cases[i], test_perf_highlight);
        g_free (path);
    }
}

#endif /* MOO_ENABLE_UNIT_TESTS */
