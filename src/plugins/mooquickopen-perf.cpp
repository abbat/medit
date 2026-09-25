/*
 *   plugins/mooquickopen-perf.cpp
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
 * How long quick_open_rank() takes over a synthetic file list -- the same
 * MOO_PERF-gated convention as mooedit-perf.cpp: nothing here asserts a time,
 * and it is not registered unless MOO_PERF is set, so it never runs in CI.
 *
 *     cmake -S . -B buildp -DCMAKE_BUILD_TYPE=Release -DENABLE_UNIT_TESTS=ON
 *     cmake --build buildp --target perf
 *
 * MOO_PERF_FILES controls the candidate count (default 50000, the plan's
 * per-keystroke scale). Two cases share the same candidate list: a query that
 * matches ("moo", the keystroke case) and an empty one (the cold/just-opened
 * case, which still has to score and partial_sort every candidate). Median of
 * a few runs is printed and, with MOO_PERF_OUT/MOO_PERF_BASELINE, recorded and
 * compared, exactly as mooedit-perf.cpp does.
 */

#include "plugins/mooquickopen-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "plugins/mooquickopen-ranker.h"

#include <cstdio>
#include <cstring>

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

/* A previous MOO_PERF_OUT, "<name> <ms>" a line, or nothing. */
static double
baseline_ms (const char *name)
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
        char n[64];
        double ms;

        if (sscanf (lines[i], "%63s %lf", n, &ms) == 2 && strcmp (n, name) == 0)
            result = ms;
    }

    g_strfreev (lines);
    g_free (content);

    return result;
}

static void
report (const char *name,
        double      ms)
{
    double base = baseline_ms (name);
    const char *out = g_getenv ("MOO_PERF_OUT");

    if (base > 0)
        g_print ("# perf %-24s %9.1f ms  (%.2fx of baseline %.1f)\n", name, ms, ms / base, base);
    else
        g_print ("# perf %-24s %9.1f ms\n", name, ms);

    if (out != nullptr)
    {
        FILE *f = fopen (out, "a");

        if (f != nullptr)
        {
            fprintf (f, "%s %.1f\n", name, ms);
            fclose (f);
        }
    }
}

static void
make_candidates (std::vector<QuickOpenCandidate> &candidates,
                  guint                             n)
{
    static const char *dirs[] = { "src/mooedit", "src/mooutils", "src/plugins", "src/moocpp", "tests" };

    candidates.reserve (n);

    for (guint i = 0; i < n; ++i)
    {
        char *path = g_strdup_printf ("%s/file%06u.cpp", dirs[i % G_N_ELEMENTS (dirs)], i);
        candidates.push_back (QuickOpenCandidate (path));
        g_free (path);
    }
}

/* data is the query: "moo" for the keystroke case, "" for the cold case. */
static void
test_perf_rank (gconstpointer data)
{
    const char *query = (const char*) data;
    const char *env = g_getenv ("MOO_PERF_FILES");
    guint n = env != nullptr ? (guint) g_ascii_strtoull (env, nullptr, 10) : 50000;
    std::vector<QuickOpenCandidate> candidates;
    double runs[PERF_RUNS];
    char *name = g_strdup_printf ("quick-open-rank-%s-%u", *query ? "query" : "cold", n);

    make_candidates (candidates, n);
    g_print ("# perf %s: %u candidates\n", name, n);

    for (int run = 0; run < PERF_RUNS; ++run)
    {
        double t0 = now_ms ();
        std::vector<QuickOpenResult> results = quick_open_rank (candidates, query, std::string (), 0);
        runs[run] = now_ms () - t0;
        g_assert_cmpuint (results.size (), <=, 200);
    }

    qsort (runs, PERF_RUNS, sizeof (double), cmp_double);
    report (name, runs[PERF_RUNS / 2]);

    g_free (name);
}

void
_moo_quick_open_add_perf_tests (void)
{
    if (g_getenv ("MOO_PERF") == nullptr)
        return;

    g_test_add_data_func ("/perf/quick-open/rank-keystroke", (gconstpointer) "moo", test_perf_rank);
    g_test_add_data_func ("/perf/quick-open/rank-cold", (gconstpointer) "", test_perf_rank);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
