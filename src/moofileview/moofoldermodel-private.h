/*
 *   moofoldermodel-private.h
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#ifndef MOO_FOLDER_MODEL_PRIVATE_H
#define MOO_FOLDER_MODEL_PRIVATE_H

#include "mooglib/moo-glib.h"

G_BEGIN_DECLS

typedef struct _FileList FileList;

typedef int (*MooFileCmp) (MooFile *file1, MooFile *file2);

/*
 * Backed by a GSequence (a balanced tree) instead of a GList: file_list_add(),
 * file_list_remove(), file_list_nth() and file_list_position() used to walk
 * or splice a GList by hand (g_list_nth(), g_list_position(),
 * g_list_delete_link()), each O(n) -- O(n^2) total over a folder with n
 * entries loaded one file at a time. GSequence gives every one of those an
 * O(log n) primitive, and file_to_iter maps a MooFile* straight to its
 * GSequenceIter* the same way file_to_link used to map it to a GList*.
 */
struct _FileList {
    GSequence   *seq;                   /* MooFile*, sorted by name */
    int          size;
    GHashTable  *name_to_file;          /* char* -> MooFile* */
    GHashTable  *display_name_to_file;  /* char* -> MooFile* */
    GHashTable  *file_to_iter;          /* MooFile* -> GSequenceIter* */
    MooFileCmp   cmp_func;
};


static FileList *file_list_new          (MooFileCmp  cmp_func);
static void      file_list_destroy      (FileList   *flist);

static void      file_list_set_cmp_func (FileList   *flist,
                                         MooFileCmp  cmp_func,
                                         int       **new_order);

static int       file_list_add          (FileList   *flist,
                                         MooFile    *file);
static int       file_list_remove       (FileList   *flist,
                                         MooFile    *file);

static MooFile  *file_list_nth          (FileList   *flist,
                                         int         index_);
static int       file_list_position     (FileList   *flist,
                                         MooFile    *file);

static MooFile  *file_list_find_name    (FileList   *flist,
                                         const char *name);
static MooFile  *file_list_find_display_name
                                        (FileList   *flist,
                                         const char *display_name);

static MooFile  *file_list_first        (FileList   *flist);
static MooFile  *file_list_next         (FileList   *flist,
                                         MooFile    *file);

static GSList   *file_list_get_slist    (FileList   *flist);

static int       _cmp_func_wrapper      (gconstpointer   a,
                                         gconstpointer   b,
                                         gpointer        user_data);
static int       _compare_file_indices  (int            *a,
                                         int            *b,
                                         gpointer        user_data);

static void      _hash_table_insert     (FileList       *flist,
                                         MooFile        *file,
                                         GSequenceIter  *iter);
static void      _hash_table_remove     (FileList       *flist,
                                         MooFile        *file);


#ifdef MOO_DEBUG
#endif /* MOO_DEBUG */

#ifndef DEFINE_CHECK_FILE_LIST_INTEGRITY
#define CHECK_FILE_LIST_INTEGRITY(flist)
#endif


static FileList *file_list_new          (MooFileCmp cmp_func)
{
    FileList *flist = g_new0 (FileList, 1);

    flist->seq = g_sequence_new (NULL);
    flist->name_to_file =
            g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    flist->display_name_to_file =
            g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    flist->file_to_iter = g_hash_table_new (g_direct_hash, g_direct_equal);
    flist->cmp_func = cmp_func;

    CHECK_FILE_LIST_INTEGRITY (flist);

    return flist;
}


static void      file_list_destroy      (FileList   *flist)
{
    GSequenceIter *iter;

    g_return_if_fail (flist != NULL);

    g_hash_table_destroy (flist->display_name_to_file);
    g_hash_table_destroy (flist->name_to_file);
    g_hash_table_destroy (flist->file_to_iter);

    for (iter = g_sequence_get_begin_iter (flist->seq);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter))
        _moo_file_unref ((MooFile *) g_sequence_get (iter));

    g_sequence_free (flist->seq);

    g_free (flist);
}


static int       file_list_add          (FileList   *flist,
                                         MooFile    *file)
{
    MooFile *f = _moo_file_ref (file);
    GSequenceIter *iter;
    int index_;

    iter = g_sequence_insert_sorted (flist->seq, f, _cmp_func_wrapper,
                                     (gpointer) flist->cmp_func);
    index_ = g_sequence_iter_get_position (iter);

    _hash_table_insert (flist, f, iter);
    flist->size++;

    CHECK_FILE_LIST_INTEGRITY (flist);

    return index_;
}


static int       file_list_remove       (FileList   *flist,
                                         MooFile    *file)
{
    GSequenceIter *iter;
    int index_;

    iter = (GSequenceIter *) g_hash_table_lookup (flist->file_to_iter, file);
    g_assert (iter != NULL);
    index_ = g_sequence_iter_get_position (iter);

    _hash_table_remove (flist, file);
    g_sequence_remove (iter);
    flist->size--;

    CHECK_FILE_LIST_INTEGRITY (flist);

    _moo_file_unref (file);
    return index_;
}


static MooFile  *file_list_nth          (FileList   *flist,
                                         int         index_)
{
    GSequenceIter *iter;
    MooFile *file;
    g_assert (0 <= index_ && index_ < flist->size);
    iter = g_sequence_get_iter_at_pos (flist->seq, index_);
    file = (MooFile *) g_sequence_get (iter);
    g_assert (file != NULL);
    return file;
}


/*
 * Only ever asked by the assertions in moofoldermodel.c, which a release build
 * compiles away -- hence G_GNUC_UNUSED, and hence the two of these having been
 * removed once as unused code. They are not unused; they are the invariant the
 * model is written against, checked in the builds that check things.
 */
G_GNUC_UNUSED static gboolean
                 file_list_contains     (FileList   *flist,
                                         MooFile    *file)
{
    return g_hash_table_lookup (flist->file_to_iter, file) != NULL;
}


static int       file_list_position     (FileList   *flist,
                                         MooFile    *file)
{
    GSequenceIter *iter;
    int position;
    g_assert (file != NULL);
    iter = (GSequenceIter *) g_hash_table_lookup (flist->file_to_iter, file);
    g_assert (iter != NULL);
    position = g_sequence_iter_get_position (iter);
    g_assert (position >= 0);
    return position;
}


static MooFile  *file_list_find_name    (FileList   *flist,
                                         const char *name)
{
    return (MooFile *) g_hash_table_lookup (flist->name_to_file, name);
}


static MooFile  *file_list_find_display_name
                                        (FileList   *flist,
                                         const char *display_name)
{
    return (MooFile *) g_hash_table_lookup (flist->display_name_to_file,
                                display_name);
}


static MooFile  *file_list_first        (FileList   *flist)
{
    GSequenceIter *iter = g_sequence_get_begin_iter (flist->seq);

    if (g_sequence_iter_is_end (iter))
        return NULL;

    return (MooFile *) g_sequence_get (iter);
}


static MooFile  *file_list_next         (FileList   *flist,
                                         MooFile    *file)
{
    GSequenceIter *iter = (GSequenceIter *) g_hash_table_lookup (flist->file_to_iter, file);
    GSequenceIter *next;

    g_assert (iter != NULL);
    next = g_sequence_iter_next (iter);

    if (g_sequence_iter_is_end (next))
        return NULL;

    return (MooFile *) g_sequence_get (next);
}


static GSList   *file_list_get_slist    (FileList   *flist)
{
    GSequenceIter *iter;
    GSList *slist = NULL;

    for (iter = g_sequence_get_begin_iter (flist->seq);
         !g_sequence_iter_is_end (iter);
         iter = g_sequence_iter_next (iter))
        slist = g_slist_prepend (slist, _moo_file_ref ((MooFile *) g_sequence_get (iter)));

    return g_slist_reverse (slist);
}


/*
 * The permutation moofoldermodel.cpp hands to GtkTreeModel's
 * "rows-reordered": new_order[k] is the ORIGINAL position of the file that
 * ends up at position k. The sequence itself is emptied and rebuilt in the
 * new order rather than resorted in place, which needs nothing beyond one
 * pass to collect the current files, one qsort over their indices, and one
 * pass to reinsert -- no per-element O(log n) insert-sorted, since the target
 * order is already known.
 */
static void      file_list_set_cmp_func (FileList   *flist,
                                         MooFileCmp  cmp_func,
                                         int       **new_order)
{
    flist->cmp_func = cmp_func;

    if (flist->size)
    {
        MooFile **files;
        int *order;
        GSequenceIter *iter;
        int i;
        struct {
            MooFileCmp cmp_func;
            MooFile  **files;
        } data;

        files = g_new (MooFile *, flist->size);
        order = g_new (int, flist->size);

        i = 0;
        for (iter = g_sequence_get_begin_iter (flist->seq);
             !g_sequence_iter_is_end (iter);
             iter = g_sequence_iter_next (iter))
        {
            files[i] = (MooFile *) g_sequence_get (iter);
            order[i] = i;
            ++i;
        }
        g_assert (i == flist->size);

        data.cmp_func = cmp_func;
        data.files = files;
        g_qsort_with_data (order, flist->size, sizeof (int),
                           (GCompareDataFunc) _compare_file_indices, &data);

        g_sequence_remove_range (g_sequence_get_begin_iter (flist->seq),
                                 g_sequence_get_end_iter (flist->seq));

        for (i = 0; i < flist->size; ++i)
        {
            MooFile *file = files[order[i]];
            GSequenceIter *new_iter = g_sequence_append (flist->seq, file);
            g_hash_table_replace (flist->file_to_iter, file, new_iter);
        }

        g_free (files);
        *new_order = order;

        CHECK_FILE_LIST_INTEGRITY (flist);
    }
}


static void      _hash_table_insert     (FileList       *flist,
                                         MooFile        *file,
                                         GSequenceIter  *iter)
{
    g_hash_table_insert (flist->file_to_iter, file, iter);
    g_hash_table_insert (flist->name_to_file,
                         g_strdup (_moo_file_name (file)),
                         file);
    g_hash_table_insert (flist->display_name_to_file,
                         g_strdup (_moo_file_display_name (file)),
                         file);
}


static void      _hash_table_remove     (FileList       *flist,
                                         MooFile        *file)
{
    g_hash_table_remove (flist->file_to_iter, file);
    g_hash_table_remove (flist->name_to_file,
                         _moo_file_name (file));
    g_hash_table_remove (flist->display_name_to_file,
                         _moo_file_display_name (file));
}


static int
moo_file_case_cmp (MooFile *f1,
                   MooFile *f2)
{
    if (!strcmp (_moo_file_name (f1), ".."))
        return strcmp (_moo_file_name (f2), "..") ? -1 : 0;
    else if (!strcmp (_moo_file_name (f2), ".."))
        return 1;
    else if (MOO_FILE_IS_DIR (f1) && !MOO_FILE_IS_DIR (f2))
        return -1;
    else if (!MOO_FILE_IS_DIR (f1) && MOO_FILE_IS_DIR (f2))
        return 1;
    else
        return _moo_collation_key_cmp (_moo_file_collation_key (f1),
                                       _moo_file_collation_key (f2));
}

static int
moo_file_cmp (MooFile *f1,
              MooFile *f2)
{
    if (!strcmp (_moo_file_name (f1), ".."))
        return strcmp (_moo_file_name (f2), "..") ? -1 : 0;
    else if (!strcmp (_moo_file_name (f2), ".."))
        return 1;
    else if (MOO_FILE_IS_DIR (f1) && !MOO_FILE_IS_DIR (f2))
        return -1;
    else if (!MOO_FILE_IS_DIR (f1) && MOO_FILE_IS_DIR (f2))
        return 1;
    else
        return strcmp (_moo_file_display_name (f1),
                       _moo_file_display_name (f2));
}

static int
moo_file_case_cmp_fi (MooFile *f1,
                      MooFile *f2)
{
    if (!strcmp (_moo_file_name (f1), ".."))
        return strcmp (_moo_file_name (f2), "..") ? -1 : 0;
    else if (!strcmp (_moo_file_name (f2), ".."))
        return 1;
    else
        return _moo_collation_key_cmp (_moo_file_collation_key (f1),
                                       _moo_file_collation_key (f2));
}

static int
moo_file_cmp_fi (MooFile *f1,
                 MooFile *f2)
{
    if (!strcmp (_moo_file_name (f1), ".."))
        return strcmp (_moo_file_name (f2), "..") ? -1 : 0;
    else if (!strcmp (_moo_file_name (f2), ".."))
        return 1;
    else
        return strcmp (_moo_file_display_name (f1),
                       _moo_file_display_name (f2));
}


/* Adapts a MooFileCmp (no user_data) to the GCompareDataFunc g_sequence_*
   wants, so the same comparators used everywhere else in this file can be
   passed straight to g_sequence_insert_sorted(). */
static int       _cmp_func_wrapper      (gconstpointer   a,
                                         gconstpointer   b,
                                         gpointer        user_data)
{
    MooFileCmp cmp_func = (MooFileCmp) user_data;
    return cmp_func ((MooFile *) a, (MooFile *) b);
}


static int       _compare_file_indices  (int            *a,
                                         int            *b,
                                         gpointer        user_data)
{
    struct {
        MooFileCmp cmp_func;
        MooFile  **files;
    } *data = (decltype(data)) user_data;

    return data->cmp_func (data->files[*a], data->files[*b]);
}


G_END_DECLS

#endif /* MOO_FOLDER_MODEL_PRIVATE_H */
