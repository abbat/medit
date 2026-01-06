/*
 *   medit-app.c
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

#include "main.h"
#include "mooapp/mooapp.h"
#include "moocpp/regex.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"
#include "plugins/mooplugin-builtin.h"

/**
 * @brief Structure to store medit command line options
 *
 * Contains all parameters that can be passed to the application
 * via command line or other methods.
 */
struct MeditOpts
{
  int use_session = -1;                /**< Whether to load and save session (1=yes, 0=no, -1=not specified) */
  int pid = -1;                        /**< Process ID of existing instance to connect to */
  gboolean new_app = false;            /**< Whether to start a new instance of the application */
  const char *instance_name = nullptr; /**< Name of the application instance */
  gboolean new_window = false;         /**< Whether to open files in a new window */
  gboolean new_tab = false;            /**< Whether to open files in a new tab */
  gboolean reload = false;             /**< Whether to automatically reload files if modified on disk */
  int line = 0;                        /**< Line number to position cursor at when opening file */
  const char *encoding = nullptr;      /**< Character encoding to use for files */
  const char *log_file = nullptr;      /**< Path to file for writing debug output */
  gboolean log_window = false;         /**< Whether to show debug output in a window */
  gstrvec files;                       /**< List of files to open */
  char **filesp = nullptr;             /**< Pointer to array of files (temporary, used during parsing) */
  const char *geometry = nullptr;      /**< Window geometry specification (WIDTHxHEIGHT[+X+Y]) */
  gboolean show_version = false;       /**< Whether to display version information and exit */
  const char *debug = nullptr;         /**< Debug mode options */
  char **run_script = nullptr;         /**< Scripts to run on startup */
  char **send_script = nullptr;        /**< Scripts to send to existing instance */
};

/**
 * @brief Global instance of command line options
 *
 * Stores all command line parameters and options passed to the application.
 * This structure is initialized during argument parsing and used throughout
 * the application lifecycle.
 */
static MeditOpts medit_opts;

/**
 * @brief Type definition for MeditApp
 *
 * MeditApp is a typedef for MooApp, providing a type-safe way to reference
 * the application instance in medit-specific code.
 */
typedef MooApp MeditApp;

/**
 * @brief Type definition for MeditAppClass
 *
 * MeditAppClass is a typedef for MooAppClass, providing a type-safe way to reference
 * the application class in medit-specific code.
 */
typedef MooAppClass MeditAppClass;

/**
 * @brief Macro to define MeditApp type
 *
 * This macro defines the MeditApp type system integration with GObject.
 * It creates the necessary type information for the MeditApp class,
 * including initialization functions and parent class (MOO_TYPE_APP).
 */
G_DEFINE_TYPE (MeditApp, medit_app, MOO_TYPE_APP)

/**
 * @brief Initialize application plugins
 *
 * @param app Pointer to the MooApp application object
 */
static void
medit_app_init_plugins (MooApp *app)
{
  (void) app;

  moo_plugin_init ();
}

/**
 * @brief Initialize application class
 *
 * Sets the pointer to the plugin initialization function
 *
 * @param klass Pointer to the MooAppClass application class
 */
static void
medit_app_class_init (MooAppClass *klass)
{
  klass->init_plugins = medit_app_init_plugins;
}

/**
 * @brief Initialize application instance
 *
 * @param app Pointer to the MooApp application object
 */
static void
medit_app_init (MooApp *app)
{
  (void) app;
}

/**
 * @brief Parser for the session usage option
 *
 * Parses the value of the --use-session option and sets the corresponding flag
 *
 * @param option_name Name of the option
 * @param value Option value ("yes", "no" or NULL)
 * @param data User data (not used)
 * @param error Pointer to error structure to return error information
 * @return TRUE on success, FALSE on error
 */
static gboolean
parse_use_session (const char *option_name, const char *value, gpointer data, GError **error)
{
  (void) data;

  if (!value || strcmp (value, "yes") == 0)
    {
      medit_opts.use_session = TRUE;
      return TRUE;
    }
  else if (strcmp (value, "no") == 0)
    {
      medit_opts.use_session = FALSE;
      return TRUE;
    }

  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
               /* error message for wrong command line */
               _ ("Invalid value '%s' for option %s"), value, option_name);

  return FALSE;
}

/**
 * @brief Check for +<number> argument
 *
 * Looks for a command line argument in the format +<number> and interprets it
 * as the --line <number> option. If a file with such name exists, it is not processed.
 *
 * Example:
 *   medit file.txt +42
 *   This will open file.txt and position cursor at line 42
 *
 * Result:
 *   medit_opts.line will be set to 42
 *   "+42" will be removed from medit_opts.files
 */
static void
check_plus_line_arg (void)
{
  g::Regex re = g::Regex::compile ("^\\+(?P<line>\\d+)", g::Regex::OPTIMIZE | g::Regex::DUPNAMES);
  g_return_if_fail (re.is_valid ());

  for (size_t i = 0; i < medit_opts.files.size (); ++i)
    {
      const gstr &file = medit_opts.files[i];
      if (g::MatchInfo match_info = re.match (file))
        {
          int line = 0;
          gstr line_string = match_info.fetch_named ("line");

          errno = 0;
          line = strtol (line_string.get (), NULL, 10);
          if (errno != 0)
            line = 0;

          // if a file "+10" exists, open it
          if (line > 0 && g_file_test (file.get (), G_FILE_TEST_EXISTS))
            line = 0;

          if (line > 0)
            {
              medit_opts.line = line;
              medit_opts.files.erase (medit_opts.files.begin () + i);
              return;
            }
        }
    }
}

/**
 * @brief Post-processing function for command line arguments
 *
 * Performs additional processing of arguments after the main parsing:
 * - Converts file array to vector
 * - Processes version display option
 * - Checks for conflicting options
 * - Sets environment variables for debugging
 * - Checks for filename +<line-number> arguments
 *
 * @param context Option context (not used)
 * @param group Option group (not used)
 * @param data User data (not used)
 * @param error Pointer to error structure (not used)
 * @return TRUE on success
 */
static gboolean
post_parse_func (GOptionContext *, GOptionGroup *, void *, GError **)
{
  medit_opts.files = gstr::take (medit_opts.filesp);
  medit_opts.filesp = nullptr;

  if (medit_opts.show_version)
    {
      g_print ("medit " MOO_DISPLAY_VERSION "\n");
      exit (EXIT_SUCCESS);
    }

  if (medit_opts.pid > 0 && medit_opts.instance_name)
    {
      /* error message for wrong commmand line */
      g_printerr (_ ("%s and %s options may not be used simultaneously\n"),
                  "--app-name", "--pid");

      exit (EXIT_FAILURE);
    }

  if (medit_opts.debug)
    g_setenv ("MOO_DEBUG", medit_opts.debug, FALSE);

  check_plus_line_arg ();

  return TRUE;
}

/**
 * @brief Parse command line arguments
 *
 * Creates option context, defines all available command line options,
 * sets callback functions and performs argument parsing.
 *
 * @param argc Number of arguments
 * @param argv Array of arguments
 * @return Pointer to option context
 */
static GOptionContext *
parse_args (int argc, char *argv[])
{
  GError *error = nullptr;
  GOptionGroup *grp;
  GOptionContext *ctx;

  GOptionEntry medit_options[] = {
    { "new-app", 'n', 0, G_OPTION_ARG_NONE, &medit_opts.new_app, N_ ("Run new instance of application"), NULL },
    { "use-session", 's', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, (void *) parse_use_session, N_ ("Load and save session"), "yes|no" },
    { "pid", 0, 0, G_OPTION_ARG_INT, &medit_opts.pid, N_ ("Use existing instance with process id PID"), N_ ("PID") },
    { "app-name", 0, 0, G_OPTION_ARG_STRING, (gpointer) &medit_opts.instance_name, N_ ("Set instance name to NAME if it's not already running"), N_ ("NAME") },
    { "new-window", 'w', 0, G_OPTION_ARG_NONE, &medit_opts.new_window, N_ ("Open file(s) in a new window"), NULL },
    { "new-tab", 't', 0, G_OPTION_ARG_NONE, &medit_opts.new_tab, N_ ("Open file(s) in a new tab"), NULL },
    { "line", 'l', 0, G_OPTION_ARG_INT, &medit_opts.line, N_ ("Open file and position cursor on line LINE"), N_ ("LINE") },
    { "encoding", 'e', 0, G_OPTION_ARG_STRING, (gpointer) &medit_opts.encoding, N_ ("Use character encoding ENCODING"), N_ ("ENCODING") },
    { "reload", 'r', 0, G_OPTION_ARG_NONE, &medit_opts.reload, N_ ("Automatically reload file if it was modified on disk"), NULL },
    { "run-script", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING_ARRAY, (gpointer) &medit_opts.run_script, "Run SCRIPT", "SCRIPT" },
    { "send-script", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING_ARRAY, (gpointer) &medit_opts.send_script, "Send SCRIPT to existing instance", "SCRIPT" },
    { "log-window", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &medit_opts.log_window, "Show debug output", NULL },
    { "log-file", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME, (gpointer) &medit_opts.log_file, "Write debug output to FILE", "FILE" },
    { "debug", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, (gpointer) &medit_opts.debug, "Run in debug mode", NULL },
    { "geometry", 0, 0, G_OPTION_ARG_STRING, (gpointer) &medit_opts.geometry, N_ ("Default window size and position"), N_ ("WIDTHxHEIGHT[+X+Y]") },
    { "version", 0, 0, G_OPTION_ARG_NONE, &medit_opts.show_version, N_ ("Show version information and exit"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &medit_opts.filesp, NULL, N_ ("FILES") },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
  };

  grp = g_option_group_new ("medit", "medit", "medit", NULL, NULL);
  g_option_group_add_entries (grp, medit_options);
  g_option_group_set_parse_hooks (grp, NULL, (GOptionParseFunc) post_parse_func);
  g_option_group_set_translation_domain (grp, GETTEXT_PACKAGE);

  ctx = g_option_context_new (NULL);
  g_option_context_set_main_group (ctx, grp);
  g_option_context_add_group (ctx, gtk_get_option_group (FALSE));

  if (!g_option_context_parse (ctx, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      exit (EXIT_FAILURE);
    }

  return ctx;
}

/**
 * @brief Notify about application startup completion
 *
 * Sends notification to the desktop environment that the application
 * has finished starting up and is ready to work. Works only in X11 environment.
 */
static void
notify_startup_complete (void)
{
#ifdef GDK_WINDOWING_X11
  const char *v = g_getenv ("DESKTOP_STARTUP_ID");

  if (v && *v)
    {
      gtk_init (NULL, NULL);
      gdk_notify_startup_complete ();
    }
#endif
}

/**
 * @brief Get startup timestamp
 *
 * Extracts timestamp from the DESKTOP_STARTUP_ID environment variable.
 * Used for synchronization with the desktop environment during startup.
 * Works only in X11 environment.
 *
 * @return Timestamp or 0 in case of error
 */
static guint32
get_time_stamp (void)
{
#ifdef GDK_WINDOWING_X11
  char *end;
  char *time_str;
  gulong stamp;
  const char *startup_id;

  startup_id = g_getenv ("DESKTOP_STARTUP_ID");
  if (!startup_id || !startup_id[0])
    return 0;

  if (!(time_str = g_strrstr (startup_id, "_TIME")))
    return 0;

  errno = 0;
  time_str += 5;
  stamp = strtoul (time_str, &end, 0);

  return !*end && !errno ? stamp : 0;
#else
  return 0;
#endif
}

/**
 * @brief Execute scripts on startup
 *
 * Executes all scripts specified in the --run-script option.
 * Called as a callback after application startup.
 */
static void
run_script_func (void)
{
  char **p;
  for (p = medit_opts.run_script; p && *p; ++p)
    moo_app_run_script (moo_app_instance (), *p);
}

/**
 * @brief Install logging handlers
 *
 * Sets up handlers for outputting debug information
 * to a file or a separate window depending on command line options.
 */
static void
install_log_handlers (void)
{
  if (medit_opts.log_file)
    moo_set_log_func_file (medit_opts.log_file);
  else if (medit_opts.log_window)
    moo_set_log_func_window (TRUE);
}

/**
 * @brief Parse filename and create opening information
 *
 * Converts file path to URI and extracts line number from filename
 * if it contains "file:line" or "file(line)" format.
 *
 * Example:
 *   Input: "/home/user/document.txt:42"
 *   Result: Creates MooOpenInfo with URI "file:///home/user/document.txt" and line 41
 *
 *   Input: "/home/user/source.c(100)"
 *   Result: Creates MooOpenInfo with URI "file:///home/user/source.c" and line 99
 *
 * @param filename Filename to parse
 * @return Pointer to MooOpenInfo structure or NULL on error
 */
static MooOpenInfo *
parse_filename (const char *filename)
{
  int line = 0;
  char *uri;
  char *freeme1 = NULL;
  char *freeme2 = NULL;
  MooOpenInfo *info;

  freeme1 = _moo_normalize_file_path (filename);
  filename = freeme1;

  if (g_str_has_suffix (filename, "/") || g_file_test (filename, G_FILE_TEST_IS_DIR))
    {
      g_free (freeme1);
      g_free (freeme2);

      return NULL;
    }

  if (!g_file_test (filename, G_FILE_TEST_EXISTS) && g_utf8_validate (filename, -1, NULL))
    {
      GError *error = NULL;
      GRegex *re = g_regex_new ("((?P<path>.*):(?P<line>\\d+)?|(?P<path>.*)\\((?P<line>\\d+)\\))$",
                                GRegexCompileFlags (G_REGEX_OPTIMIZE | G_REGEX_DUPNAMES),
                                GRegexMatchFlags (0), &error);
      if (!re)
        {
          g_critical ("could not compile regex: %s", error->message);
          g_error_free (error);
        }
      else
        {
          GMatchInfo *match_info = NULL;

          if (g_regex_match (re, filename, GRegexMatchFlags (0), &match_info))
            {
              char *path = g_match_info_fetch_named (match_info, "path");
              char *line_string = g_match_info_fetch_named (match_info, "line");

              if (path && *path)
                {
                  path = NULL;
                  filename = path;
                  freeme2 = path;

                  if (line_string && *line_string)
                    {
                      errno = 0;
                      line = strtol (line_string, NULL, 10);
                      if (errno)
                        line = 0;
                    }
                }

              g_free (line_string);
              g_free (path);
            }

          g_match_info_free (match_info);
          g_regex_unref (re);
        }
    }

  if (!(uri = g_filename_to_uri (filename, NULL, NULL)))
    {
      g_critical ("could not convert filename to URI");

      g_free (freeme1);
      g_free (freeme2);

      return NULL;
    }

  info = moo_open_info_new_uri (uri, NULL, line - 1, MOO_OPEN_FLAGS_NONE);

  g_free (uri);
  g_free (freeme1);
  g_free (freeme2);

  return info;
}

/**
 * @brief Parse options from URI
 *
 * Extracts options (line number, opening flags) from URI parameter string.
 *
 * @param optstring String with options in format "option=value;option=value"
 * @param info Pointer to MooOpenInfo structure to update
 */
static void
parse_options_from_uri (const char *optstring, MooOpenInfo *info)
{
  char **p, **comps;

  comps = g_strsplit (optstring, ";", 0);

  for (p = comps; p && *p; ++p)
    {
      if (!strncmp (*p, "line=", strlen ("line=")))
        {
          /* doesn't matter if there is an error */
          moo_open_info_set_line (info, strtoul (*p + strlen ("line="), NULL, 10) - 1);
        }
      else if (!strncmp (*p, "options=", strlen ("options=")))
        {
          char **opts, **op;
          opts = g_strsplit (*p + strlen ("options="), ",", 0);
          for (op = opts; op && *op; ++op)
            {
              if (!strcmp (*op, "new-window"))
                moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_WINDOW);
              else if (!strcmp (*op, "new-tab"))
                moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_TAB);
            }

          g_strfreev (opts);
        }
    }

  g_strfreev (comps);
}

/**
 * @brief Parse URI and create opening information
 *
 * Parses URI, extracts options from query string and creates MooOpenInfo structure.
 * For schemes other than "file", creates a simple structure.
 *
 * @param scheme URI scheme (e.g., "file")
 * @param uri Full URI to parse
 * @return Pointer to MooOpenInfo structure
 */
static MooOpenInfo *
parse_uri (const char *scheme, const char *uri)
{
  char *real_uri;
  const char *question_mark;
  const char *optstring = NULL;
  MooOpenInfo *info;

  if (strcmp (scheme, "file") != 0)
    return moo_open_info_new_uri (uri, NULL, -1, MOO_OPEN_FLAGS_NONE);

  question_mark = strchr (uri, '?');

  if (question_mark && question_mark > uri)
    {
      real_uri = g_strndup (uri, question_mark - uri);
      optstring = question_mark + 1;
    }
  else
    {
      real_uri = g_strdup (uri);
    }

  info = moo_open_info_new_uri (real_uri, NULL, -1, MOO_OPEN_FLAGS_NONE);

  if (optstring)
    parse_options_from_uri (optstring, info);

  g_free (real_uri);
  return info;
}

/**
 * @brief Extract scheme from URI
 *
 * Determines the URI scheme (part before the colon).
 *
 * @param string URI string to analyze
 * @return Extracted scheme or NULL if scheme not found
 */
static char *
parse_uri_scheme (const char *string)
{
  const char *p;

  for (p = string; *p; ++p)
    {
      if (*p == ':')
        {
          if (p != string)
            return g_strndup (string, p - string);

          break;
        }

      if (!(p != string && g_ascii_isalnum (*p)) &&
          !(p == string && g_ascii_isalpha (*p)))
        break;
    }

  return NULL;
}

/**
 * @brief Parse file path or URI
 *
 * Determines the type of the passed string (absolute path, relative path or URI)
 * and calls the appropriate function for parsing.
 *
 * @param string String with file path or URI
 * @param current_dir Pointer to current working directory (may be updated)
 * @return Pointer to MooOpenInfo structure or NULL on error
 */
static MooOpenInfo *
parse_file (const char *string, char **current_dir)
{
  char *filename;
  char *uri_scheme;
  MooOpenInfo *ret;

  if (g_path_is_absolute (string))
    return parse_filename (string);

  if ((uri_scheme = parse_uri_scheme (string)))
    {
      ret = parse_uri (uri_scheme, string);
      g_free (uri_scheme);
      return ret;
    }

  if (!*current_dir)
    *current_dir = g_get_current_dir ();

  filename = g_build_filename (*current_dir, string, nullptr);
  ret = parse_filename (filename);

  g_free (filename);
  return ret;
}

/**
 * @brief Parse all files from command line options
 *
 * Processes the list of files from the command line, creates MooOpenInfo structure
 * for each and applies global options (encoding, line number, etc.).
 *
 * @return Pointer to MooOpenInfoArray or NULL if no files
 */
static MooOpenInfoArray *
parse_files (void)
{
  int i;
  int n_files;
  char *current_dir = NULL;
  MooOpenInfoArray *files;

  if (medit_opts.files.empty () || !(n_files = (int) medit_opts.files.size ()))
    return NULL;

  files = moo_open_info_array_new ();

  for (i = 0; i < n_files; ++i)
    {
      MooOpenInfo *info;

      info = parse_file (medit_opts.files[i].get (), &current_dir);

      if (!info)
        continue;

      if (medit_opts.new_window)
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_WINDOW);
      if (medit_opts.new_tab)
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_TAB);
      if (medit_opts.reload)
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_RELOAD);

      if (moo_open_info_get_line (info) < 0)
        moo_open_info_set_line (info, medit_opts.line - 1);

      if (!moo_open_info_get_encoding (info) && medit_opts.encoding && medit_opts.encoding[0])
        moo_open_info_set_encoding (info, medit_opts.encoding);

      moo_open_info_array_take (files, info);
    }

  g_free (current_dir);
  return files;
}

/**
 * @brief Main function of the medit application
 *
 * Performs application initialization, command line argument parsing,
 * file processing, launching a new instance or connecting to an existing one.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 * @return Application exit code
 */
int
medit_app_main (int argc, char *argv[])
{
  int retval;
  char pid_buf[32];
  guint32 stamp;
  MooEditor *editor;
  MooApp *app = NULL;
  GOptionContext *ctx;
  const char *name = NULL;
  MooOpenInfoArray *files;
  gboolean run_input = TRUE;
  gboolean new_instance = FALSE;

  stamp = get_time_stamp ();

  ctx = parse_args (argc, argv);

  if (medit_opts.new_app)
    new_instance = TRUE;

  run_input = !medit_opts.new_app || medit_opts.instance_name || medit_opts.use_session == 1;

  if (medit_opts.pid > 0)
    {
      sprintf (pid_buf, "%d", medit_opts.pid);
      name = pid_buf;
    }
  else if (medit_opts.instance_name)
    name = medit_opts.instance_name;
  else if (!medit_opts.new_app)
    name = g_getenv ("MEDIT_PID");

  if (name && !name[0])
    name = NULL;

  if (medit_opts.send_script)
    {
      char **p;
      for (p = medit_opts.send_script; *p; ++p)
        {
          GString *msg = g_string_new ("e");
          g_string_append (msg, *p);
          moo_app_send_msg (name, msg->str, msg->len + 1);
          g_string_free (msg, TRUE);
        }

      notify_startup_complete ();
      exit (EXIT_SUCCESS);
    }

  files = parse_files ();
  if (name)
    {
      if (moo_app_send_files (files, stamp, name))
        exit (EXIT_SUCCESS);

      if (!medit_opts.instance_name)
        {
          g_printerr ("Could not send files to instance '%s'\n", name);
          exit (EXIT_FAILURE);
        }
    }

  if (!new_instance && !medit_opts.instance_name &&
      moo_app_send_files (files, stamp, NULL))
    {
      notify_startup_complete ();
      exit (EXIT_SUCCESS);
    }

  gtk_init (NULL, NULL);

  install_log_handlers ();

  app = MOO_APP (g_object_new (medit_app_get_type (),
                               "run-input", run_input,
                               "use-session", medit_opts.use_session,
                               "instance-name", medit_opts.instance_name,
                               (const char *) NULL));

  if (!moo_app_init (app))
    {
      gdk_notify_startup_complete ();
      g_object_unref (app);
      exit (EXIT_FAILURE);
    }

  if (medit_opts.geometry && *medit_opts.geometry)
    moo_window_set_default_geometry (medit_opts.geometry);

  moo_app_load_session (app);

  editor = moo_app_get_editor (app);
  if (!moo_editor_get_active_window (editor))
    moo_editor_new_window (editor);

  if (files)
    moo_app_open_files (app, files, stamp);

  moo_open_info_array_free (files);
  g_option_context_free (ctx);

  if (medit_opts.run_script)
    g_signal_connect (app, "started", G_CALLBACK (run_script_func), NULL);

  retval = moo_app_run (app);

  g_object_unref (app);

  return retval;
}
