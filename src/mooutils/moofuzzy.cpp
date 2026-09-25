/*
 *   mooutils/moofuzzy.cpp
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
 * FuzzyMatchV2, the algorithm fzf uses (src/algo/algo.go): a Smith-Waterman
 * subsequence match with bonuses for word boundaries, camelCase/digit runs
 * and consecutive characters, so that "lspc" prefers "lsp-client.cpp" over
 * a path that merely contains the same four letters in that order.
 */

#include "mooutils/moofuzzy.h"
#include <limits.h>

/* Nothing real is this long; refusing outsized input is simpler than
   growing an O(pattern*text) matrix without bound. */
#define MAX_PATTERN_LEN 256
#define MAX_TEXT_LEN    4096

enum {
    SCORE_MATCH     = 16,
    SCORE_GAP_START = -3,
    SCORE_GAP_EXT   = -1,

    BONUS_BOUNDARY        = SCORE_MATCH / 2,                  /* 8: after '_', '-', '.', a non-word char */
    BONUS_BOUNDARY_WHITE  = BONUS_BOUNDARY + 2,               /* 10: after whitespace or start of string */
    BONUS_BOUNDARY_DELIM  = BONUS_BOUNDARY + 1,               /* 9: after '/', ':', ';', ',', '|' */
    BONUS_NON_WORD        = SCORE_MATCH / 2,                  /* 8: the matched character is itself a separator */
    BONUS_CAMEL123        = BONUS_BOUNDARY + SCORE_GAP_EXT,   /* 7: aB or a1 */
    BONUS_CONSECUTIVE     = -(SCORE_GAP_START + SCORE_GAP_EXT), /* 4 */
    BONUS_FIRST_CHAR_MULT = 2,

    NEG_INF = INT_MIN / 2  /* room to add a few bonuses without overflow */
};

typedef enum {
    CLASS_WHITE,
    CLASS_DELIM,
    CLASS_NON_WORD,
    CLASS_LOWER,
    CLASS_UPPER,
    CLASS_NUMBER
} CharClass;

static CharClass
classify (gunichar c)
{
    if (g_unichar_isspace (c))
        return CLASS_WHITE;
    if (c == '/' || c == ':' || c == ';' || c == ',' || c == '|')
        return CLASS_DELIM;
    if (g_unichar_islower (c))
        return CLASS_LOWER;
    if (g_unichar_isupper (c))
        return CLASS_UPPER;
    if (g_unichar_isdigit (c))
        return CLASS_NUMBER;
    return CLASS_NON_WORD;
}

static gboolean
is_word_class (CharClass c)
{
    return c == CLASS_LOWER || c == CLASS_UPPER || c == CLASS_NUMBER;
}

/* The bonus for matching here, given the class of the character before it. */
static int
bonus_for (CharClass prev, CharClass cur)
{
    if (is_word_class (cur))
    {
        if (prev == CLASS_WHITE)
            return BONUS_BOUNDARY_WHITE;
        if (prev == CLASS_DELIM)
            return BONUS_BOUNDARY_DELIM;
        if (prev == CLASS_NON_WORD)
            return BONUS_BOUNDARY;
    }

    if ((prev == CLASS_LOWER && cur == CLASS_UPPER) ||
        (prev != CLASS_NUMBER && cur == CLASS_NUMBER))
        return BONUS_CAMEL123;

    if (cur == CLASS_NON_WORD || cur == CLASS_DELIM)
        return BONUS_NON_WORD;

    return 0;
}

static gboolean
chars_equal (gunichar a, gunichar b, gboolean case_sensitive)
{
    if (case_sensitive)
        return a == b;
    return g_unichar_tolower (a) == g_unichar_tolower (b);
}

/* Cheap O(n) rejection: pattern must occur as an in-order subsequence at
   all, or a Smith-Waterman pass over a hopeless candidate is wasted work.
   This is what rejects the overwhelming majority of files as the user
   types, before the DP below ever runs. */
static gboolean
is_subsequence (const gunichar *pattern, guint plen,
                const gunichar *text,    guint tlen,
                gboolean        case_sensitive)
{
    guint pi = 0, ti = 0;

    while (pi < plen && ti < tlen)
    {
        if (chars_equal (pattern[pi], text[ti], case_sensitive))
            pi += 1;
        ti += 1;
    }

    return pi == plen;
}

gboolean
moo_fuzzy_match (const char    *pattern,
                 const char    *text,
                 gboolean       want_positions,
                 MooFuzzyMatch *match)
{
    g_return_val_if_fail (pattern != NULL, FALSE);
    g_return_val_if_fail (text != NULL, FALSE);
    g_return_val_if_fail (match != NULL, FALSE);

    match->score = 0;
    match->n_positions = 0;

    if (!*pattern)
        return TRUE;

    glong p_len_signed, t_len_signed;
    g_autofree gunichar *p = g_utf8_to_ucs4_fast (pattern, -1, &p_len_signed);
    g_autofree gunichar *t = g_utf8_to_ucs4_fast (text, -1, &t_len_signed);
    guint plen = (guint) p_len_signed;
    guint tlen = (guint) t_len_signed;

    if (plen > MAX_PATTERN_LEN || tlen > MAX_TEXT_LEN || plen > tlen)
        return FALSE;

    gboolean case_sensitive = FALSE;
    for (guint i = 0; i < plen; ++i)
        if (g_unichar_isupper (p[i]))
        {
            case_sensitive = TRUE;
            break;
        }

    if (!is_subsequence (p, plen, t, tlen, case_sensitive))
        return FALSE;

    /* Per-position bonus for text[j], independent of the pattern: what a
       match is worth there, given the character before it. */
    g_autofree int *pos_bonus = g_new (int, tlen);
    CharClass prev_class = CLASS_WHITE; /* start of string is a boundary too */
    for (guint j = 0; j < tlen; ++j)
    {
        CharClass cur_class = classify (t[j]);
        pos_bonus[j] = bonus_for (prev_class, cur_class);
        prev_class = cur_class;
    }

    guint rows = plen + 1;
    guint cols = tlen + 1;

    /* H[i][j]: best score matching pattern[0..i) somewhere within text[0..j),
       keeping only the current and previous row -- neither the diagonal
       jump nor a consecutive run looks back further than that. C is the
       matched run length ending at H[i][j], needed for the consecutive
       bonus and to tell a fresh gap from one already open. match_pos (kept
       in full, not just two rows) remembers the text position where the
       i-th pattern character actually landed, for the caller's highlight
       positions; it is skipped when want_positions is FALSE. */
    g_autofree int *H_prev = g_new0 (int, cols);
    g_autofree int *H_cur  = g_new (int, cols);
    g_autofree int *C_prev = g_new0 (int, cols);
    g_autofree int *C_cur  = g_new (int, cols);
    g_autofree int *pos_cur = g_new (int, cols);
    g_autofree int *match_pos = want_positions ? g_new (int, rows * cols) : NULL;

    int best_score = NEG_INF;
    guint best_j = 0;

    for (guint i = 1; i <= plen; ++i)
    {
        H_cur[0] = NEG_INF;
        C_cur[0] = 0;
        pos_cur[0] = -1;
        if (match_pos)
            match_pos[i * cols] = -1;

        for (guint j = 1; j < cols; ++j)
        {
            guint tj = j - 1;
            int d_score = NEG_INF;
            int d_consec = 0;
            int d_pos = -1;

            if (H_prev[j - 1] > NEG_INF / 2 && chars_equal (p[i - 1], t[tj], case_sensitive))
            {
                int bonus = pos_bonus[tj];
                if (i == 1)
                    bonus *= BONUS_FIRST_CHAR_MULT;

                int consec_bonus = 0;
                if (C_prev[j - 1] > 0)
                {
                    d_consec = C_prev[j - 1] + 1;
                    consec_bonus = BONUS_CONSECUTIVE;
                }
                else
                {
                    d_consec = 1;
                }

                d_score = H_prev[j - 1] + SCORE_MATCH + bonus + consec_bonus;
                d_pos = (int) tj;
            }

            int gap_score = NEG_INF;
            if (H_cur[j - 1] > NEG_INF / 2)
                gap_score = H_cur[j - 1] + (C_cur[j - 1] > 0 ? SCORE_GAP_START : SCORE_GAP_EXT);

            if (d_score >= gap_score)
            {
                H_cur[j] = d_score;
                C_cur[j] = d_consec;
                pos_cur[j] = d_pos;
            }
            else
            {
                H_cur[j] = gap_score;
                C_cur[j] = 0;
                pos_cur[j] = pos_cur[j - 1];
            }

            if (match_pos)
                match_pos[i * cols + j] = pos_cur[j];

            /* The last pattern character needs no trailing-gap propagation:
               nothing follows it, so the best placement is simply the best
               fresh match in this row, not whatever H_cur ends up carrying
               forward. */
            if (i == plen && d_score > NEG_INF / 2 && d_score > best_score)
            {
                best_score = d_score;
                best_j = j;
            }
        }

        int *tmp;
        tmp = H_prev; H_prev = H_cur; H_cur = tmp;
        tmp = C_prev; C_prev = C_cur; C_cur = tmp;
    }

    /* Every g_autofree local above -- H_prev/H_cur, C_prev/C_cur, pos_bonus,
       pos_cur, match_pos -- is freed by the compiler-inserted cleanup on this
       return, same as on the one below. The analyzer loses track of it here
       because of the double-buffer swap two lines up (`tmp = H_prev; H_prev =
       H_cur;`): it sees the pre-swap block as overwritten without a free,
       missing that the swap only renamed which variable's cleanup owns it. */
    /* NOLINTNEXTLINE(clang-analyzer-unix.Malloc) */
    if (best_score <= NEG_INF / 2)
        /* NOLINTNEXTLINE(clang-analyzer-unix.Malloc) */
        return FALSE; /* unreachable: is_subsequence already guarantees a path */

    match->score = best_score;

    if (want_positions)
    {
        guint col = best_j;
        for (guint i = plen; i >= 1; --i)
        {
            /* match_pos is non-NULL exactly when want_positions is TRUE (see
               its allocation above), which is where this loop runs; the
               analyzer cannot correlate the two separate variables. */
            /* NOLINTNEXTLINE(clang-analyzer-core.NullDereference) */
            int p_pos = match_pos[i * cols + col];
            guint idx = i - 1;
            if (idx < MOO_FUZZY_MAX_POSITIONS)
                match->positions[idx] = (guint) p_pos;
            col = (guint) p_pos + 1;
        }
        match->n_positions = MIN (plen, (guint) MOO_FUZZY_MAX_POSITIONS);
    }

    return TRUE;
}
