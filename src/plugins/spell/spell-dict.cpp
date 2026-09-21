/*
 *   plugins/spell/spell-dict.cpp
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
 * Enchant is opened with dlopen and used through the few prototypes below
 * rather than through its headers, so that building medit needs nothing of it
 * and a machine without it merely has no spell checking.
 */

#include "plugins/spell/spell-dict.h"


MooSpellScript
moo_spell_lang_script (const char *lang)
{
    static const char *const cyrillic[] = { "ru", "uk", "be", "bg", "sr", "mk", "kk" };

    for (guint i = 0; i < G_N_ELEMENTS (cyrillic); ++i)
    {
        gsize n = strlen (cyrillic[i]);

        if (strncmp (lang, cyrillic[i], n) == 0 && !g_ascii_isalpha (lang[n]))
            return MOO_SPELL_CYRILLIC;
    }

    return MOO_SPELL_LATIN;
}

void
moo_spell_dict_destroy (SpellDict *dict)
{
    if (dict)
        dict->destroy (dict);
}


/**********************************************************************/
/* Enchant
 */

typedef struct EnchantBroker EnchantBroker;
typedef struct EnchantDict EnchantDict;

static struct {
    GModule        *module;
    EnchantBroker  *broker;
    gboolean        tried;

    EnchantBroker  *(*broker_init)          (void);
    void            (*broker_free)          (EnchantBroker *);
    EnchantDict    *(*broker_request_dict)  (EnchantBroker *, const char *);
    void            (*broker_free_dict)     (EnchantBroker *, EnchantDict *);
    int             (*dict_check)           (EnchantDict *, const char *, ssize_t);
    char          **(*dict_suggest)         (EnchantDict *, const char *, ssize_t, size_t *);
    void            (*dict_free_string_list)(EnchantDict *, char **);
    void            (*dict_add)             (EnchantDict *, const char *, ssize_t);
} enchant;

static gboolean
enchant_load (void)
{
    if (enchant.tried)
        return enchant.broker != NULL;

    enchant.tried = TRUE;

    enchant.module = g_module_open ("libenchant-2.so.2", (GModuleFlags) (G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL));
    if (!enchant.module)
        return FALSE;

#define SYM(field, name) g_module_symbol (enchant.module, name, (gpointer*) &enchant.field)
    if (!SYM (broker_init, "enchant_broker_init") ||
        !SYM (broker_free, "enchant_broker_free") ||
        !SYM (broker_request_dict, "enchant_broker_request_dict") ||
        !SYM (broker_free_dict, "enchant_broker_free_dict") ||
        !SYM (dict_check, "enchant_dict_check") ||
        !SYM (dict_suggest, "enchant_dict_suggest") ||
        !SYM (dict_free_string_list, "enchant_dict_free_string_list") ||
        !SYM (dict_add, "enchant_dict_add"))
    {
        g_module_close (enchant.module);
        enchant.module = NULL;
        return FALSE;
    }
#undef SYM

    enchant.broker = enchant.broker_init ();
    return enchant.broker != NULL;
}

typedef struct {
    SpellDict    parent;
    EnchantDict *dict;
} EnchantSpellDict;

static gboolean
enchant_spell_check (SpellDict  *dict,
                     const char *word,
                     gsize       len)
{
    /* negative is an error, which is no reason to underline anything */
    return enchant.dict_check (((EnchantSpellDict*) dict)->dict, word, len) <= 0;
}

static char **
enchant_spell_suggest (SpellDict  *dict,
                       const char *word,
                       gsize       len,
                       guint       max)
{
    EnchantDict *ed = ((EnchantSpellDict*) dict)->dict;
    size_t n = 0;
    char **list = enchant.dict_suggest (ed, word, len, &n);
    GPtrArray *copy = g_ptr_array_new ();

    for (size_t i = 0; list && i < n && i < max; ++i)
        g_ptr_array_add (copy, g_strdup (list[i]));
    g_ptr_array_add (copy, NULL);

    if (list)
        enchant.dict_free_string_list (ed, list);

    return (char**) g_ptr_array_free (copy, FALSE);
}

static void
enchant_spell_add (SpellDict  *dict,
                   const char *word,
                   gsize       len)
{
    enchant.dict_add (((EnchantSpellDict*) dict)->dict, word, len);
}

static void
enchant_spell_destroy (SpellDict *dict)
{
    enchant.broker_free_dict (enchant.broker, ((EnchantSpellDict*) dict)->dict);
    g_free (dict);
}

SpellDict *
moo_spell_dict_open (const char *lang)
{
    if (!enchant_load ())
        return NULL;

    EnchantDict *ed = enchant.broker_request_dict (enchant.broker, lang);
    if (!ed)
        return NULL;

    EnchantSpellDict *dict = g_new0 (EnchantSpellDict, 1);
    dict->parent.check = enchant_spell_check;
    dict->parent.suggest = enchant_spell_suggest;
    dict->parent.add = enchant_spell_add;
    dict->parent.destroy = enchant_spell_destroy;
    dict->dict = ed;
    return &dict->parent;
}

void
moo_spell_dict_shutdown (void)
{
    if (enchant.broker)
        enchant.broker_free (enchant.broker);
    if (enchant.module)
        g_module_close (enchant.module);

    memset (&enchant, 0, sizeof enchant);
}


/**********************************************************************/
/* The stub
 */

typedef struct {
    SpellDict   parent;
    GHashTable *words;      /* folded */
    GPtrArray  *known;      /* the ones it started with, in order */
} StubDict;

static const char *const stub_latin[] = {
    "hello", "help", "world", "the", "quick", "brown", "fox", "test", "text", "document", "don't", NULL
};

static const char *const stub_cyrillic[] = {
    "привет", "проверка", "мир", "это", "тест", "текст", "документ", NULL
};

static gboolean
stub_check (SpellDict  *dict,
            const char *word,
            gsize       len)
{
    char *w = g_utf8_strdown (word, len);
    gboolean found = g_hash_table_contains (((StubDict*) dict)->words, w);
    g_free (w);
    return found;
}

static char **
stub_suggest (SpellDict  *dict,
              const char *word,
              gsize       len,
              guint       max)
{
    StubDict *stub = (StubDict*) dict;
    char *w = g_utf8_strdown (word, len);
    GPtrArray *list = g_ptr_array_new ();

    for (guint i = 0; i < stub->known->len && list->len < max; ++i)
    {
        const char *k = (const char*) g_ptr_array_index (stub->known, i);
        if (g_utf8_get_char (k) == g_utf8_get_char (w))
            g_ptr_array_add (list, g_strdup (k));
    }

    g_ptr_array_add (list, NULL);
    g_free (w);
    return (char**) g_ptr_array_free (list, FALSE);
}

static void
stub_add (SpellDict  *dict,
          const char *word,
          gsize       len)
{
    g_hash_table_add (((StubDict*) dict)->words, g_utf8_strdown (word, len));
}

static void
stub_destroy (SpellDict *dict)
{
    StubDict *stub = (StubDict*) dict;

    g_hash_table_unref (stub->words);
    g_ptr_array_free (stub->known, TRUE);
    g_free (stub);
}

SpellDict *
moo_spell_dict_new_stub (MooSpellScript script)
{
    const char *const *list = script == MOO_SPELL_CYRILLIC ? stub_cyrillic : stub_latin;
    StubDict *stub = g_new0 (StubDict, 1);

    stub->parent.check = stub_check;
    stub->parent.suggest = stub_suggest;
    stub->parent.add = stub_add;
    stub->parent.destroy = stub_destroy;
    stub->words = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    stub->known = g_ptr_array_new_with_free_func (g_free);

    for (; *list; ++list)
    {
        g_hash_table_add (stub->words, g_strdup (*list));
        g_ptr_array_add (stub->known, g_strdup (*list));
    }

    return &stub->parent;
}
