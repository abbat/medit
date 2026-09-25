/*
 *   moofileindex.cpp
 *
 *   Copyright (C) 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#include "mooutils/moofileindex.h"
#include <string.h>

#define MOO_FILE_INDEX_MAX_AGE_SEC 5

static const char *SKIP_DIR_NAMES[] = {
    ".git", ".svn", ".hg", "CVS", "node_modules", NULL
};

static gboolean
is_skip_dir_name (const char *name)
{
    guint i;

    for (i = 0; SKIP_DIR_NAMES[i] != NULL; ++i)
        if (strcmp (name, SKIP_DIR_NAMES[i]) == 0)
            return TRUE;

    return FALSE;
}

/* reldir is "" at the root, otherwise a path relative to root with no
   trailing slash. A symlinked entry is never entered -- and never added
   either, which is the simplest way to keep a symlink cycle from looping
   the walk. */
static void
walk_dir (const char *root,
          const char *reldir,
          GPtrArray  *files,
          guint       max_files)
{
    g_autofree char *abs_dir = reldir[0] ? g_build_filename (root, reldir, NULL) : g_strdup (root);
    GDir *dir = g_dir_open (abs_dir, 0, NULL);
    const char *name;

    if (!dir)
        return;

    while (files->len < max_files && (name = g_dir_read_name (dir)) != NULL)
    {
        g_autofree char *rel_child = reldir[0] ? g_build_filename (reldir, name, NULL) : g_strdup (name);
        g_autofree char *abs_child = g_build_filename (root, rel_child, NULL);

        if (g_file_test (abs_child, G_FILE_TEST_IS_SYMLINK))
            continue;

        if (g_file_test (abs_child, G_FILE_TEST_IS_DIR))
        {
            if (!is_skip_dir_name (name))
                walk_dir (root, rel_child, files, max_files);
        }
        else
        {
            g_ptr_array_add (files, g_steal_pointer (&rel_child));
        }
    }

    g_dir_close (dir);
}

/* NULL means root is not a git work tree, or git could not be run --
   either way the caller falls back to walk_dir(). */
static GPtrArray *
try_git_ls_files (const char *root,
                  guint       max_files)
{
    g_autoptr(GSubprocessLauncher) launcher = NULL;
    g_autoptr(GSubprocess) proc = NULL;
    g_autoptr(GBytes) output = NULL;
    g_autoptr(GError) error = NULL;
    GPtrArray *files;
    const char *data;
    gsize size, start, i;

    launcher = g_subprocess_launcher_new ((GSubprocessFlags) (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE));
    g_subprocess_launcher_set_cwd (launcher, root);

    proc = g_subprocess_launcher_spawn (launcher, &error, "git", "ls-files",
                                        "-co", "--exclude-standard", "-z", NULL);

    if (!proc)
        return NULL;

    if (!g_subprocess_communicate (proc, NULL, NULL, &output, NULL, &error))
        return NULL;

    if (!g_subprocess_get_successful (proc))
        return NULL;

    files = g_ptr_array_new_with_free_func (g_free);
    data = (const char *) g_bytes_get_data (output, &size);
    start = 0;

    for (i = 0; i < size && files->len < max_files; ++i)
    {
        if (data[i] != '\0')
            continue;

        if (i > start)
            g_ptr_array_add (files, g_strndup (data + start, i - start));

        start = i + 1;
    }

    return files;
}

GPtrArray *
_moo_file_index_build (const char *root,
                       guint       max_files)
{
    GPtrArray *files;

    g_return_val_if_fail (root != NULL, NULL);

    if (!max_files)
        max_files = MOO_FILE_INDEX_MAX_FILES;

    files = try_git_ls_files (root, max_files);

    if (!files)
    {
        files = g_ptr_array_new_with_free_func (g_free);
        walk_dir (root, "", files, max_files);
    }

    return files;
}


/*
 * Per-root cache.
 */

typedef struct {
    MooFileIndexReadyFunc func;
    gpointer               user_data;
} Waiter;

typedef struct {
    GPtrArray *files;      /* NULL until the first build lands */
    gint64     built_at;
    gboolean   refreshing;
    GPtrArray *waiters;    /* of Waiter *, non-NULL only while refreshing */
} CacheEntry;

static void
cache_entry_free (gpointer data)
{
    CacheEntry *entry = (CacheEntry *) data;

    if (entry->files)
        g_ptr_array_unref (entry->files);
    if (entry->waiters)
        g_ptr_array_unref (entry->waiters);

    g_free (entry);
}

static GHashTable *
get_cache (void)
{
    static GHashTable *table = NULL;

    if (!table)
        table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, cache_entry_free);

    return table;
}

static void
build_in_thread (GTask                  *task,
                 G_GNUC_UNUSED gpointer  source_object,
                 gpointer                task_data,
                 G_GNUC_UNUSED GCancellable *cancellable)
{
    GPtrArray *files = _moo_file_index_build ((const char *) task_data, 0);
    g_task_return_pointer (task, files, (GDestroyNotify) g_ptr_array_unref);
}

static void
refresh_done (G_GNUC_UNUSED GObject *source,
             GAsyncResult          *result,
             gpointer               user_data)
{
    g_autofree char *root = (char *) user_data;
    GPtrArray *files = (GPtrArray *) g_task_propagate_pointer (G_TASK (result), NULL);
    CacheEntry *entry = (CacheEntry *) g_hash_table_lookup (get_cache (), root);
    g_autoptr(GPtrArray) waiters = NULL;
    guint i;

    if (!entry)
    {
        if (files)
            g_ptr_array_unref (files);
        return;
    }

    if (entry->files)
        g_ptr_array_unref (entry->files);

    entry->files = files;
    entry->built_at = g_get_monotonic_time ();
    entry->refreshing = FALSE;

    waiters = entry->waiters;
    entry->waiters = NULL;

    if (!waiters)
        return;

    for (i = 0; i < waiters->len; ++i)
    {
        Waiter *w = (Waiter *) g_ptr_array_index (waiters, i);
        w->func (files, w->user_data);
    }
}

static void
start_refresh (const char            *root,
               CacheEntry            *entry,
               MooFileIndexReadyFunc  func,
               gpointer               user_data)
{
    GTask *task;

    if (func)
    {
        Waiter *w = g_new (Waiter, 1);
        w->func = func;
        w->user_data = user_data;

        if (!entry->waiters)
            entry->waiters = g_ptr_array_new_with_free_func (g_free);

        g_ptr_array_add (entry->waiters, w);
    }

    if (entry->refreshing)
        return;

    entry->refreshing = TRUE;

    task = g_task_new (NULL, NULL, refresh_done, g_strdup (root));
    g_task_set_task_data (task, g_strdup (root), g_free);
    g_task_run_in_thread (task, build_in_thread);
    g_object_unref (task);
}

GPtrArray *
_moo_file_index_get (const char            *root,
                     MooFileIndexReadyFunc  func,
                     gpointer               user_data)
{
    GHashTable *table = get_cache ();
    CacheEntry *entry;

    g_return_val_if_fail (root != NULL, NULL);

    entry = (CacheEntry *) g_hash_table_lookup (table, root);

    if (!entry)
    {
        entry = g_new0 (CacheEntry, 1);
        g_hash_table_insert (table, g_strdup (root), entry);
    }

    if (!entry->files ||
        g_get_monotonic_time () - entry->built_at > MOO_FILE_INDEX_MAX_AGE_SEC * G_USEC_PER_SEC)
        start_refresh (root, entry, func, user_data);

    return entry->files;
}

void
_moo_file_index_forget (const char *root)
{
    g_return_if_fail (root != NULL);
    g_hash_table_remove (get_cache (), root);
}
