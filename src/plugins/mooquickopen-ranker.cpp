/*
 *   plugins/mooquickopen-ranker.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#include "plugins/mooquickopen-ranker.h"

#include "mooutils/moofuzzy.h"
#include "mooutils/mooutils-fs.h"

#include <algorithm>
#include <cmath>

#define QUICK_OPEN_BASENAME_BONUS 16
#define QUICK_OPEN_FRECENCY_WEIGHT 8.0
#define QUICK_OPEN_FRECENCY_CAP 24.0
#define QUICK_OPEN_OPEN_BONUS 12
#define QUICK_OPEN_PROXIMITY_BONUS 6
#define QUICK_OPEN_CURRENT_PENALTY 1000
#define QUICK_OPEN_MAX_RESULTS 200

static bool
same_directory (const std::string &path, const std::string &dir)
{
    if (dir.empty ())
        return false;

    gstr path_dir = gstr::take (g_path_get_dirname (path.c_str ()));
    return dir == path_dir.get ();
}

int
quick_open_score_one (const QuickOpenCandidate &c,
                      const char               *query,
                      const std::string        &active_dir,
                      gint64                    now)
{
    MooFuzzyMatch match;

    if (!moo_fuzzy_match (query, c.path.c_str (), FALSE, &match))
        return 0;

    int score = match.score;

    if (query[0])
    {
        gstr base = gstr::take (g_path_get_basename (c.path.c_str ()));
        MooFuzzyMatch base_match;
        if (moo_fuzzy_match (query, base.get (), FALSE, &base_match))
            score += QUICK_OPEN_BASENAME_BONUS;
    }

    double f = std::min (c.frecency, exp2 (QUICK_OPEN_FRECENCY_CAP / QUICK_OPEN_FRECENCY_WEIGHT) - 1.0);
    score += (int) (QUICK_OPEN_FRECENCY_WEIGHT * log2 (1.0 + f));

    if (c.is_open)
        score += QUICK_OPEN_OPEN_BONUS;

    if (same_directory (c.path, active_dir))
        score += QUICK_OPEN_PROXIMITY_BONUS;

    if (c.is_current)
        score -= QUICK_OPEN_CURRENT_PENALTY;

    (void) now;
    return score;
}

bool
quick_open_result_less (const QuickOpenResult &a, const QuickOpenResult &b)
{
    if (a.score != b.score)
        return a.score > b.score;
    if (a.candidate->path.size () != b.candidate->path.size ())
        return a.candidate->path.size () < b.candidate->path.size ();
    return a.candidate->path < b.candidate->path;
}

std::vector<QuickOpenResult>
quick_open_rank (const std::vector<QuickOpenCandidate> &candidates,
                 const char                             *query,
                 const std::string                      &active_dir,
                 gint64                                  now)
{
    std::vector<QuickOpenResult> results;
    results.reserve (candidates.size ());

    for (const QuickOpenCandidate &c : candidates)
    {
        MooFuzzyMatch match;
        if (query[0] && !moo_fuzzy_match (query, c.path.c_str (), FALSE, &match))
            continue;

        results.push_back ({ &c, quick_open_score_one (c, query, active_dir, now) });
    }

    if (results.size () > QUICK_OPEN_MAX_RESULTS)
    {
        std::partial_sort (results.begin (), results.begin () + QUICK_OPEN_MAX_RESULTS,
                           results.end (), quick_open_result_less);
        results.resize (QUICK_OPEN_MAX_RESULTS);
    }
    else
    {
        std::sort (results.begin (), results.end (), quick_open_result_less);
    }

    return results;
}

void
add_candidate (std::vector<QuickOpenCandidate>        &candidates,
               std::unordered_map<std::string, size_t> &index,
               const std::string                       &path,
               bool                                      is_open,
               bool                                      is_current,
               double                                    frecency)
{
    if (path.empty ())
        return;

    auto it = index.find (path);
    if (it == index.end ())
    {
        index[path] = candidates.size ();
        candidates.push_back (QuickOpenCandidate (path));
        it = index.find (path);
    }

    QuickOpenCandidate &c = candidates[it->second];
    c.is_open |= is_open;
    c.is_current |= is_current;
    c.frecency = std::max (c.frecency, frecency);
}

gstr
quick_open_split_line (const char *query, int *out_line)
{
    char *parsed_path = NULL;
    int   parsed_line = 0;

    if (query[0] && _moo_parse_file_line (query, &parsed_path, &parsed_line))
    {
        *out_line = parsed_line - 1;
        return gstr::take (parsed_path);
    }

    *out_line = -1;
    return gstr (query);
}
