/*
 *   ctags/ctags-tests.cpp
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
 * The part of the ctags pane that can be asked a question without running
 * ctags and without a window: how a tag kind maps onto a group of the tree.
 */

#include "ctags-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "ctags-doc.h"


static void
test_group_for_kind (void)
{
    /* The kinds the C-like languages share, one per group. */
    g_assert_cmpint (_moo_ctags_group_for_kind ("f"), ==, MOO_CTAGS_GROUP_FUNCS);
    g_assert_cmpint (_moo_ctags_group_for_kind ("d"), ==, MOO_CTAGS_GROUP_MACROS);
    g_assert_cmpint (_moo_ctags_group_for_kind ("v"), ==, MOO_CTAGS_GROUP_VARS);
    g_assert_cmpint (_moo_ctags_group_for_kind ("t"), ==, MOO_CTAGS_GROUP_TYPES);

    /* An enum is a type where it is not a container of its own. */
    g_assert_cmpint (_moo_ctags_group_for_kind ("g"), ==, MOO_CTAGS_GROUP_TYPES);

    /* Nothing is dropped: a kind this does not know about still gets a place
       in the tree, which is what keeps an unfamiliar language usable. */
    g_assert_cmpint (_moo_ctags_group_for_kind ("p"), ==, MOO_CTAGS_GROUP_OTHER);
    g_assert_cmpint (_moo_ctags_group_for_kind ("x"), ==, MOO_CTAGS_GROUP_OTHER);
    g_assert_cmpint (_moo_ctags_group_for_kind (""), ==, MOO_CTAGS_GROUP_OTHER);

    /* Kinds are one character and compared whole, so a longer string that
       starts with a known letter is not that kind. */
    g_assert_cmpint (_moo_ctags_group_for_kind ("function"), ==, MOO_CTAGS_GROUP_OTHER);
}


void
_moo_add_ctags_unit_tests (void)
{
    g_test_add_func ("/ctags/group-for-kind", test_group_for_kind);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
