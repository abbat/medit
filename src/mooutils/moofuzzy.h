/*
 *   mooutils/moofuzzy.h
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

#ifndef MOO_FUZZY_H
#define MOO_FUZZY_H

#include "mooglib/moo-glib.h"

G_BEGIN_DECLS

/* A query longer than this still scores correctly; only the highlighting
   stops past the 32nd matched character. */
#define MOO_FUZZY_MAX_POSITIONS 32

typedef struct {
    int   score;
    guint n_positions;
    guint positions[MOO_FUZZY_MAX_POSITIONS];  /* character offsets into text, ascending */
} MooFuzzyMatch;

/*
 * fzf's FuzzyMatchV2: a Smith-Waterman subsequence match that scores word
 * boundaries (after '/', ':', ';', ',', '|', whitespace, or another
 * non-word character), camelCase/digit transitions, and consecutive
 * characters above a scattered match of the same characters.
 *
 * pattern must be a subsequence of text -- case-sensitively if pattern
 * contains an uppercase letter (smart case), case-insensitively otherwise --
 * or this returns FALSE and leaves *match with score 0. An empty pattern
 * always matches, with score 0.
 *
 * want_positions controls whether the backtrace that fills match->positions
 * runs; pass FALSE when only the score is needed, which is every candidate
 * that will not end up in the visible top rows.
 */
gboolean    moo_fuzzy_match     (const char    *pattern,
                                 const char    *text,
                                 gboolean       want_positions,
                                 MooFuzzyMatch *match);

G_END_DECLS

#endif /* MOO_FUZZY_H */
