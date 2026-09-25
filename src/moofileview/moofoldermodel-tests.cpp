/*
 *   moofileview/moofoldermodel-tests.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

/*
 * moofoldermodel-private.h's file_list_* functions are static, but defined
 * directly in the header, so this file gets its own working copies just by
 * including it -- no need to reach inside moofoldermodel.cpp itself.
 */

#include "moofileview/moofoldermodel-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "moofileview/moofile-private.h"

/* moofoldermodel-private.h defines its whole FileList API as static functions
   inline in the header; this file exercises only a slice of it, so the rest
   are "defined but not used" in this translation unit -- not the case
   moofoldermodel.cpp's own compile of the header is in. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "moofileview/moofoldermodel-private.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <string.h>


/* Reverse of moo_file_cmp: a resort with this as the new comparator must
   reverse the list's walk order. */
static int
cmp_desc (MooFile *f1,
          MooFile *f2)
{
    return moo_file_cmp (f2, f1);
}


/*
 * file_list_set_cmp_func() resorts the list and hands back new_order[], which
 * moofoldermodel.cpp feeds straight to GtkTreeModel's "rows-reordered" to tell
 * the view how the rows just moved. If the physical GList disagrees with what
 * new_order[] claims, the view is told one permutation while file_list_next()
 * walks another -- wrong row contents at a given position after any sort
 * change in the file browser.
 */
static void
test_set_cmp_func_reorders_list (void)
{
    static const char *names[] = { "banana", "apple", "cherry" };
    static const char *names_desc[] = { "cherry", "banana", "apple" };
    const guint n = G_N_ELEMENTS (names);
    FileList *flist = file_list_new ((MooFileCmp) moo_file_cmp);
    MooFile *original[3];
    int *new_order = NULL;
    guint i;
    MooFile *file;

    for (i = 0; i < n; ++i)
    {
        MooFile *f = _moo_file_new ("/tmp", names[i]);
        file_list_add (flist, f);
        _moo_file_unref (f);
    }

    /* moo_file_cmp already sorted them on insert: apple, banana, cherry. */
    for (i = 0, file = file_list_first (flist); file; ++i, file = file_list_next (flist, file))
        original[i] = file;
    g_assert_cmpuint (i, ==, n);
    g_assert_cmpstr (_moo_file_name (original[0]), ==, "apple");
    g_assert_cmpstr (_moo_file_name (original[1]), ==, "banana");
    g_assert_cmpstr (_moo_file_name (original[2]), ==, "cherry");

    file_list_set_cmp_func (flist, (MooFileCmp) cmp_desc, &new_order);
    g_assert_nonnull (new_order);

    /* The physical walk order must now be descending. */
    i = 0;
    for (file = file_list_first (flist); file; ++i, file = file_list_next (flist, file))
        g_assert_cmpstr (_moo_file_name (file), ==, names_desc[i]);
    g_assert_cmpuint (i, ==, n);

    /* new_order[k] must be the ORIGINAL index of the file now at position k --
       that is the contract "rows-reordered" is told holds. */
    i = 0;
    for (file = file_list_first (flist); file; ++i, file = file_list_next (flist, file))
        g_assert_true (file == original[new_order[i]]);

    g_free (new_order);
    file_list_destroy (flist);
}


void
_moo_add_moofoldermodel_unit_tests (void)
{
    g_test_add_func ("/moofoldermodel/set-cmp-func-reorders-list", test_set_cmp_func_reorders_list);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
