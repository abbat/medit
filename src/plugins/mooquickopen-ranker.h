/*
 *   plugins/mooquickopen-ranker.h
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#ifndef MOO_QUICK_OPEN_RANKER_H
#define MOO_QUICK_OPEN_RANKER_H

#include "moocpp/gstr.h"

#include <glib.h>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * The pure half of Quick Open: scoring and ranking candidates, and parsing a
 * ":N" suffix off the query. Split out of mooquickopen.cpp so it can be
 * exercised from mooquickopen-tests.cpp without a display; the dialog itself
 * stays there. See moo-unit-tests.h for why this split exists.
 */

struct QuickOpenCandidate {
    std::string path;      /* absolute */
    bool        is_open;
    bool        is_current;
    double      frecency;

    QuickOpenCandidate (const std::string &p)
        : path (p), is_open (false), is_current (false), frecency (0.0)
    {
    }
};

struct QuickOpenResult {
    const QuickOpenCandidate *candidate;
    int                       score;
};

/* Pure, testable: one candidate's score against a query. 0 means "no match". */
int quick_open_score_one (const QuickOpenCandidate &c,
                          const char               *query,
                          const std::string        &active_dir,
                          gint64                    now);

bool quick_open_result_less (const QuickOpenResult &a, const QuickOpenResult &b);

/* Pure, testable: rank & truncate candidates for a query. */
std::vector<QuickOpenResult> quick_open_rank (const std::vector<QuickOpenCandidate> &candidates,
                                              const char                             *query,
                                              const std::string                      &active_dir,
                                              gint64                                  now);

void add_candidate (std::vector<QuickOpenCandidate>        &candidates,
                    std::unordered_map<std::string, size_t> &index,
                    const std::string                       &path,
                    bool                                      is_open,
                    bool                                      is_current,
                    double                                    frecency);

/*
 * Splits a trailing ":N" or "(N)" off the query, the way medit's command line
 * does (_moo_parse_file_line), so typing "foo.cpp:42" both searches for
 * "foo.cpp" and remembers line 42 for quick_open_open_selected(). Same
 * 1-based-to-0-based conversion as main.cpp; a query with no line, or one
 * ending in a bare separator, yields -1 ("no line").
 */
gstr quick_open_split_line (const char *query, int *out_line);

#endif /* MOO_QUICK_OPEN_RANKER_H */
