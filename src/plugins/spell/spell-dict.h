/*
 *   plugins/spell/spell-dict.h
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

#ifndef MOO_SPELL_DICT_H
#define MOO_SPELL_DICT_H

#include "plugins/spell/spell-words.h"

G_BEGIN_DECLS

/* A dictionary of one language. Words are UTF-8, not terminated. */
typedef struct SpellDict SpellDict;

struct SpellDict {
    gboolean    (*check)    (SpellDict *dict, const char *word, gsize len);
    /* NULL-terminated, for g_strfreev(); at most max of them */
    char      **(*suggest)  (SpellDict *dict, const char *word, gsize len, guint max);
    /* into the personal dictionary, or for the session if there is none */
    void        (*add)      (SpellDict *dict, const char *word, gsize len);
    void        (*destroy)  (SpellDict *dict);
};

/*
 * The system's dictionary for a language tag such as en_US, through Enchant,
 * which is loaded when first asked: without it installed, or without that
 * language, this is NULL and nothing is checked.
 */
SpellDict      *moo_spell_dict_open         (const char *lang);

/* A fixed handful of words, so that tests do not depend on what is installed. */
SpellDict      *moo_spell_dict_new_stub     (MooSpellScript script);

/* Which alphabet a language tag is written in. */
MooSpellScript  moo_spell_lang_script       (const char *lang);

/* Lets go of Enchant; every dictionary must have been destroyed. */
void            moo_spell_dict_shutdown     (void);

void            moo_spell_dict_destroy      (SpellDict *dict);

G_END_DECLS

#endif /* MOO_SPELL_DICT_H */
