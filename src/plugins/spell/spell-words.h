/*
 *   plugins/spell/spell-words.h
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

#ifndef MOO_SPELL_WORDS_H
#define MOO_SPELL_WORDS_H


G_BEGIN_DECLS

/* The alphabet decides which dictionary a word is looked up in. */
typedef enum {
    MOO_SPELL_LATIN,
    MOO_SPELL_CYRILLIC
} MooSpellScript;

typedef struct {
    const char     *start;
    const char     *end;
    MooSpellScript  script;
} MooSpellWord;

/*
 * Finds the next word of the UTF-8 text worth looking up, starting at byte
 * *pos, and moves *pos past it. Returns FALSE when there is none left.
 *
 * What is not worth it: what is not written in one of the two scripts, a word
 * of both, one letter, ALL CAPS, CamelCase, whatever touches a digit, an
 * underscore or a dot between letters (identifiers, file names), and every
 * chunk between spaces that holds a slash, an @ or starts with www. (paths,
 * addresses).
 */
gboolean    moo_spell_next_word     (const char     *text,
                                     gsize           len,
                                     gsize          *pos,
                                     MooSpellWord   *word);

G_END_DECLS

#endif /* MOO_SPELL_WORDS_H */
