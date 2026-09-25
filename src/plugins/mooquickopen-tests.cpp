/*
 *   plugins/mooquickopen-tests.cpp
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
 * Golden-ranking tests for the pure half of Quick Open (mooquickopen-ranker.h):
 * the score formula from doc's Quick Open plan section 2.3 -- fuzzy score +
 * basename bonus (16) + frecency (8*log2(1+f), capped at 24) + open bonus (12)
 * + proximity (6) - current-file penalty (1000) -- and the ":N" line-suffix
 * parser. No display is needed, so these run under `medit --unit-test`.
 */

#include "plugins/mooquickopen-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "plugins/mooquickopen-ranker.h"
#include "mooutils/moofuzzy.h"


static void
test_score_no_match (void)
{
    QuickOpenCandidate c ("src/mooedit/mooedit.cpp");
    g_assert_cmpint (quick_open_score_one (c, "zzz", std::string (), 0), ==, 0);
}

static void
test_score_basename_bonus (void)
{
    /* "bar" is a subsequence of the full path in both, but only B's basename
       contains it -- the difference between the two scores must be exactly
       the basename bonus, with nothing else (frecency/open/proximity/current)
       in play. */
    QuickOpenCandidate a ("src/bar/other.cpp");
    QuickOpenCandidate b ("src/other/barfile.cpp");

    MooFuzzyMatch match_a, match_b;
    g_assert_true (moo_fuzzy_match ("bar", a.path.c_str (), FALSE, &match_a));
    g_assert_true (moo_fuzzy_match ("bar", b.path.c_str (), FALSE, &match_b));

    int score_a = quick_open_score_one (a, "bar", std::string (), 0);
    int score_b = quick_open_score_one (b, "bar", std::string (), 0);

    g_assert_cmpint (score_a, ==, match_a.score);
    g_assert_cmpint (score_b, ==, match_b.score + 16);
}

static void
test_score_frecency (void)
{
    /* Empty query: base fuzzy score is always 0, isolating the frecency term. */
    QuickOpenCandidate c ("a.txt");

    c.frecency = 0.0;
    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 0);

    c.frecency = 1.0;
    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 8); /* 8*log2(2) */

    c.frecency = 7.0; /* the cap: 2^(24/8) - 1 */
    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 24);

    c.frecency = 1000.0; /* above the cap: same as the cap */
    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 24);
}

static void
test_score_open_proximity_current (void)
{
    QuickOpenCandidate c ("src/mooedit/mooedit.cpp");

    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 0);

    c.is_open = true;
    g_assert_cmpint (quick_open_score_one (c, "", std::string (), 0), ==, 12);

    c.is_open = false;
    g_assert_cmpint (quick_open_score_one (c, "", "src/mooedit", 0), ==, 6);

    c.is_current = true;
    g_assert_cmpint (quick_open_score_one (c, "", "src/mooedit", 0), ==, 6 - 1000);
}

static void
test_result_less_tie_break (void)
{
    QuickOpenCandidate short_path ("b.txt");
    QuickOpenCandidate long_path ("aa.txt");
    QuickOpenCandidate lex_a ("a.txt");
    QuickOpenCandidate lex_b ("b.txt");

    /* higher score first */
    g_assert_true (quick_open_result_less ({ &short_path, 10 }, { &long_path, 5 }));
    g_assert_false (quick_open_result_less ({ &long_path, 5 }, { &short_path, 10 }));

    /* equal score: shorter path first */
    g_assert_true (quick_open_result_less ({ &short_path, 5 }, { &long_path, 5 }));

    /* equal score and length: lexicographic */
    g_assert_true (quick_open_result_less ({ &lex_a, 5 }, { &lex_b, 5 }));
    g_assert_false (quick_open_result_less ({ &lex_b, 5 }, { &lex_a, 5 }));
}

static void
test_rank_orders_by_score (void)
{
    std::vector<QuickOpenCandidate> candidates;
    candidates.push_back (QuickOpenCandidate ("src/foo.cpp"));
    candidates.push_back (QuickOpenCandidate ("src/foobar.cpp"));
    candidates[1].is_open = true; /* foobar.cpp outscores foo.cpp despite the longer name */

    std::vector<QuickOpenResult> results = quick_open_rank (candidates, "foo", std::string (), 0);

    g_assert_cmpuint (results.size (), ==, 2);
    g_assert_cmpstr (results[0].candidate->path.c_str (), ==, "src/foobar.cpp");
    g_assert_cmpstr (results[1].candidate->path.c_str (), ==, "src/foo.cpp");
    g_assert_cmpint (results[0].score, >, results[1].score);
}

static void
test_rank_truncates_to_top_200 (void)
{
    /* Exercises the partial_sort path (plan 2.4, Top-K instead of a full
       sort): the best candidate must still be found even when it is not the
       one appended last, and the result must be capped at 200. */
    std::vector<QuickOpenCandidate> candidates;
    for (int i = 0; i < 500; ++i)
        candidates.push_back (QuickOpenCandidate (g_strdup_printf ("file%03d.txt", i)));

    candidates[321].frecency = 1000.0; /* the one candidate that must survive the cut */

    std::vector<QuickOpenResult> results = quick_open_rank (candidates, "", std::string (), 0);

    g_assert_cmpuint (results.size (), ==, 200);
    g_assert_cmpstr (results[0].candidate->path.c_str (), ==, "file321.txt");
}

static void
test_add_candidate_merges_duplicates (void)
{
    std::vector<QuickOpenCandidate> candidates;
    std::unordered_map<std::string, size_t> index;

    add_candidate (candidates, index, "a.txt", false, false, 1.0);
    add_candidate (candidates, index, "a.txt", true, false, 5.0);
    add_candidate (candidates, index, "a.txt", false, true, 2.0);

    g_assert_cmpuint (candidates.size (), ==, 1);
    g_assert_true (candidates[0].is_open);
    g_assert_true (candidates[0].is_current);
    g_assert_cmpfloat (candidates[0].frecency, ==, 5.0);

    add_candidate (candidates, index, "", true, true, 9.0);
    g_assert_cmpuint (candidates.size (), ==, 1); /* an empty path is not a candidate */
}

static void
test_split_line (void)
{
    int line;
    gstr path;

    path = quick_open_split_line ("foo.cpp:42", &line);
    g_assert_cmpstr (path.get (), ==, "foo.cpp");
    g_assert_cmpint (line, ==, 41);

    path = quick_open_split_line ("foo.cpp", &line);
    g_assert_cmpstr (path.get (), ==, "foo.cpp");
    g_assert_cmpint (line, ==, -1);

    path = quick_open_split_line ("foo.cpp:", &line);
    g_assert_cmpstr (path.get (), ==, "foo.cpp");
    g_assert_cmpint (line, ==, -1);

    path = quick_open_split_line ("", &line);
    g_assert_cmpstr (path.get (), ==, "");
    g_assert_cmpint (line, ==, -1);
}

void
_moo_quick_open_add_unit_tests (void)
{
    g_test_add_func ("/quick-open/score/no-match", test_score_no_match);
    g_test_add_func ("/quick-open/score/basename-bonus", test_score_basename_bonus);
    g_test_add_func ("/quick-open/score/frecency", test_score_frecency);
    g_test_add_func ("/quick-open/score/open-proximity-current", test_score_open_proximity_current);
    g_test_add_func ("/quick-open/result-less/tie-break", test_result_less_tie_break);
    g_test_add_func ("/quick-open/rank/orders-by-score", test_rank_orders_by_score);
    g_test_add_func ("/quick-open/rank/truncates-to-top-200", test_rank_truncates_to_top_200);
    g_test_add_func ("/quick-open/add-candidate/merges-duplicates", test_add_candidate_merges_duplicates);
    g_test_add_func ("/quick-open/split-line", test_split_line);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
