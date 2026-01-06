/* eggdesktopfile.c - Freedesktop.Org Desktop Files
 * Copyright (C) 2007 Novell, Inc.
 *
 * Based on gnome-desktop-item.c
 * Copyright (C) 1999, 2000 Red Hat Inc.
 * Copyright (C) 2001 George Lebl
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING.LIB. If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place -
 * Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MOO_DO_NOT_MANGLE_GLIB_FUNCTIONS

#include "eggdesktopfile.h"
#include "mooutils/mooutils-mem.h"

#include <string.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

struct EggDesktopFile {
  GKeyFile           *key_file;
  char               *source;

  char               *name, *icon;
  EggDesktopFileType  type;
  char                document_code;
};

/**
 * egg_desktop_file_new:
 * @desktop_file_path: path to a Freedesktop-style Desktop file
 * @error: error pointer
 *
 * Creates a new #EggDesktopFile for @desktop_file.
 *
 * Return value: the new #EggDesktopFile, or %NULL on error.
 **/
EggDesktopFile *
egg_desktop_file_new (const char *desktop_file_path, GError **error)
{
  GKeyFile *key_file;

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_file (key_file, desktop_file_path, 0, error))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  return egg_desktop_file_new_from_key_file (key_file, desktop_file_path,
					     error);
}

/**
 * egg_desktop_file_new_from_key_file:
 * @key_file: a #GKeyFile representing a desktop file
 * @source: the path or URI that @key_file was loaded from, or %NULL
 * @error: error pointer
 *
 * Creates a new #EggDesktopFile for @key_file. Assumes ownership of
 * @key_file (on success or failure); you should consider @key_file to
 * be freed after calling this function.
 *
 * Return value: the new #EggDesktopFile, or %NULL on error.
 **/
EggDesktopFile *
egg_desktop_file_new_from_key_file (GKeyFile    *key_file,
				    const char  *source,
				    GError     **error)
{
  EggDesktopFile *desktop_file;
  char *version, *type;

  if (!g_key_file_has_group (key_file, EGG_DESKTOP_FILE_GROUP))
    {
      g_set_error (error, EGG_DESKTOP_FILE_ERROR,
		   EGG_DESKTOP_FILE_ERROR_INVALID,
		   _("File is not a valid .desktop file"));
      g_key_file_free (key_file);
      return NULL;
    }

  version = g_key_file_get_value (key_file, EGG_DESKTOP_FILE_GROUP,
				  EGG_DESKTOP_FILE_KEY_VERSION,
				  NULL);
  if (version)
    {
      double version_num;
      char *end;

      version_num = g_ascii_strtod (version, &end);
      if (*end)
	{
	  g_warning ("Invalid Version string '%s' in %s",
		     version, source ? source : "(unknown)");
	}
      else if (version_num > 1.0)
	{
	  g_set_error (error, EGG_DESKTOP_FILE_ERROR,
		       EGG_DESKTOP_FILE_ERROR_INVALID,
		       /* translators: 'Version' is from a desktop file, and
			* should not be translated. '%s' would probably be a
			* version number. */
		       _("Unrecognized desktop file Version '%s'"), version);
	  g_free (version);
	  g_key_file_free (key_file);
	  return NULL;
	}
      g_free (version);
    }

  desktop_file = g_new0 (EggDesktopFile, 1);
  desktop_file->key_file = key_file;

  if (g_path_is_absolute (source))
    desktop_file->source = g_filename_to_uri (source, NULL, NULL);
  else
    desktop_file->source = g_strdup (source);

  desktop_file->name = g_key_file_get_locale_string (key_file,
						     EGG_DESKTOP_FILE_GROUP,
						     EGG_DESKTOP_FILE_KEY_NAME,
						     NULL,
						     error);
  if (!desktop_file->name)
    {
      egg_desktop_file_free (desktop_file);
      return NULL;
    }

  type = g_key_file_get_string (key_file, EGG_DESKTOP_FILE_GROUP,
				EGG_DESKTOP_FILE_KEY_TYPE, error);
  if (!type)
    {
      egg_desktop_file_free (desktop_file);
      return NULL;
    }

  if (!strcmp (type, "Application"))
    {
      char *exec, *p;

      desktop_file->type = EGG_DESKTOP_FILE_TYPE_APPLICATION;

      exec = g_key_file_get_string (key_file,
				    EGG_DESKTOP_FILE_GROUP,
				    EGG_DESKTOP_FILE_KEY_EXEC,
				    error);
      if (!exec)
	{
	  egg_desktop_file_free (desktop_file);
	  g_free (type);
	  return NULL;
	}

      /* See if it takes paths or URIs or neither */
      for (p = exec; *p; p++)
	{
	  if (*p == '%')
	    {
	      if (p[1] == '\0' || strchr ("FfUu", p[1]))
		{
		  desktop_file->document_code = p[1];
		  break;
		}
	      p++;
	    }
	}

      g_free (exec);
    }
  else if (!strcmp (type, "Link"))
    {
      char *url;

      desktop_file->type = EGG_DESKTOP_FILE_TYPE_LINK;

      url = g_key_file_get_string (key_file,
				   EGG_DESKTOP_FILE_GROUP,
				   EGG_DESKTOP_FILE_KEY_URL,
				   error);
      if (!url)
	{
	  egg_desktop_file_free (desktop_file);
	  g_free (type);
	  return NULL;
	}
      g_free (url);
    }
  else if (!strcmp (type, "Directory"))
    desktop_file->type = EGG_DESKTOP_FILE_TYPE_DIRECTORY;
  else
    desktop_file->type = EGG_DESKTOP_FILE_TYPE_UNRECOGNIZED;

  g_free (type);

  /* Check the Icon key */
  desktop_file->icon = g_key_file_get_string (key_file,
					      EGG_DESKTOP_FILE_GROUP,
					      EGG_DESKTOP_FILE_KEY_ICON,
					      NULL);
  if (desktop_file->icon && !g_path_is_absolute (desktop_file->icon))
    {
      char *ext;

      /* Lots of .desktop files still get this wrong */
      ext = strrchr (desktop_file->icon, '.');
      if (ext && (!strcmp (ext, ".png") ||
		  !strcmp (ext, ".xpm") ||
		  !strcmp (ext, ".svg")))
	{
	  g_warning ("Desktop file '%s' has malformed Icon key '%s'"
		     "(should not include extension)",
		     source ? source : "(unknown)",
		     desktop_file->icon);
	  *ext = '\0';
	}
    }

  return desktop_file;
}

/**
 * egg_desktop_file_free:
 * @desktop_file: an #EggDesktopFile
 *
 * Frees @desktop_file.
 **/
void
egg_desktop_file_free (EggDesktopFile *desktop_file)
{
  g_key_file_free (desktop_file->key_file);
  g_free (desktop_file->source);
  g_free (desktop_file->name);
  g_free (desktop_file->icon);
  g_free (desktop_file);
}

/**
 * egg_desktop_file_get_source:
 * @desktop_file: an #EggDesktopFile
 *
 * Gets the URI that @desktop_file was loaded from.
 *
 * Return value: @desktop_file's source URI
 **/
const char *
egg_desktop_file_get_source (EggDesktopFile *desktop_file)
{
  return desktop_file->source;
}

gboolean
egg_desktop_file_get_boolean (EggDesktopFile  *desktop_file,
			      const char      *key,
			      GError         **error)
{
  return g_key_file_get_boolean (desktop_file->key_file,
				 EGG_DESKTOP_FILE_GROUP, key,
				 error);
}


/**
 * egg_desktop_file_accepts_uris:
 * @desktop_file: an #EggDesktopFile
 *
 * Tests if @desktop_file can accept (non-"file:") URIs as documents to
 * open.
 *
 * Return value: %TRUE or %FALSE
 **/
gboolean
egg_desktop_file_accepts_uris (EggDesktopFile *desktop_file)
{
  return (desktop_file->document_code == 'U' ||
	  desktop_file->document_code == 'u');
}

static void
append_quoted_word (GString    *str,
		    const char *s,
		    gboolean    in_single_quotes,
		    gboolean    in_double_quotes)
{
  const char *p;

  if (!in_single_quotes && !in_double_quotes)
    g_string_append_c (str, '\'');
  else if (!in_single_quotes && in_double_quotes)
    g_string_append (str, "\"'");

  if (!strchr (s, '\''))
    g_string_append (str, s);
  else
    {
      for (p = s; *p != '\0'; p++)
	{
	  if (*p == '\'')
	    g_string_append (str, "'\\''");
	  else
	    g_string_append_c (str, *p);
	}
    }

  if (!in_single_quotes && !in_double_quotes)
    g_string_append_c (str, '\'');
  else if (!in_single_quotes && in_double_quotes)
    g_string_append (str, "'\"");
}

static void
do_percent_subst (EggDesktopFile *desktop_file,
		  char            code,
		  GString        *str,
		  GSList        **documents,
		  gboolean        in_single_quotes,
		  gboolean        in_double_quotes)
{
  GSList *d;
  char *doc;

  switch (code)
    {
    case '%':
      g_string_append_c (str, '%');
      break;

    case 'F':
    case 'U':
      for (d = *documents; d; d = d->next)
	{
	  doc = d->data;
	  g_string_append (str, " ");
	  append_quoted_word (str, doc, in_single_quotes, in_double_quotes);
	}
      *documents = NULL;
      break;

    case 'f':
    case 'u':
      if (*documents)
	{
	  doc = (*documents)->data;
	  g_string_append (str, " ");
	  append_quoted_word (str, doc, in_single_quotes, in_double_quotes);
	  *documents = (*documents)->next;
	}
      break;

    case 'i':
      if (desktop_file->icon)
	{
	  g_string_append (str, "--icon ");
	  append_quoted_word (str, desktop_file->icon,
			      in_single_quotes, in_double_quotes);
	}
      break;

    case 'c':
      if (desktop_file->name)
	{
	  append_quoted_word (str, desktop_file->name,
			      in_single_quotes, in_double_quotes);
	}
      break;

    case 'k':
      if (desktop_file->source)
	{
	  append_quoted_word (str, desktop_file->source,
			      in_single_quotes, in_double_quotes);
	}
      break;

    case 'D':
    case 'N':
    case 'd':
    case 'n':
    case 'v':
    case 'm':
      /* Deprecated; skip */
      break;

    default:
      g_warning ("Unrecognized %%-code '%%%c' in Exec", code);
      break;
    }
}

static char *
parse_exec (EggDesktopFile  *desktop_file,
	    GSList         **documents,
	    GError         **error)
{
  char *exec, *p, *command;
  gboolean escape, single_quot, double_quot;
  GString *gs;

  exec = g_key_file_get_string (desktop_file->key_file,
				EGG_DESKTOP_FILE_GROUP,
				EGG_DESKTOP_FILE_KEY_EXEC,
				error);
  if (!exec)
    return NULL;

  /* Build the command */
  gs = g_string_new (NULL);
  escape = single_quot = double_quot = FALSE;

  for (p = exec; *p != '\0'; p++)
    {
      if (escape)
	{
	  escape = FALSE;
	  g_string_append_c (gs, *p);
	}
      else if (*p == '\\')
	{
	  if (!single_quot)
	    escape = TRUE;
	  g_string_append_c (gs, *p);
	}
      else if (*p == '\'')
	{
	  g_string_append_c (gs, *p);
	  if (!single_quot && !double_quot)
	    single_quot = TRUE;
	  else if (single_quot)
	    single_quot = FALSE;
	}
      else if (*p == '"')
	{
	  g_string_append_c (gs, *p);
	  if (!single_quot && !double_quot)
	    double_quot = TRUE;
	  else if (double_quot)
	    double_quot = FALSE;
	}
      else if (*p == '%' && p[1])
	{
	  do_percent_subst (desktop_file, p[1], gs, documents,
			    single_quot, double_quot);
	  p++;
	}
      else
	g_string_append_c (gs, *p);
    }

  g_free (exec);
  command = g_string_free (gs, FALSE);

  /* Prepend "xdg-terminal " if needed (FIXME: use gvfs) */
  if (g_key_file_has_key (desktop_file->key_file,
			  EGG_DESKTOP_FILE_GROUP,
			  EGG_DESKTOP_FILE_KEY_TERMINAL,
			  NULL))
    {
      GError *terminal_error = NULL;
      gboolean use_terminal =
	g_key_file_get_boolean (desktop_file->key_file,
				EGG_DESKTOP_FILE_GROUP,
				EGG_DESKTOP_FILE_KEY_TERMINAL,
				&terminal_error);
      if (terminal_error)
	{
	  g_free (command);
	  g_propagate_error (error, terminal_error);
	  return NULL;
	}

      if (use_terminal)
	{
	  gs = g_string_new ("xdg-terminal ");
	  append_quoted_word (gs, command, FALSE, FALSE);
	  g_free (command);
	  command = g_string_free (gs, FALSE);
	}
    }

  return command;
}

static GSList *
translate_document_list (EggDesktopFile *desktop_file, GSList *documents)
{
  gboolean accepts_uris = egg_desktop_file_accepts_uris (desktop_file);
  GSList *ret, *d;

  for (d = documents, ret = NULL; d; d = d->next)
    {
      const char *document = d->data;
      gboolean is_uri = !g_path_is_absolute (document);
      char *translated;

      if (accepts_uris)
	{
	  if (is_uri)
	    translated = g_strdup (document);
	  else
	    translated = g_filename_to_uri (document, NULL, NULL);
	}
      else
	{
	  if (is_uri)
	    translated = g_filename_from_uri (document, NULL, NULL);
	  else
	    translated = g_strdup (document);
	}

      if (translated)
	ret = g_slist_prepend (ret, translated);
    }

  return g_slist_reverse (ret);
}

static void
free_document_list (GSList *documents)
{
  GSList *d;

  for (d = documents; d; d = d->next)
    g_free (d->data);
  g_slist_free (documents);
}

/**
 * egg_desktop_file_parse_exec:
 * @desktop_file: a #EggDesktopFile
 * @documents: a list of document paths or URIs
 * @error: error pointer
 *
 * Parses @desktop_file's Exec key, inserting @documents into it, and
 * returns the result.
 *
 * If @documents contains non-file: URIs and @desktop_file does not
 * accept URIs, those URIs will be ignored. Likewise, if @documents
 * contains more elements than @desktop_file accepts, the extra
 * documents will be ignored.
 *
 * Return value: the parsed Exec string
 **/
char *
egg_desktop_file_parse_exec (EggDesktopFile  *desktop_file,
			     GSList          *documents,
			     GError         **error)
{
  GSList *translated, *docs;
  char *command;

  docs = translated = translate_document_list (desktop_file, documents);
  command = parse_exec (desktop_file, &docs, error);
  free_document_list (translated);

  return command;
}


GQuark
egg_desktop_file_error_quark (void)
{
  return g_quark_from_static_string ("egg-desktop_file-error-quark");
}


G_LOCK_DEFINE_STATIC (egg_desktop_file);
static EggDesktopFile *egg_desktop_file;

/**
 * egg_get_desktop_file:
 * 
 * Gets the application's #EggDesktopFile, as set by
 * egg_set_desktop_file().
 * 
 * Return value: the #EggDesktopFile, or %NULL if it hasn't been set.
 **/
EggDesktopFile *
egg_get_desktop_file (void)
{
  EggDesktopFile *retval;

  G_LOCK (egg_desktop_file);
  retval = egg_desktop_file;
  G_UNLOCK (egg_desktop_file);

  return retval;
}
