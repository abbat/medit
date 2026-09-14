/*
 *   moo-mime.c
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

#include "mooutils/moo-mime.h"
#include "mooglib/moo-stat.h"

G_LOCK_DEFINE (moo_mime);

static const char *
mime_type_intern (const char *mime);

static const char *
mime_type_from_content_type (const char *content_type)
{
    char *mime_type;
    const char *interned;

    if (!content_type || g_content_type_is_unknown (content_type))
        return MOO_MIME_TYPE_UNKNOWN;

    mime_type = g_content_type_get_mime_type (content_type);
    interned = mime_type_intern (mime_type);
    g_free (mime_type);
    return interned;
}

const char *
moo_mime_type_unknown (void)
{
    return "application/octet-stream";
}

/* The one piece of shared state this file has, and so the only thing
   G_LOCK (moo_mime) is for -- glib's own content type functions do their
   locking themselves. The strings are interned for the lifetime of the
   process: callers compare the results by pointer. */
static const char *
mime_type_intern (const char *mime)
{
    static GHashTable *hash;
    const char *interned;

    if (mime == NULL || !strcmp (mime, MOO_MIME_TYPE_UNKNOWN))
        return MOO_MIME_TYPE_UNKNOWN;

    G_LOCK (moo_mime);

    if (G_UNLIKELY (!hash))
        hash = g_hash_table_new (g_str_hash, g_str_equal);

    if (G_UNLIKELY (!(interned = (const char*) g_hash_table_lookup (hash, mime))))
    {
        char *copy = g_strdup (mime);
        g_hash_table_insert (hash, copy, copy);
        interned = copy;
    }

    G_UNLOCK (moo_mime);

    return interned;
}

const char *
moo_get_mime_type_for_file (const char *filename,
                            MgwStatBuf *statbuf)
{
    const char *mime;
    GFile *file;
    GFileInfo *info;
    char *content_type;

    if (filename == NULL)
        return MOO_MIME_TYPE_UNKNOWN;

    if (statbuf && !statbuf->isreg)
        return MOO_MIME_TYPE_UNKNOWN;

    file = g_file_new_for_path (filename);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                              G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (info)
        mime = mime_type_from_content_type (g_file_info_get_content_type (info));
    else
    {
        content_type = g_content_type_guess (filename, NULL, 0, NULL);
        mime = mime_type_from_content_type (content_type);
        g_free (content_type);
    }
    if (info)
        g_object_unref (info);
    g_object_unref (file);

    return mime;
}

const char *
moo_get_mime_type_for_filename (const char *filename)
{
    const char *mime;
    char *content_type;

    if (filename == NULL)
        return NULL;

    content_type = g_content_type_guess (filename, NULL, 0, NULL);
    mime = mime_type_from_content_type (content_type);
    g_free (content_type);

    return mime;
}

gboolean
moo_mime_type_is_subclass (const char *mime_type,
                           const char *base)
{
    gboolean ret;

    if (mime_type == NULL || base == NULL)
        return FALSE;

    ret = !strcmp (mime_type, base) ||
          (g_str_has_suffix (base, "/*") &&
           !strncmp (mime_type, base, strlen (base) - 1)) ||
          g_content_type_is_a (mime_type, base);
    return ret;
}
