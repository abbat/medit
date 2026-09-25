/*
 *   moofileview/moofoldermodel-perf.cpp
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
 * How FileList's add/nth/position scale as a folder grows. Developer's tool,
 * not a test: nothing here asserts a time, and none of it is registered
 * unless MOO_PERF is set, so it is not in the list ctest discovers and never
 * runs in CI. Shape follows mooedit-perf.cpp.
 *
 *     cmake -S . -B buildp -DCMAKE_BUILD_TYPE=Release -DENABLE_UNIT_TESTS=ON
 *     cmake --build buildp --target perf-foldermodel
 *
 * MOO_PERF_N sets the file count (default 20000). Each phase prints the
 * median of a few runs; with MOO_PERF_OUT=<file> the lines are also written
 * there, and with MOO_PERF_BASELINE=<file from an earlier run> each line
 * says how it compares.
 */

#include "moofileview/moofoldermodel-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "moofileview/moofile-private.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "moofileview/moofoldermodel-private.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdio.h>
#include <string.h>

#define PERF_RUNS 3


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


/* Same shape as mooedit-perf.cpp's baseline_ms()/report(); kept as its own
   copy rather than shared, since the two files have no common header and it
   is fifteen lines each. */
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


static guint
perf_n (void)
{
    const char *n = g_getenv ("MOO_PERF_N");
    return n != nullptr ? (guint) g_ascii_strtoull (n, nullptr, 10) : 20000;
}


/* Fisher-Yates: add/position are run in a shuffled order, the way a real
   directory listing arrives, rather than in the sorted order file_list_*
   would otherwise happen to see. */
static void
shuffle (guint *a,
        guint  n)
{
    guint i;

    for (i = n; i > 1; --i)
    {
        guint j = (guint) g_random_int_range (0, (gint32) i);
        guint tmp = a[i - 1];
        a[i - 1] = a[j];
        a[j] = tmp;
    }
}


/*
 * add: file_list_add() once per file, unsorted arrival order -- what loading
 * a folder does. nth: file_list_nth() walked in order, what the view does
 * scanning rows. position: file_list_position() in shuffled order, what a
 * single file changing on disk does. All three were O(n) per call over the
 * old GList (a folder of n files cost O(n^2) total); GSequence makes each
 * O(log n).
 */
static void
test_perf_foldermodel (gconstpointer data)
{
    guint n = perf_n ();
    guint *order = g_new (guint, n);
    MooFile **files = g_new (MooFile *, n);
    double add[PERF_RUNS], nth[PERF_RUNS], position[PERF_RUNS];
    guint i;

    (void) data;

    for (i = 0; i < n; ++i)
        order[i] = i;

    g_print ("# perf foldermodel: %u files\n", n);

    for (int run = 0; run < PERF_RUNS; ++run)
    {
        FileList *flist = file_list_new ((MooFileCmp) moo_file_cmp);
        double t0;

        shuffle (order, n);

        t0 = now_ms ();
        for (i = 0; i < n; ++i)
        {
            char name[32];
            MooFile *f;

            g_snprintf (name, sizeof (name), "file%06u", order[i]);
            f = _moo_file_new ("/tmp", name);
            file_list_add (flist, f);
            files[order[i]] = f;
            _moo_file_unref (f);
        }
        add[run] = now_ms () - t0;

        t0 = now_ms ();
        for (i = 0; i < n; ++i)
            file_list_nth (flist, (int) i);
        nth[run] = now_ms () - t0;

        shuffle (order, n);

        t0 = now_ms ();
        for (i = 0; i < n; ++i)
            file_list_position (flist, files[order[i]]);
        position[run] = now_ms () - t0;

        for (i = 0; i < n; ++i)
            file_list_remove (flist, files[i]);

        file_list_destroy (flist);
    }

    qsort (add, PERF_RUNS, sizeof (double), cmp_double);
    qsort (nth, PERF_RUNS, sizeof (double), cmp_double);
    qsort (position, PERF_RUNS, sizeof (double), cmp_double);

    report ("foldermodel", "add", add[PERF_RUNS / 2]);
    report ("foldermodel", "nth", nth[PERF_RUNS / 2]);
    report ("foldermodel", "position", position[PERF_RUNS / 2]);

    g_free (files);
    g_free (order);
}


void
_moo_add_moofoldermodel_perf_tests (void)
{
    if (g_getenv ("MOO_PERF") == nullptr)
        return;

    g_test_add_data_func ("/perf/foldermodel/bulk", nullptr, test_perf_foldermodel);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
