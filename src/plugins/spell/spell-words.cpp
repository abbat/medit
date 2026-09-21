/*
 *   plugins/spell/spell-words.cpp
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

#include "plugins/spell/spell-words.h"

static gboolean
is_apostrophe (gunichar c)
{
    return c == '\'' || c == 0x2019;
}

/* A chunk is what lies between two spaces. */
static gboolean
chunk_is_skipped (const char *s,
                  const char *e)
{
    if (e - s >= 4 && g_ascii_strncasecmp (s, "www.", 4) == 0)
        return TRUE;

    for (; s < e; ++s)
        if (*s == '/' || *s == '@')
            return TRUE;

    return FALSE;
}

/* A dot with a letter on the far side of it: "file.txt", "self.value". */
static gboolean
is_dotted (const char *chunk_end,
           const char *dot)
{
    return *dot == '.' && dot + 1 < chunk_end &&
           g_unichar_isalpha (g_utf8_get_char (dot + 1));
}

static gboolean
touches_identifier (const char *cs,
                    const char *ce,
                    const char *rs,
                    const char *re)
{
    if (rs > cs)
    {
        const char *prev = g_utf8_prev_char (rs);
        gunichar c = g_utf8_get_char (prev);

        if (g_unichar_isdigit (c) || c == '_')
            return TRUE;
        if (c == '.' && prev > cs && g_unichar_isalpha (g_utf8_get_char (g_utf8_prev_char (prev))))
            return TRUE;
    }

    if (re < ce)
    {
        gunichar c = g_utf8_get_char (re);

        if (g_unichar_isdigit (c) || c == '_' || is_dotted (ce, re))
            return TRUE;
    }

    return FALSE;
}

/* Fills in the script and says whether the run of letters is a word. */
static gboolean
run_is_word (const char     *rs,
             const char     *re,
             MooSpellScript *script)
{
    int n = 0, upper = 0, upper_run = 0;
    gboolean camel = FALSE, prev_lower = FALSE, have_script = FALSE;

    for (const char *p = rs; p < re; p = g_utf8_next_char (p))
    {
        gunichar c = g_utf8_get_char (p);
        MooSpellScript s;

        if (is_apostrophe (c))
            continue;

        switch (g_unichar_get_script (c))
        {
            case G_UNICODE_SCRIPT_LATIN:    s = MOO_SPELL_LATIN;    break;
            case G_UNICODE_SCRIPT_CYRILLIC: s = MOO_SPELL_CYRILLIC; break;
            default:                        return FALSE;
        }

        if (have_script && s != *script)
            return FALSE;

        *script = s;
        have_script = TRUE;
        ++n;

        if (g_unichar_isupper (c))
        {
            ++upper;
            if (prev_lower)
                camel = TRUE;
            prev_lower = FALSE;
            ++upper_run;
        }
        else
        {
            /* HTMLParser: an acronym followed by a word */
            if (upper_run > 2)
                camel = TRUE;
            upper_run = 0;
            prev_lower = TRUE;
        }
    }

    return n >= 2 && upper < n && !camel;
}

gboolean
moo_spell_next_word (const char   *text,
                     gsize         len,
                     gsize        *pos,
                     MooSpellWord *word)
{
    const char *end = text + len;
    const char *p = text + *pos;

    while (p < end)
    {
        if (g_unichar_isspace (g_utf8_get_char (p)))
        {
            p = g_utf8_next_char (p);
            continue;
        }

        const char *cs = p, *ce = p;
        while (ce < end && !g_unichar_isspace (g_utf8_get_char (ce)))
            ce = g_utf8_next_char (ce);

        if (chunk_is_skipped (cs, ce))
        {
            p = ce;
            continue;
        }

        while (p < ce)
        {
            if (!g_unichar_isalpha (g_utf8_get_char (p)))
            {
                p = g_utf8_next_char (p);
                continue;
            }

            const char *rs = p, *re = p;

            while (re < ce)
            {
                gunichar c = g_utf8_get_char (re);
                const char *next = g_utf8_next_char (re);

                if (g_unichar_isalpha (c) ||
                    (is_apostrophe (c) && next < ce && g_unichar_isalpha (g_utf8_get_char (next))))
                {
                    re = next;
                    continue;
                }

                break;
            }

            p = re;

            if (!touches_identifier (cs, ce, rs, re) && run_is_word (rs, re, &word->script))
            {
                word->start = rs;
                word->end = re;
                *pos = p - text;
                return TRUE;
            }
        }
    }

    *pos = len;
    return FALSE;
}
