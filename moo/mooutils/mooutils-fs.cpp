/*
 *   mooutils-fs.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-debug.h"
#include "mooutils/mooutils-mem.h"
#include "mooutils/mootype-macros.h"
#include "mooutils/mooi18n.h"
#include <mooutils/mooutils-tests.h>
#include <mooglib/moo-stat.h>
#include <mooglib/moo-glib.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifndef S_IRWXU
#define S_IRWXU 0
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define BROKEN_NAME "<" "????" ">"

MOO_DEFINE_QUARK (moo-file-error-quark, _moo_file_error_quark)

/* XXX fix this */
gboolean
_moo_save_file_utf8 (const char *name,
                     const char *text,
                     gssize      len,
                     GError    **error)
{
    GIOChannel *file;
    GIOStatus status;
    gsize bytes_written;
    gsize real_len;

    file = g_io_channel_new_file (name, "w", error);

    if (!file)
        return FALSE;

    real_len = len < 0 ? strlen (text) : (gsize) len;

    status = g_io_channel_write_chars (file, text,
                                       len, &bytes_written,
                                       error);

    if (status != G_IO_STATUS_NORMAL || bytes_written != real_len)
    {
        /* glib #320668 */
        g_io_channel_flush (file, NULL);
        g_io_channel_shutdown (file, FALSE, NULL);
        g_io_channel_unref (file);
        return FALSE;
    }

    /* glib #320668 */
    g_io_channel_flush (file, NULL);
    g_io_channel_shutdown (file, FALSE, NULL);
    g_io_channel_unref (file);
    return TRUE;
}

static gboolean
rm_fr (const char *path,
       GError    **error)
{
    GError *error_here = NULL;
    char **argv;
    char *child_err;
    int status;

    argv = g_new0 (char*, 5);
    argv[0] = g_strdup ("/bin/rm");
    argv[1] = g_strdup ("-fr");
    argv[2] = g_strdup ("--");
    argv[3] = g_strdup (path);

    if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL,
                       NULL, NULL, NULL, &child_err, &status, &error_here))
    {
        g_set_error (error, MOO_FILE_ERROR, MOO_FILE_ERROR_FAILED,
                     /* This is error message in file selector when rm
                        fails for some strange reason */
                     _("Could not run 'rm' command: %s"),
                     error_here->message);
        g_error_free (error_here);
        g_strfreev (argv);
        return FALSE;
    }

    g_strfreev (argv);

    if (!WIFEXITED (status) || WEXITSTATUS (status))
    {
        if (child_err && strlen (child_err) > 5000)
            strcpy (child_err + 4997, "...");

        g_set_error (error, MOO_FILE_ERROR,
                     MOO_FILE_ERROR_FAILED,
                     /* This is error message in file selector when rm
                        fails to delete a file or folder */
                     _("'rm' command failed: %s"),
                     MOO_NZS (child_err));

        g_free (child_err);
        return FALSE;
    }
    else
    {
        g_free (child_err);
        return TRUE;
    }
}

gboolean
_moo_remove_dir (const char *path,
                 gboolean    recursive,
                 GError    **error)
{
    g_return_val_if_fail (path != NULL, FALSE);

    if (!recursive)
    {
        mgw_errno_t err;

        if (mgw_remove (path, &err) != 0)
        {
            char *path_utf8 = g_filename_display_name (path);
            g_set_error (error, MOO_FILE_ERROR,
                         _moo_file_error_from_errno (err),
                         _("Could not remove %s: %s"),
                         path_utf8, mgw_strerror (err));
            g_free (path_utf8);
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

    return rm_fr (path, error);
}


int
_moo_mkdir_with_parents (const char *path, mgw_errno_t* err)
{
    return mgw_mkdir_with_parents (path, S_IRWXU, err);
}


gboolean
_moo_create_dir (const char *path,
                 GError    **error)
{
    MgwStatBuf buf;
    char *utf8_path;
    mgw_errno_t err;

    g_return_val_if_fail (path != NULL, FALSE);

    if (mgw_stat (path, &buf, &err) != 0 && err.value != MGW_ENOENT)
    {
        utf8_path = g_filename_display_name (path);

        g_set_error (error,
                     MOO_FILE_ERROR,
                     _moo_file_error_from_errno (err),
                     _("Could not create folder %s: %s"),
                     utf8_path, mgw_strerror (err));

        g_free (utf8_path);
        return FALSE;
    }

    if (mgw_errno_is_set (err))
    {
        if (_moo_mkdir (path, &err) == -1)
        {
            utf8_path = g_filename_display_name (path);

            g_set_error (error,
                         MOO_FILE_ERROR,
                         _moo_file_error_from_errno (err),
                         _("Could not create folder %s: %s"),
                         utf8_path, mgw_strerror (err));

            g_free (utf8_path);
            return FALSE;
        }

        return TRUE;
    }

    if (buf.isdir)
        return TRUE;

    utf8_path = g_filename_display_name (path);
    g_set_error (error, MOO_FILE_ERROR,
                 MOO_FILE_ERROR_ALREADY_EXISTS,
                 _("Could not create folder %s: %s"),
                 utf8_path, mgw_strerror (MGW_E_EXIST));
    g_free (utf8_path);

    return FALSE;
}


gboolean
_moo_rename_file (const char *path,
                  const char *new_path,
                  GError    **error)
{
    // Do not break this for directories!
    mgw_errno_t err;

    g_return_val_if_fail (path != NULL, FALSE);
    g_return_val_if_fail (new_path != NULL, FALSE);

    if (mgw_rename (path, new_path, &err) != 0)
    {
        char *utf8_path = g_filename_display_name (path);
        char *utf8_new_path = g_filename_display_name (new_path);

        g_set_error (error,
                     MOO_FILE_ERROR,
                     _moo_file_error_from_errno (err),
                     _("Could not rename file %s to %s: %s"),
                     utf8_path, utf8_new_path, mgw_strerror (err));

        g_free (utf8_path);
        g_free (utf8_new_path);
        return FALSE;
    }

    return TRUE;
}


MooFileError
_moo_file_error_from_errno (mgw_errno_t code)
{
    switch (code.value)
    {
        case MGW_EPERM:
            return MOO_FILE_ERROR_ACCESS_DENIED;
        case MGW_EEXIST:
            return MOO_FILE_ERROR_ALREADY_EXISTS;
        case MGW_ELOOP:
        case MGW_ENAMETOOLONG:
            return MOO_FILE_ERROR_BAD_FILENAME;
        case MGW_ENOENT:
            return MOO_FILE_ERROR_NONEXISTENT;
        case MGW_ENOTDIR:
            return MOO_FILE_ERROR_NOT_FOLDER;
        case MGW_EROFS:
            return MOO_FILE_ERROR_READONLY;
        case MGW_EXDEV:
            return MOO_FILE_ERROR_DIFFERENT_FS;

        default:
            return MOO_FILE_ERROR_FAILED;
    }
}


char **
moo_filenames_from_locale (char **files)
{
    guint i;
    char **conv;

    if (!files)
        return NULL;

    conv = g_new0 (char*, g_strv_length (files) + 1);

    for (i = 0; files && *files; ++files)
    {
        conv[i] = moo_filename_from_locale (*files);

        if (!conv[i])
            g_warning ("could not convert '%s' to UTF8", *files);
        else
            ++i;
    }

    return conv;
}

char *
moo_filename_from_locale (const char *file)
{
    g_return_val_if_fail (file != NULL, NULL);
    return g_strdup (file);
}

char *
_moo_filename_to_uri (const char *file,
                      GError    **error)
{
    char *uri;
    char *freeme = NULL;

    g_return_val_if_fail (file != NULL, NULL);

    if (!_moo_path_is_absolute (file))
    {
        char *cd = g_get_current_dir ();
        file = freeme = g_build_filename (cd, file, nullptr);
        g_free (cd);
    }

    uri = g_filename_to_uri (file, NULL, error);

    g_free (freeme);
    return uri;
}


static char *
normalize_path_string (const char *path)
{
    GPtrArray *comps;
    gboolean first_slash;
    char **pieces, **p;
    char *normpath;

    g_return_val_if_fail (path != NULL, NULL);

    first_slash = (path[0] == G_DIR_SEPARATOR);

    pieces = g_strsplit (path, G_DIR_SEPARATOR_S, 0);
    g_return_val_if_fail (pieces != NULL, NULL);

    comps = g_ptr_array_new ();

    for (p = pieces; *p != NULL; ++p)
    {
        char *s = *p;
        gboolean push = TRUE;
        gboolean pop = FALSE;

        if (!strcmp (s, "") || !strcmp (s, "."))
        {
            push = FALSE;
        }
        else if (!strcmp (s, ".."))
        {
            if (!comps->len && first_slash)
            {
                push = FALSE;
            }
            else if (comps->len)
            {
                push = FALSE;
                pop = TRUE;
            }
        }

        if (pop)
        {
            g_free (comps->pdata[comps->len - 1]);
            g_ptr_array_remove_index (comps, comps->len - 1);
        }

        if (push)
            g_ptr_array_add (comps, g_strdup (s));
    }

    g_ptr_array_add (comps, NULL);

    if (comps->len == 1)
    {
        if (first_slash)
            normpath = g_strdup (G_DIR_SEPARATOR_S);
        else
            normpath = g_strdup (".");
    }
    else
    {
        char *tmp = g_strjoinv (G_DIR_SEPARATOR_S, (char**) comps->pdata);

        if (first_slash)
        {
            guint len = strlen (tmp);
            normpath = g_renew (char, tmp, len + 2);
            memmove (normpath + 1, normpath, len + 1);
            normpath[0] = G_DIR_SEPARATOR;
        }
        else
        {
            normpath = tmp;
        }
    }

    g_strfreev (pieces);
    g_strfreev ((char**) comps->pdata);
    g_ptr_array_free (comps, FALSE);

    return normpath;
}

static char *
normalize_full_path_unix (const char *path)
{
    guint len;
    char *normpath;

    g_return_val_if_fail (path != NULL, NULL);

    normpath = normalize_path_string (path);
    g_return_val_if_fail (normpath != NULL, NULL);

    len = strlen (normpath);
    g_return_val_if_fail (len > 0, normpath);

    if (len > 1 && normpath[len-1] == G_DIR_SEPARATOR)
        normpath[len-1] = 0;

    return normpath;
}

static char *
normalize_path (const char *filename)
{
    char *freeme = NULL;
    char *norm_filename;

    g_assert (filename && filename[0]);

    if (!_moo_path_is_absolute (filename))
    {
        char *working_dir = g_get_current_dir ();
        g_return_val_if_fail (working_dir != NULL, g_strdup (filename));
        freeme = g_build_filename (working_dir, filename, nullptr);
        filename = freeme;
        g_free (working_dir);
    }

    norm_filename = normalize_full_path_unix (filename);

    g_free (freeme);
    return norm_filename;
}

char *
_moo_normalize_file_path (const char *filename)
{
    g_return_val_if_fail (filename != NULL, NULL);
    /* empty filename is an error, but we don't want to crash here */
    g_return_val_if_fail (filename[0] != 0, g_strdup (""));
    return normalize_path (filename);
}

gboolean
_moo_path_is_absolute (const char *path)
{
    g_return_val_if_fail (path != NULL, FALSE);
    return g_path_is_absolute (path)
        ;
}

int
_moo_mkdir (const char *path, mgw_errno_t *err)
{
    return mgw_mkdir (path, S_IRWXU, err);
}


/***********************************************************************/
/* Glob matching
 */

#define MOO_GLOB_REGEX
#include <mooglib/moo-glib.h>

typedef struct _MooGlob {
#ifdef MOO_GLOB_REGEX
    GRegex *re;
#else
    char *pattern;
#endif
} MooGlob;

#ifdef MOO_GLOB_REGEX
static char *
glob_to_re (const char *pattern)
{
    GString *string;
    const char *p, *piece, *bracket;
    char *escaped;

    g_return_val_if_fail (pattern != NULL, NULL);

    p = piece = pattern;
    string = g_string_new (NULL);

    while (*p)
    {
        switch (*p)
        {
            case '*':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append (string, ".*");
                piece = ++p;
                break;

            case '?':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append_c (string, '.');
                piece = ++p;
                break;

            case '[':
                if (!(bracket = strchr (p + 1, ']')))
                {
                    g_warning ("in %s: unmatched '['", pattern);
                    goto error;
                }

                if (p != piece)
                    g_string_append_len (string, piece, p - piece);

                g_string_append_c (string, '[');
                if (p[1] == '^')
                {
                    g_string_append_c (string, '^');
                    escaped = g_regex_escape_string (p + 2, (int) (bracket - p - 2));
                }
                else
                {
                    escaped = g_regex_escape_string (p + 1, (int) (bracket - p - 1));
                }
                g_string_append (string, escaped);
                g_free (escaped);
                g_string_append_c (string, ']');
                piece = p = bracket + 1;
                break;

            case '\\':
            case '|':
            case '(':
            case ')':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '+':
            case '.':
                if (p != piece)
                    g_string_append_len (string, piece, p - piece);
                g_string_append_c (string, '\\');
                g_string_append_c (string, *p);
                piece = ++p;
                break;

            default:
                p = g_utf8_next_char (p);
        }
    }

    if (*piece)
        g_string_append (string, piece);

    g_string_append_c (string, '$');

    if (0)
        _moo_message ("converted '%s' to '%s'\n", pattern, string->str);

    return g_string_free (string, FALSE);

error:
    g_string_free (string, TRUE);
    return NULL;
}


static MooGlob *
_moo_glob_new (const char *pattern)
{
    MooGlob *gl;
    GRegex *re;
    char *re_pattern;
    GRegexCompileFlags flags = (GRegexCompileFlags)  0;
    GError *error = NULL;

    g_return_val_if_fail (pattern != NULL, NULL);

    if (!(re_pattern = glob_to_re (pattern)))
        return NULL;

    re = g_regex_new (re_pattern, flags, (GRegexMatchFlags) 0, &error);

    g_free (re_pattern);

    if (!re)
    {
        g_warning ("%s", moo_error_message (error));
        g_error_free (error);
        return NULL;
    }

    gl = g_new0 (MooGlob, 1);
    gl->re = re;

    return gl;
}


static gboolean
_moo_glob_match (MooGlob    *glob,
                 const char *filename_utf8)
{
    g_return_val_if_fail (glob != NULL, FALSE);
    g_return_val_if_fail (filename_utf8 != NULL, FALSE);
    g_return_val_if_fail (g_utf8_validate (filename_utf8, -1, NULL), FALSE);

    return g_regex_match (glob->re, filename_utf8, (GRegexMatchFlags) 0, NULL);
}
#endif


static void
_moo_glob_free (MooGlob *glob)
{
    if (glob)
    {
#ifdef MOO_GLOB_REGEX
        g_regex_unref (glob->re);
#else
        g_free (glob->pattern);
#endif
        g_free (glob);
    }
}


gboolean
_moo_glob_match_simple (const char *pattern,
                        const char *filename)
{
    MooGlob *gl;
    gboolean result = FALSE;

    g_return_val_if_fail (pattern != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);

    if ((gl = _moo_glob_new (pattern)))
        result = _moo_glob_match (gl, filename);

#if 0
    if (result && 0)
        _moo_message ("'%s' matched '%s'", filename, pattern);
#endif

    _moo_glob_free (gl);
    return result;
}
