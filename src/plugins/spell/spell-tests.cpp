/*
 *   plugins/spell/spell-tests.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#include "plugins/spell/spell-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "plugins/spell/spell-dict.h"


/* The words the tokenizer finds, as "word|word", Cyrillic ones marked with a *. */
static char *
words_of (const char *text)
{
    GString *out = g_string_new (NULL);
    gsize pos = 0;
    MooSpellWord w;

    while (moo_spell_next_word (text, strlen (text), &pos, &w))
    {
        if (out->len)
            g_string_append_c (out, '|');
        if (w.script == MOO_SPELL_CYRILLIC)
            g_string_append_c (out, '*');
        g_string_append_len (out, w.start, w.end - w.start);
    }

    return g_string_free (out, FALSE);
}

static void
check_words (const char *text,
             const char *expected)
{
    char *got = words_of (text);
    g_assert_cmpstr (got, ==, expected);
    g_free (got);
}

static void
test_words_plain (void)
{
    check_words ("", "");
    check_words ("   \n\t ", "");
    check_words ("Hello, world!", "Hello|world");
    check_words ("a I x", "");
    check_words ("don't 'quoted' rock’n’roll", "don't|quoted|rock’n’roll");
    check_words ("well-known", "well|known");
}

static void
test_words_scripts (void)
{
    check_words ("Привет, мир! hello", "*Привет|*мир|hello");
    /* both alphabets in one word, and one that is neither */
    check_words ("hеllo мirроr 世界 ελληνικά", "");
    check_words ("café naïve", "café|naïve");
}

static void
test_words_skipped (void)
{
    check_words ("NASA HTTP", "");
    check_words ("CamelCase camelCase iPhone HTMLParser", "");
    check_words ("snake_case _private foo_ __init__", "");
    check_words ("mp3 3rd abc123 x2y", "");
    check_words ("file.txt self.value a.b.c", "");
    check_words ("end.Next", "");
    check_words ("http://example.org/path www.example.org mail@example.org", "");
    check_words ("/usr/bin/env and/or", "");
    /* a full stop after a word is the end of a sentence, not a name */
    check_words ("Hello. World.", "Hello|World");
    check_words ("see http://example.org today", "see|today");
}

static void
test_words_resume (void)
{
    /* every call goes on where the last one stopped, and the end is sticky */
    const char *text = "one two";
    gsize pos = 0;
    MooSpellWord w;

    g_assert_true (moo_spell_next_word (text, 7, &pos, &w));
    g_assert_cmpuint (pos, ==, 3);
    g_assert_true (moo_spell_next_word (text, 7, &pos, &w));
    g_assert_true (w.start == text + 4 && w.end == text + 7);
    g_assert_false (moo_spell_next_word (text, 7, &pos, &w));
    g_assert_cmpuint (pos, ==, 7);
    g_assert_false (moo_spell_next_word (text, 7, &pos, &w));

    /* the length is honored, not the terminator */
    pos = 0;
    g_assert_true (moo_spell_next_word (text, 3, &pos, &w));
    g_assert_false (moo_spell_next_word (text, 3, &pos, &w));
}

static void
test_dict_stub (void)
{
    SpellDict *en = moo_spell_dict_new_stub (MOO_SPELL_LATIN);
    SpellDict *ru = moo_spell_dict_new_stub (MOO_SPELL_CYRILLIC);

    g_assert_true (en->check (en, "Hello", 5));
    g_assert_false (en->check (en, "Helo", 4));
    g_assert_true (ru->check (ru, "Привет", strlen ("Привет")));
    g_assert_false (ru->check (ru, "hello", 5));

    char **s = en->suggest (en, "Helo", 4, 5);
    g_assert_cmpuint (g_strv_length (s), ==, 2);
    g_assert_cmpstr (s[0], ==, "hello");
    g_strfreev (s);

    s = en->suggest (en, "Helo", 4, 1);
    g_assert_cmpuint (g_strv_length (s), ==, 1);
    g_strfreev (s);

    en->add (en, "Helo", 4);
    g_assert_true (en->check (en, "helo", 4));

    moo_spell_dict_destroy (en);
    moo_spell_dict_destroy (ru);
}

static void
test_lang_script (void)
{
    g_assert_cmpint (moo_spell_lang_script ("en_US"), ==, MOO_SPELL_LATIN);
    g_assert_cmpint (moo_spell_lang_script ("de"), ==, MOO_SPELL_LATIN);
    g_assert_cmpint (moo_spell_lang_script ("ru_RU"), ==, MOO_SPELL_CYRILLIC);
    g_assert_cmpint (moo_spell_lang_script ("uk"), ==, MOO_SPELL_CYRILLIC);
    g_assert_cmpint (moo_spell_lang_script ("rue"), ==, MOO_SPELL_LATIN);  /* not ru */
}

void
_moo_spell_add_unit_tests (void)
{
    g_test_add_func ("/spell/words/plain", test_words_plain);
    g_test_add_func ("/spell/words/scripts", test_words_scripts);
    g_test_add_func ("/spell/words/skipped", test_words_skipped);
    g_test_add_func ("/spell/words/resume", test_words_resume);
    g_test_add_func ("/spell/dict/stub", test_dict_stub);
    g_test_add_func ("/spell/dict/lang-script", test_lang_script);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
