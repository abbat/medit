/*
 *   moofileindex.h
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

/*
 * Every file under a project root, for Quick Open. git ls-files when the
 * root is a git work tree, a plain directory walk otherwise.
 */

#ifndef MOO_FILE_INDEX_H
#define MOO_FILE_INDEX_H

#include "mooglib/moo-glib.h"

G_BEGIN_DECLS


#define MOO_FILE_INDEX_MAX_FILES ((guint) 200000)

/*
 * Every file under root, as paths relative to root: git ls-files (tracked and
 * untracked, minus .gitignore) when root is a git work tree, a recursive
 * directory walk otherwise. A symlinked directory is never entered, which is
 * also what keeps a symlink cycle from looping the walk. Stops once it has
 * max_files entries; 0 means MOO_FILE_INDEX_MAX_FILES.
 *
 * Free the result with g_ptr_array_unref().
 */
GPtrArray      *_moo_file_index_build        (const char *root,
                                              guint       max_files);

typedef void (*MooFileIndexReadyFunc) (GPtrArray *files,
                                       gpointer   user_data);

/*
 * Per-root cache, stale-while-revalidate. Returns the cached list right away
 * -- NULL the first time root is asked for -- and, when there is none yet or
 * the cached one is older than MOO_FILE_INDEX_MAX_AGE_SEC, starts a
 * background rebuild and calls func with the fresh list once it lands. A
 * rebuild already running for root is not started twice.
 *
 * The returned array is owned by the cache; do not free or modify it.
 */
GPtrArray      *_moo_file_index_get          (const char            *root,
                                              MooFileIndexReadyFunc  func,
                                              gpointer               user_data);

/* Drops the cached entry for root, if any. */
void            _moo_file_index_forget       (const char *root);


G_END_DECLS

#endif /* MOO_FILE_INDEX_H */
