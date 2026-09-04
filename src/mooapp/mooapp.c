/*
 *   mooapp/mooapp.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *                 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#include "mooapp.h"

#include "about.h"
#include "eggsmclient/eggsmclient.h"
#include "marshals.h"
#include "mooapp-accels.h"
#include "mooapp-info.h"
#include "mooapp-private.h"
#include "mooedit/mooeditprefs.h"
#include "mooedit/mooplugin.h"
#include "mooutils/moo-mime.h"
#include "mooutils/mooappinput.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moohelp.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooprefsdialog.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/mooutils-script.h"

/*!< \brief Name of the UI XML file */
const char *MOO_UI_XML_FILE = "ui.xml";

/*!< \brief Version string for session files */
const char *SESSION_VERSION = "1.0";

/*!< \brief Version string for application commands */
const char *MOO_APP_CMD_VERSION = "1.0";

/*!< \brief Preferences key for asking about opening bug report URL */
const char *ASK_OPEN_BUG_URL_KEY = "Application/ask_open_bug_url";

/*!
 * \brief Property identifiers for MooApp objects
 *
 * These identifiers are used for the properties of the MooApp class.
 */
enum
{
  PROP_0,            /*!< \brief Placeholder for the first property */
  PROP_RUN_INPUT,    /*!< \brief Whether to run input processing */
  PROP_USE_SESSION,  /*!< \brief Whether to use session management */
  PROP_DEFAULT_UI,   /*!< \brief Default UI XML data */
  PROP_INSTANCE_NAME /*!< \brief Name of the application instance */
};

/*!
 * \brief Signal identifiers for MooApp objects
 *
 * These identifiers are used for the signals emitted by the MooApp class.
 */
enum
{
  STARTED,      /*!< \brief Signal emitted when the application has started */
  QUIT,         /*!< \brief Signal emitted when the application is quitting */
  LOAD_SESSION, /*!< \brief Signal emitted when loading a session */
  SAVE_SESSION, /*!< \brief Signal emitted when saving a session */
  LAST_SIGNAL   /*!< \brief Placeholder for the last signal */
};

/*!
 * \brief Global application data structure
 *
 * This structure holds global data for the MooApp singleton instance.
 */
static struct
{
  MooApp *instance;          /*!< \brief Pointer to the singleton MooApp instance */
  gboolean atexit_installed; /*!< \brief Flag indicating if cleanup handler is installed */
} moo_app_data;

/*!
 * \brief Private data structure for MooApp
 *
 * This structure contains private data for the MooApp class.
 */
struct _MooAppPrivate
{
  MooEditor *editor;                  /*!< \brief Pointer to the editor instance */
  EggSMClient *sm_client;             /*!< \brief Session management client */
  MooMarkupDoc *session;              /*!< \brief Session document */
  MooUiXml *ui_xml;                   /*!< \brief UI XML data */
  const char *default_ui;             /*!< \brief Default UI XML data */
  char *rc_files[2];                  /*!< \brief Paths to configuration files */
  char *instance_name;                /*!< \brief Name of the application instance */
  char *session_file;                 /*!< \brief Path to session file */
  int exit_status;                    /*!< \brief Exit status code */
  int use_session;                    /*!< \brief Whether to use session management */
  gboolean run_input;                 /*!< \brief Whether to run input processing */
  gboolean running;                   /*!< \brief Whether the application is running */
  gboolean in_try_quit;               /*!< \brief Whether the application is trying to quit */
  gboolean saved_session_in_try_quit; /*!< \brief Whether session was saved during quit attempt */
  gboolean in_after_close_window;     /*!< \brief Whether processing after window close */
#if !GTK_CHECK_VERSION(3, 0, 0)
  guint quit_handler_id; /*!< \brief GTK2 quit handler ID */
#endif
};

/*!< \brief Parent class of MooApp */
static GObjectClass *moo_app_parent_class;

/*!< \brief Array of signal IDs for MooApp */
static guint signals[LAST_SIGNAL];

/*!< \brief Stores the most recently received signal */
static volatile int signal_received;

#if GTK_CHECK_VERSION(3, 0, 0)
/*!< \brief Global application pointer for GTK3 (replaces quit_handler_id) */
static MooApp *on_gtk_main_quit_app_arg;
#endif

/*!
 * \brief Gets the system name and version information
 * \return (transfer full): a newly allocated string containing system name, release, version and machine type
 * \return Example: "Linux 5.15.0 (#1 SMP), x86_64"
 */
static char *
get_system_name ()
{
  struct utsname name;

  if (uname (&name) != 0)
    {
      g_critical ("%s", g_strerror (errno));
      return g_strdup ("unknown");
    }

  return g_strdup_printf ("%s %s (%s), %s", name.sysname,
                          name.release, name.version, name.machine);
}

/*!
 * \brief Opens help for the focused widget or the window if no widget has focus.
 * \param window the parent window
 */
static void
moo_app_help (GtkWidget *window)
{
  GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (window));
  moo_help_open_any (focus ? focus : window);
}

/*!
 * \brief Opens a bug reporting URL with version and OS information. Asks for confirmation before opening the URL.
 * \param window the parent window
 */
static void
moo_app_report_bug (GtkWidget *window)
{
  char *message;
  const char *url = "https://github.com/abbat/medit/issues/new";
  const char *prefs_val;
  gboolean do_open = TRUE;

  moo_prefs_create_key (ASK_OPEN_BUG_URL_KEY, MOO_PREFS_STATE, G_TYPE_STRING, NULL);

  message = g_strdup_printf (_ ("The following URL will be opened:\n\n%s"), url);

  prefs_val = moo_prefs_get_string (ASK_OPEN_BUG_URL_KEY);
  if (!prefs_val || strcmp (prefs_val, url) != 0)
    {
      do_open = moo_question_dialog (_ ("Open URL?"), message, window, GTK_RESPONSE_OK);
      if (do_open)
        moo_prefs_set_string (ASK_OPEN_BUG_URL_KEY, url);
    }

  if (do_open)
    moo_open_url (url);

  g_free (message);
}

/*!
 * \brief Saves application preferences to user configuration files.
 * \param app the MooApp instance
 */
static void
moo_app_save_prefs (MooApp *app)
{
  GError *error = NULL;

  if (!moo_prefs_save (app->priv->rc_files[MOO_PREFS_RC],
                       app->priv->rc_files[MOO_PREFS_STATE],
                       &error))
    {
      g_warning ("could not save config files: %s", moo_error_message (error));
      g_error_free (error);
    }
}

/*!
 * \brief Callback function called when the preferences dialog is applied. Saves the application preferences.
 */
static void
prefs_dialog_apply (void)
{
  moo_app_save_prefs (moo_app_instance ());
}

/*!
 * \brief Creates the preferences dialog with all preference pages.
 * \param app the MooApp instance
 * \return the created preferences dialog widget
 */
static GtkWidget *
moo_app_create_prefs_dialog (MooApp *app)
{
  MooPrefsDialog *dialog;

  /* Prefs dialog title */
  dialog = MOO_PREFS_DIALOG (moo_prefs_dialog_new (_ ("Preferences")));

  moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_1 (moo_app_get_editor (app)));
  moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_2 (moo_app_get_editor (app)));
  moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_3 (moo_app_get_editor (app)));
  moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_4 (moo_app_get_editor (app)));
  moo_prefs_dialog_append_page (dialog, moo_edit_prefs_page_new_5 (moo_app_get_editor (app)));
  moo_plugin_attach_prefs (GTK_WIDGET (dialog));

  g_signal_connect_after (dialog, "apply",
                          G_CALLBACK (prefs_dialog_apply),
                          NULL);

  return GTK_WIDGET (dialog);
}

/*!
 * \brief Shows the preferences dialog for the application.
 * \param parent the parent window
 */
static void
moo_app_prefs_dialog (GtkWidget *parent)
{
  GtkWidget *dialog = moo_app_create_prefs_dialog (moo_app_instance ());
  g_return_if_fail (MOO_IS_PREFS_DIALOG (dialog));
  moo_prefs_dialog_run (MOO_PREFS_DIALOG (dialog), parent);
}

/*!
 * \brief Installs common actions for all windows in the application, including Preferences, About, Help, Report Bug, and Quit.
 */
static void
install_common_actions (void)
{
  MooWindowClass *klass = (MooWindowClass *) g_type_class_ref (MOO_TYPE_WINDOW);

  g_return_if_fail (klass != NULL);

  moo_window_class_new_action (klass, "Preferences", NULL,
                               "display-name", GTK_STOCK_PREFERENCES,
                               "label", GTK_STOCK_PREFERENCES,
                               "tooltip", GTK_STOCK_PREFERENCES,
                               "stock-id", GTK_STOCK_PREFERENCES,
                               "closure-callback", moo_app_prefs_dialog,
                               NULL);

  moo_window_class_new_action (klass, "About", NULL,
                               "label", GTK_STOCK_ABOUT,
                               "no-accel", TRUE,
                               "stock-id", GTK_STOCK_ABOUT,
                               "closure-callback", show_about /* was moo_app_about_dialog from mooappabout.h */,
                               NULL);

  moo_window_class_new_action (klass, "Help", NULL,
                               "label", GTK_STOCK_HELP,
                               "default-accel", MOO_APP_ACCEL_HELP,
                               "stock-id", GTK_STOCK_HELP,
                               "closure-callback", moo_app_help,
                               NULL);

  moo_window_class_new_action (klass, "ReportBug", NULL,
                               "label", _ ("Report a Bug..."),
                               "closure-callback", moo_app_report_bug,
                               NULL);

  moo_window_class_new_action (klass, "Quit", NULL,
                               "display-name", GTK_STOCK_QUIT,
                               "label", GTK_STOCK_QUIT,
                               "tooltip", GTK_STOCK_QUIT,
                               "stock-id", GTK_STOCK_QUIT,
                               "default-accel", MOO_APP_ACCEL_QUIT,
                               "closure-callback", moo_app_quit,
                               "closure-proxy-func", moo_app_instance,
                               NULL);

  g_type_class_unref (klass);
}

/*!
 * \brief Installs editor-specific actions. Currently a placeholder function.
 */
static void
install_editor_actions (void)
{
  MooWindowClass *klass = (MooWindowClass *) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
  g_return_if_fail (klass != NULL);
  g_type_class_unref (klass);
}

/*!
 * \brief Cleanup function called at exit to shut down various subsystems.
 */
static void
moo_app_cleanup (void)
{
  _moo_app_input_shutdown ();
  moo_mime_shutdown ();
  moo_cleanup ();
}

/*!
 * \brief Installs the cleanup function to be called at exit if not already installed.
 */
static void
moo_app_install_cleanup (void)
{
  if (!moo_app_data.atexit_installed)
    {
      moo_app_data.atexit_installed = TRUE;
      atexit (moo_app_cleanup);
    }
}

/*!
 * \brief Sets up signal handlers for SIGINT and SIGHUP.
 * \param handler signal handler function
 */
static void
setup_signals (void (*handler) (int))
{
  signal (SIGINT, handler);
  /* TODO: maybe detach from terminal in this case? */
  signal (SIGHUP, handler);
}

/*!
 * \brief Handles SIGINT and SIGHUP signals by storing the signal and resetting signal handlers to default.
 * \param sig the signal number received
 */
static void
sigint_handler (int sig)
{
  signal_received = sig;
  setup_signals (SIG_DFL);
}

/*!
 * \brief Constructor for MooApp objects. Ensures only one instance exists, sets up signal handlers, cleanup functions, and installs actions.
 * \param type the GType of the object to construct
 * \param n_params number of construction parameters
 * \param params construction parameters
 * \return the newly constructed GObject or NULL on error
 */
static GObject *
moo_app_constructor (GType type, guint n_params, GObjectConstructParam *params)
{
  GObject *object;

  if (moo_app_data.instance != NULL)
    {
      g_critical ("attempt to create second instance of application class");
      g_critical ("going to crash now");
      return NULL;
    }

  object = moo_app_parent_class->constructor (type, n_params, params);

  setup_signals (sigint_handler);

  moo_app_install_cleanup ();

  install_common_actions ();
  install_editor_actions ();

  return object;
}

/*!
 * \brief Writes the session data to disk. If no session data exists, removes the session file.
 * \param app the MooApp instance
 */
static void
moo_app_write_session (MooApp *app)
{
  char *filename;
  GError *error = NULL;
  MooFileWriter *writer;

  if (!app->priv->session_file)
    return;

  filename = moo_get_user_cache_file (app->priv->session_file);

  if (!app->priv->session)
    {
      mgw_errno_t err;
      mgw_unlink (filename, &err);
      g_free (filename);
      return;
    }

  if ((writer = moo_config_writer_new (filename, FALSE, &error)))
    {
      moo_markup_write_pretty (app->priv->session, writer, 1);
      moo_file_writer_close (writer, &error);
    }

  if (error)
    {
      g_critical ("could not save session file %s: %s", filename, error->message);
      g_error_free (error);
    }

  g_free (filename);
}

/*!
 * \brief Performs the actual quit operation, cleaning up resources and exiting the main loop.
 * \param app the MooApp instance
 */
static void
moo_app_do_quit (MooApp *app)
{
  guint i;

  if (!app->priv->running)
    return;
  else
    app->priv->running = FALSE;

  g_signal_emit (app, signals[QUIT], 0);

  g_object_unref (app->priv->sm_client);
  app->priv->sm_client = NULL;

  _moo_editor_close_all (app->priv->editor);

  moo_plugin_shutdown ();

  g_object_unref (app->priv->editor);
  app->priv->editor = NULL;

  moo_app_write_session (app);
  moo_app_save_prefs (app);

#if !GTK_CHECK_VERSION(3, 0, 0)
  if (app->priv->quit_handler_id)
    gtk_quit_remove (app->priv->quit_handler_id);
#endif

  i = 0;
  while (gtk_main_level () && i < 1000)
    {
      gtk_main_quit ();
      i++;
    }

  moo_app_cleanup ();
}

/*!
 * \brief Finalizes a MooApp object, cleaning up all allocated resources
 * and calling the parent class finalize method.
 * \param object the GObject to finalize
 */
static void
moo_app_finalize (GObject *object)
{
  MooApp *app = MOO_APP (object);

  moo_app_do_quit (app);

  moo_app_data.instance = NULL;

#if GTK_CHECK_VERSION(3, 0, 0)
  on_gtk_main_quit_app_arg = NULL;
#endif

  g_free (app->priv->rc_files[0]);
  g_free (app->priv->rc_files[1]);

  g_free (app->priv->session_file);
  if (app->priv->session)
    moo_markup_doc_unref (app->priv->session);

  if (app->priv->editor)
    g_object_unref (app->priv->editor);
  if (app->priv->ui_xml)
    g_object_unref (app->priv->ui_xml);

  g_free (app->priv->instance_name);
  g_free (app->priv);

  G_OBJECT_CLASS (moo_app_parent_class)->finalize (object);
}

/*!
 * \brief Gets a property from a MooApp object.
 * \param object the GObject to get a property from
 * \param prop_id the property ID
 * \param value the GValue to store the property value in
 * \param pspec the GParamSpec for the property
 */
static void
moo_app_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  MooApp *app = MOO_APP (object);

  switch (prop_id)
    {
    case PROP_RUN_INPUT:
      g_value_set_boolean (value, app->priv->run_input);
      break;
    case PROP_USE_SESSION:
      g_value_set_int (value, app->priv->use_session);
      break;
    case PROP_INSTANCE_NAME:
      g_value_set_string (value, app->priv->instance_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*!
 * \brief Sets a property on a MooApp object.
 * \param object the GObject to set a property on
 * \param prop_id the property ID
 * \param value the GValue containing the property value
 * \param pspec the GParamSpec for the property
 */
static void
moo_app_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  MooApp *app = MOO_APP (object);

  switch (prop_id)
    {
    case PROP_RUN_INPUT:
      app->priv->run_input = g_value_get_boolean (value);
      break;

    case PROP_USE_SESSION:
      app->priv->use_session = g_value_get_int (value);
      break;

    case PROP_INSTANCE_NAME:
      g_free (app->priv->instance_name);
      app->priv->instance_name = g_value_dup_string (value);
      break;

    case PROP_DEFAULT_UI:
      app->priv->default_ui = (const char *) g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

/*!
 * \brief Initializes the MooAppClass, setting up virtual methods, properties, and signals.
 * \param klass the MooAppClass to initialize
 * \param data unused data pointer
 */
static void
moo_app_class_init (MooAppClass *klass, G_GNUC_UNUSED gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  moo_app_parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  gobject_class->constructor = moo_app_constructor;
  gobject_class->finalize = moo_app_finalize;
  gobject_class->set_property = moo_app_set_property;
  gobject_class->get_property = moo_app_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_RUN_INPUT,
                                   g_param_spec_boolean ("run-input",
                                                         "run-input",
                                                         "run-input",
                                                         TRUE,
                                                         (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

  g_object_class_install_property (gobject_class,
                                   PROP_USE_SESSION,
                                   g_param_spec_int ("use-session",
                                                     "use-session",
                                                     "use-session",
                                                     -1, 1, -1,
                                                     (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

  g_object_class_install_property (gobject_class,
                                   PROP_INSTANCE_NAME,
                                   g_param_spec_string ("instance-name",
                                                        "instance-name",
                                                        "instance-name",
                                                        NULL,
                                                        (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

  g_object_class_install_property (gobject_class, PROP_DEFAULT_UI,
                                   g_param_spec_pointer ("default-ui", "default-ui", "default-ui",
                                                         (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));

  signals[STARTED] = g_signal_new ("started",
                                   G_OBJECT_CLASS_TYPE (klass),
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET (MooAppClass, started),
                                   NULL, NULL,
                                   _moo_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);

  signals[QUIT] = g_signal_new ("quit",
                                G_OBJECT_CLASS_TYPE (klass),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (MooAppClass, quit),
                                NULL, NULL,
                                _moo_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  signals[LOAD_SESSION] = g_signal_new ("load-session",
                                        G_OBJECT_CLASS_TYPE (klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (MooAppClass, load_session),
                                        NULL, NULL,
                                        _moo_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);

  signals[SAVE_SESSION] = g_signal_new ("save-session",
                                        G_OBJECT_CLASS_TYPE (klass),
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (MooAppClass, save_session),
                                        NULL, NULL,
                                        _moo_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);
}

/*!
 * \brief Saves the current session to memory. Creates a new session document and emits the save-session signal before saving the editor session.
 * \param app the MooApp instance
 */
static void
moo_app_save_session (MooApp *app)
{
  MooMarkupNode *root;

  if (!app->priv->session_file)
    return;

  if (app->priv->session)
    moo_markup_doc_unref (app->priv->session);

  app->priv->session = moo_markup_doc_new ("session");
  root = moo_markup_create_root_element (app->priv->session, "session");
  moo_markup_set_prop (root, "version", SESSION_VERSION);

  g_signal_emit (app, signals[SAVE_SESSION], 0);
  _moo_editor_save_session (moo_app_get_editor (app), root);
}

/*!
 * \brief Callback function called before an editor window is closed. Saves the session if this is the last window.
 * \param app the MooApp instance
 */
static void
editor_will_close_window (MooApp *app)
{
  MooEditWindowArray *windows;

  if (!app->priv->running || app->priv->saved_session_in_try_quit)
    return;

  windows = moo_editor_get_windows (app->priv->editor);

  if (moo_edit_window_array_get_size (windows) == 1)
    moo_app_save_session (app);

  moo_edit_window_array_free (windows);
}

/*!
 * \brief Callback function called after an editor window is closed.
 * Quits the application if there are no more windows.
 * \param app the MooApp instance
 */
static void
editor_after_close_window (MooApp *app)
{
  MooEditWindowArray *windows;

  if (!app->priv->running || app->priv->in_try_quit)
    return;

  windows = moo_editor_get_windows (app->priv->editor);

  if (moo_edit_window_array_get_size (windows) == 0)
    {
      app->priv->in_after_close_window = TRUE;
      moo_app_quit (app);
      app->priv->in_after_close_window = FALSE;
    }

  moo_edit_window_array_free (windows);
}

/*!
 * \brief Initializes plugins for the application if the init_plugins method is implemented in the class.
 * \param app the MooApp instance
 */
static void
init_plugins (MooApp *app)
{
  if (MOO_APP_GET_CLASS (app)->init_plugins)
    MOO_APP_GET_CLASS (app)->init_plugins (app);
}

/*!
 * \brief Gets the UI XML for the application.
 * If not already created, tries to get it from the editor or creates a new one.
 * \param app the MooApp instance
 * \return the UI XML object
 */
MooUiXml *
moo_app_get_ui_xml (MooApp *app)
{
  g_return_val_if_fail (MOO_IS_APP (app), NULL);

  if (!app->priv->ui_xml)
    {
      if (app->priv->editor)
        {
          app->priv->ui_xml = moo_editor_get_ui_xml (app->priv->editor);
          g_object_ref (app->priv->ui_xml);
        }

      if (!app->priv->ui_xml)
        app->priv->ui_xml = moo_ui_xml_new ();
    }

  return app->priv->ui_xml;
}

/*!
 * \brief Initializes the editor component of the application, setting up signal handlers and UI XML.
 * \param app the MooApp instance
 */
static void
moo_app_init_editor (MooApp *app)
{
  app->priv->editor = moo_editor_create_instance (FALSE);

  g_signal_connect_swapped (app->priv->editor, "will-close-window",
                            G_CALLBACK (editor_will_close_window), app);
  g_signal_connect_swapped (app->priv->editor, "after-close-window",
                            G_CALLBACK (editor_after_close_window), app);

  /* if ui_xml wasn't set yet, then moo_app_get_ui_xml()
     will get editor's xml */
  moo_editor_set_ui_xml (app->priv->editor,
                         moo_app_get_ui_xml (app));

  init_plugins (app);
}

/*!
 * \brief Initializes the UI XML for the application, loading from files or using the default UI if no files are found.
 * \param app the MooApp instance
 */
static void
moo_app_init_ui (MooApp *app)
{
  MooUiXml *xml = NULL;
  char **files, **p;

  // FIXME: file ui.xml not exists, seems useless code
  files = moo_get_data_files (MOO_UI_XML_FILE);

  for (p = files; p && *p; ++p)
    {
      GError *error = NULL;
      GMappedFile *file;

      file = g_mapped_file_new (*p, FALSE, &error);

      if (file)
        {
          xml = moo_ui_xml_new ();
          moo_ui_xml_add_ui_from_string (xml,
                                         g_mapped_file_get_contents (file),
                                         g_mapped_file_get_length (file));
          g_mapped_file_unref (file);
          break;
        }

      if (!(error && error->domain == G_FILE_ERROR && error->code == G_FILE_ERROR_NOENT))
        g_warning ("could not open file '%s': %s", *p, moo_error_message (error));

      g_error_free (error);
    }

  if (!xml && app->priv->default_ui)
    {
      xml = moo_ui_xml_new ();
      moo_ui_xml_add_ui_from_string (xml, app->priv->default_ui, -1);
    }

  if (xml)
    {
      if (app->priv->ui_xml)
        g_object_unref (app->priv->ui_xml);
      app->priv->ui_xml = xml;
    }

  g_strfreev (files);
}

/*!
 * \brief Parses XML data containing file information and creates an array of MooOpenInfo objects.
 * \param data the XML data to parse
 * \param stamp location to store the timestamp
 * \return the array of file information or NULL on error
 */
static MooOpenInfoArray *
moo_app_parse_files (const char *data, guint32 *stamp)
{
  MooMarkupDoc *xml;
  MooMarkupNode *root;
  MooMarkupNode *node;
  const char *version;
  MooOpenInfoArray *files;

  *stamp = 0;

  xml = moo_markup_parse_memory (data, -1, NULL);
  g_return_val_if_fail (xml != NULL, FALSE);

  if (!(root = moo_markup_get_root_element (xml, "moo-app-open-files")) ||
      !(version = moo_markup_get_prop (root, "version")) ||
      strcmp (version, MOO_APP_CMD_VERSION) != 0)
    {
      g_warning ("%s: invalid markup", G_STRFUNC);
      moo_markup_doc_unref (xml);
      return NULL;
    }

  *stamp = moo_markup_uint_prop (root, "stamp", 0);
  files = moo_open_info_array_new ();

  for (node = root->children; node != NULL; node = node->next)
    {
      const char *uri;
      const char *encoding;
      MooOpenInfo *info;
      int line;

      if (!MOO_MARKUP_IS_ELEMENT (node))
        continue;

      if (strcmp (node->name, "file") != 0 ||
          !(uri = moo_markup_get_content (node)) ||
          !uri[0])
        {
          g_critical ("%s: oops", G_STRFUNC);
          continue;
        }

      encoding = moo_markup_get_prop (node, "encoding");
      if (!encoding || !encoding[0])
        encoding = NULL;

      info = moo_open_info_new_uri (uri, encoding, -1, MOO_OPEN_FLAG_CREATE_NEW);

      line = moo_markup_int_prop (node, "line", 0);
      if (line > 0)
        moo_open_info_set_line (info, line - 1);

      if (moo_markup_bool_prop (node, "new-window", FALSE))
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_WINDOW);
      if (moo_markup_bool_prop (node, "new-tab", FALSE))
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_NEW_TAB);
      if (moo_markup_bool_prop (node, "reload", FALSE))
        moo_open_info_add_flags (info, MOO_OPEN_FLAG_RELOAD);

      moo_open_info_array_take (files, info);
    }

  moo_markup_doc_unref (xml);
  return files;
}

/*!
 * \brief Command handler for opening files. Parses the XML data and opens the files.
 * \param app the MooApp instance
 * \param data the XML data containing file information
 */
static void
moo_app_cmd_open_files (MooApp *app, const char *data)
{
  MooOpenInfoArray *files;
  guint32 stamp;
  files = moo_app_parse_files (data, &stamp);
  moo_app_open_files (app, files, stamp);
  moo_open_info_array_free (files);
}

/*!
 * \brief Converts a command character to a command code.
 * \param cmd the command character
 * \return the corresponding MooAppCmdCode
 */
static MooAppCmdCode
get_cmd_code (char cmd)
{
  guint i;

  for (i = 1; i < CMD_LAST; ++i)
    if (cmd == moo_app_cmd_chars[i])
      return (MooAppCmdCode) i;

  g_return_val_if_reached ((MooAppCmdCode) 0);
}

/*!
 * \brief Executes a command based on the command character.
 * \param app the MooApp instance
 * \param cmd the command character
 * \param data the command data
 * \param len the length of the data (unused)
 */
static void
moo_app_exec_cmd (MooApp *app, char cmd, const char *data, G_GNUC_UNUSED guint len)
{
  MooAppCmdCode code;

  g_return_if_fail (MOO_IS_APP (app));

  code = get_cmd_code (cmd);

  switch (code)
    {
    case CMD_OPEN_FILES:
      moo_app_cmd_open_files (app, data);
      break;

    default:
      g_warning ("got unknown command %c %d", cmd, code);
    }
}

/*!
 * \brief Callback function for handling input commands.
 * \param cmd the command character
 * \param data the command data
 * \param len the length of the data
 * \param cb_data user data (MooApp instance)
 */
static void
input_callback (char cmd, const char *data, gsize len, gpointer cb_data)
{
  MooApp *app = (MooApp *) cb_data;

  g_return_if_fail (MOO_IS_APP (app));
  g_return_if_fail (data != NULL);

  moo_app_exec_cmd (app, cmd, data, len);
}

/*!
 * \brief Starts the input system for the application if run_input is enabled.
 * \param app the MooApp instance
 */
static void
start_input (MooApp *app)
{
  if (app->priv->run_input)
    _moo_app_input_start (app->priv->instance_name,
                          TRUE, input_callback, app);
}

/*!
 * \brief Callback function called when GTK main loop is about to quit. Attempts to quit the application gracefully.
 * \param app the MooApp instance (GTK2 only)
 * \return FALSE (GTK2 only)
 */
static
#if GTK_CHECK_VERSION(3, 0, 0)
    void
    on_gtk_main_quit ()
#else
    gboolean
    on_gtk_main_quit (MooApp *app)
#endif
{
#if GTK_CHECK_VERSION(3, 0, 0)
  MooApp *app = on_gtk_main_quit_app_arg;
  if (app == NULL)
    return;
#else
  app->priv->quit_handler_id = 0;
#endif

  if (!moo_app_quit (app))
    moo_app_do_quit (app);

#if !GTK_CHECK_VERSION(3, 0, 0)
  return FALSE;
#endif
}

/*!
 * \brief Timeout function to check for received signals and handle them.
 * \param data unused data pointer
 * \return TRUE to continue the timeout
 */
static gboolean
check_signal (G_GNUC_UNUSED gpointer data)
{
  if (signal_received)
    {
      g_print ("%s\n", g_strsignal (signal_received));
      if (moo_app_data.instance)
        moo_app_do_quit (moo_app_data.instance);

      exit (EXIT_FAILURE);
    }

  return TRUE;
}

/*!
 * \brief Emits the "started" signal for the application.
 * \param app the MooApp instance
 * \return FALSE to remove the idle source
 */
static gboolean
emit_started (MooApp *app)
{
  g_signal_emit_by_name (app, "started");

  return FALSE;
}

/*!
 * \brief Callback for session manager quit request.
 * \param app the MooApp instance
 */
static void
sm_quit_requested (MooApp *app)
{
  EggSMClient *sm_client;

  sm_client = app->priv->sm_client;
  g_return_if_fail (sm_client != NULL);

  g_object_ref (sm_client);
  egg_sm_client_will_quit (sm_client, moo_app_quit (app));
  g_object_unref (sm_client);
}

/*!
 * \brief Callback for session manager quit command.
 * \param app the MooApp instance
 */
static void
sm_quit (MooApp *app)
{
  if (!moo_app_quit (app))
    moo_app_do_quit (app);
}

/*!
 * \brief Loads application preferences from system and user configuration files.
 * \param app the MooApp instance
 */
static void
moo_app_load_prefs (MooApp *app)
{
  GError *error = NULL;
  char **sys_files;

  app->priv->rc_files[MOO_PREFS_RC] = moo_get_user_data_file (MOO_PREFS_XML_FILE_NAME);
  app->priv->rc_files[MOO_PREFS_STATE] = moo_get_user_cache_file (MOO_STATE_XML_FILE_NAME);

  sys_files = moo_get_sys_data_files (MOO_PREFS_XML_FILE_NAME);

  if (!moo_prefs_load (sys_files,
                       app->priv->rc_files[MOO_PREFS_RC],
                       app->priv->rc_files[MOO_PREFS_STATE],
                       &error))
    {
      g_warning ("could not read config files: %s", moo_error_message (error));
      g_error_free (error);
    }

  g_strfreev (sys_files);
}

/*!
 * \brief Attempts to quit the application by closing all editor windows.
 * Saves the session if not already in the process of closing a window.
 * \param app the MooApp instance
 * \return TRUE if all windows were closed, FALSE otherwise
 */
static gboolean
moo_app_try_quit (MooApp *app)
{
  gboolean closed;

  g_return_val_if_fail (MOO_IS_APP (app), FALSE);

  if (!app->priv->running)
    return TRUE;

  app->priv->in_try_quit = TRUE;

  if (!app->priv->in_after_close_window)
    {
      app->priv->saved_session_in_try_quit = TRUE;
      moo_app_save_session (app);
    }

  closed = _moo_editor_close_all (app->priv->editor);

  app->priv->saved_session_in_try_quit = FALSE;
  app->priv->in_try_quit = FALSE;

  return closed;
}

/*!
 * \brief Loads the session from the provided XML node and emits the load-session signal.
 * \param app the MooApp instance
 * \param xml the XML node containing session data
 */
static void
moo_app_do_load_session (MooApp *app, MooMarkupNode *xml)
{
  MooEditor *editor;
  editor = moo_app_get_editor (app);
  g_return_if_fail (editor != NULL);
  _moo_editor_load_session (editor, xml);
  g_signal_emit (app, signals[LOAD_SESSION], 0);
}

/*!
 * \brief Formats a string with proper XML escaping and appends it to a GString.
 * \param str the GString to append to
 * \param format the format string
 * \param ... the arguments for the format string
 */
G_GNUC_PRINTF (2, 3)
static void
append_escaped (GString *str, const char *format, ...)
{
  va_list args;
  char *escaped;

  va_start (args, format);

  escaped = g_markup_vprintf_escaped (format, args);
  g_string_append (str, escaped);
  g_free (escaped);

  va_end (args);
}

/*!
 * \brief Initializes a new MooApp instance, setting up private data and initializing stock icons.
 * \param app the MooApp instance to initialize
 * \param data unused data pointer
 */
static void
moo_app_instance_init (MooApp *app, G_GNUC_UNUSED gpointer data)
{
  g_return_if_fail (moo_app_data.instance == NULL);

  _moo_stock_init ();

  moo_app_data.instance = app;

  app->priv = g_new0 (MooAppPrivate, 1);
  app->priv->use_session = -1;
}

/*!
 * \brief Gets the GType for the MooApp class.
 * \return the GType for the MooApp class
 */
GType
moo_app_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (!type))
    {
      static const GTypeInfo type_info = {
        sizeof (MooAppClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) moo_app_class_init,
        (GClassFinalizeFunc) NULL,
        NULL, /* class_data */
        sizeof (MooApp),
        0, /* n_preallocs */
        (GInstanceInitFunc) moo_app_instance_init,
        NULL /* value_table */
      };

      type = g_type_register_static (G_TYPE_OBJECT, "MooApp", &type_info, (GTypeFlags) 0);
    }

  return type;
}

/*!
 * \brief Gets the singleton instance of the MooApp application.
 * \return the MooApp instance or NULL if not created yet
 */
MooApp *
moo_app_instance (void)
{
  return moo_app_data.instance;
}

/*!
 * \brief Initializes the application, setting up program class, icon name, loading preferences,
 * initializing UI and editor, and starting input.
 * \param app the MooApp instance
 * \return TRUE on success, FALSE on failure
 */
gboolean
moo_app_init (MooApp *app)
{
  g_return_val_if_fail (MOO_IS_APP (app), FALSE);

  gdk_set_program_class (MOO_APP_FULL_NAME);
  gtk_window_set_default_icon_name (MOO_APP_SHORT_NAME);

  moo_set_display_app_name (MOO_APP_SHORT_NAME);
  _moo_set_app_instance_name (app->priv->instance_name);

  moo_app_load_prefs (app);
  moo_app_init_ui (app);

  moo_app_init_editor (app);

  if (app->priv->use_session == -1)
    app->priv->use_session = moo_prefs_get_bool (moo_edit_setting (MOO_EDIT_PREFS_SAVE_SESSION));

  if (app->priv->use_session)
    app->priv->run_input = TRUE;

  start_input (app);

  return TRUE;
}

/*!
 * \brief Runs the main application loop, setting up signal handlers, session management, and starting the GTK main loop.
 * \param app the MooApp instance
 * \return the exit status of the application
 */
int
moo_app_run (MooApp *app)
{
  g_return_val_if_fail (MOO_IS_APP (app), -1);
  g_return_val_if_fail (!app->priv->running, 0);

  app->priv->running = TRUE;

#if GTK_CHECK_VERSION(3, 0, 0)
  on_gtk_main_quit_app_arg = app;
  // FIXME: deprecated
  g_atexit (on_gtk_main_quit);
#else
  app->priv->quit_handler_id = gtk_quit_add (1, (GtkFunction) on_gtk_main_quit, app);
#endif

  g_timeout_add (100, (GSourceFunc) check_signal, NULL);

  app->priv->sm_client = egg_sm_client_get ();
  /* make it install log handler */
  g_option_group_free (egg_sm_client_get_option_group ());
  g_signal_connect_swapped (app->priv->sm_client, "quit-requested",
                            G_CALLBACK (sm_quit_requested), app);
  g_signal_connect_swapped (app->priv->sm_client, "quit",
                            G_CALLBACK (sm_quit), app);

  if (EGG_SM_CLIENT_GET_CLASS (app->priv->sm_client)->startup)
    EGG_SM_CLIENT_GET_CLASS (app->priv->sm_client)->startup (app->priv->sm_client, NULL);

  g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 1, (GSourceFunc) emit_started, app, NULL);

  gtk_main ();

  return app->priv->exit_status;
}

/*!
 * \brief Attempts to quit the application.
 * If already in the process of quitting or not running, returns TRUE.
 * Otherwise tries to close all windows and performs the actual quit if successful.
 * \param app the MooApp instance
 * \return TRUE if the application will quit, FALSE if the quit was cancelled
 */
gboolean
moo_app_quit (MooApp *app)
{
  g_return_val_if_fail (MOO_IS_APP (app), FALSE);

  if (app->priv->in_try_quit || !app->priv->running)
    return TRUE;

  if (moo_app_try_quit (app))
    {
      moo_app_do_quit (app);
      return TRUE;
    }

  return FALSE;
}

/*!
 * \brief Loads the session from disk if session management is enabled.
 * Validates the session file format and version before loading.
 * \param app the MooApp instance
 */
void
moo_app_load_session (MooApp *app)
{
  MooMarkupDoc *doc;
  MooMarkupNode *root;
  GError *error = NULL;
  const char *version;
  char *session_file;

  g_return_if_fail (MOO_IS_APP (app));

  if (!app->priv->use_session)
    return;

  if (!app->priv->session_file)
    {
      if (app->priv->instance_name)
        app->priv->session_file = g_strdup_printf (MOO_NAMED_SESSION_XML_FILE_NAME,
                                                   app->priv->instance_name);
      else
        app->priv->session_file = g_strdup (MOO_SESSION_XML_FILE_NAME);
    }

  session_file = moo_get_user_cache_file (app->priv->session_file);

  if (!g_file_test (session_file, G_FILE_TEST_EXISTS) ||
      !(doc = moo_markup_parse_file (session_file, &error)))
    {
      if (error)
        {
          g_warning ("could not open session file %s: %s",
                     session_file, error->message);
          g_error_free (error);
        }

      g_free (session_file);
      return;
    }

  if (!(root = moo_markup_get_root_element (doc, "session")) ||
      !(version = moo_markup_get_prop (root, "version")))
    g_warning ("malformed session file %s, ignoring", session_file);
  else if (strcmp (version, SESSION_VERSION) != 0)
    g_warning ("invalid session file version %s in %s, ignoring",
               version, session_file);
  else
    {
      app->priv->session = doc;
      moo_app_do_load_session (app, root);
      app->priv->session = NULL;
    }

  moo_markup_doc_unref (doc);
  g_free (session_file);
}

/*!
 * \brief Gets the MooEditor instance associated with the application.
 * \param app the MooApp instance
 * \return the MooEditor instance or NULL if not initialized
 */
MooEditor *
moo_app_get_editor (MooApp *app)
{
  g_return_val_if_fail (MOO_IS_APP (app), NULL);
  return app->priv->editor;
}

/*!
 * \brief Sends a message to another process.
 * \param pid the process ID to send the message to
 * \param data the message data
 * \param len the length of the data
 * \return TRUE on success, FALSE on failure
 */
static gboolean
moo_app_send_msg (const char *pid, const char *data, gssize len)
{
  g_return_val_if_fail (data != NULL, FALSE);
  return _moo_app_input_send_msg (pid, data, len);
}

/*!
 * \brief Sends file information to another process as an XML message.
 * \param files array of files to send
 * \param stamp timestamp for the operation
 * \param pid the process ID to send the files to
 * \return TRUE on success, FALSE on failure
 */
gboolean
moo_app_send_files (MooOpenInfoArray *files, guint32 stamp, const char *pid)
{
  gboolean result;
  GString *msg;
  int i, c;

  msg = g_string_new (NULL);
  g_string_append_printf (msg, "%s<moo-app-open-files version=\"%s\" stamp=\"%u\">",
                          CMD_OPEN_FILES_S, MOO_APP_CMD_VERSION, stamp);

  for (i = 0, c = moo_open_info_array_get_size (files); i < c; ++i)
    {
      MooOpenInfo *info = files->elms[i];
      const char *encoding = moo_open_info_get_encoding (info);
      int line = moo_open_info_get_line (info);
      MooOpenFlags flags = moo_open_info_get_flags (info);
      char *uri;

      g_string_append (msg, "<file");

      if (encoding)
        g_string_append_printf (msg, " encoding=\"%s\"", encoding);
      if (line >= 0)
        g_string_append_printf (msg, " line=\"%u\"", (guint) line + 1);
      if (flags & MOO_OPEN_FLAG_NEW_WINDOW)
        g_string_append_printf (msg, " new-window=\"true\"");
      if (flags & MOO_OPEN_FLAG_NEW_TAB)
        g_string_append_printf (msg, " new-tab=\"true\"");
      if (flags & MOO_OPEN_FLAG_RELOAD)
        g_string_append_printf (msg, " reload=\"true\"");

      uri = moo_open_info_get_uri (info);
      append_escaped (msg, ">%s</file>", uri);
      g_free (uri);
    }

  g_string_append (msg, "</moo-app-open-files>");

  result = moo_app_send_msg (pid, msg->str, msg->len);

  g_string_free (msg, TRUE);
  return result;
}

/*!
 * \brief Opens the specified files in the editor and presents the editor window.
 * \param app the MooApp instance
 * \param files array of files to open
 * \param stamp timestamp for the operation
 */
void
moo_app_open_files (MooApp *app, MooOpenInfoArray *files, guint32 stamp)
{
  g_return_if_fail (MOO_IS_APP (app));

  if (!moo_open_info_array_is_empty (files))
    {
      guint i;
      MooOpenInfoArray *tmp = moo_open_info_array_copy (files);
      for (i = 0; i < tmp->n_elms; ++i)
        moo_open_info_add_flags (tmp->elms[i], MOO_OPEN_FLAG_CREATE_NEW);
      moo_editor_open_files (app->priv->editor, tmp, NULL, NULL);
      moo_open_info_array_free (tmp);
    }

  moo_editor_present (app->priv->editor, stamp);
}

/*!
 * \brief Gets system information including application version, OS details, and library versions
 * \param app a MooApp
 * \return (transfer full): a newly allocated string containing system information
 */
char *
moo_app_get_system_info (MooApp *app)
{
  char **p;
  char **dirs;
  char *string;
  GString *text;

  g_return_val_if_fail (MOO_IS_APP (app), NULL);

  text = g_string_new (NULL);

  g_string_append_printf (text, "%s-%s\n", MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);

  string = get_system_name ();
  g_string_append_printf (text, "OS: %s\n", string);
  g_free (string);

  g_string_append_printf (text, "GTK version: %u.%u.%u\n",
                          gtk_major_version,
                          gtk_minor_version,
                          gtk_micro_version);
  g_string_append_printf (text, "Built with GTK %d.%d.%d\n",
                          GTK_MAJOR_VERSION,
                          GTK_MINOR_VERSION,
                          GTK_MICRO_VERSION);

  g_string_append_printf (text, "libxml2: %s\n", LIBXML_DOTTED_VERSION);

  g_string_append (text, "Data dirs: ");
  dirs = moo_get_data_dirs ();
  for (p = dirs; p && *p; ++p)
    g_string_append_printf (text, "%s'%s'", p == dirs ? "" : ", ", *p);
  g_string_append (text, "\n");
  g_strfreev (dirs);

  g_string_append (text, "Lib dirs: ");
  dirs = moo_get_lib_dirs ();
  for (p = dirs; p && *p; ++p)
    g_string_append_printf (text, "%s'%s'", p == dirs ? "" : ", ", *p);
  g_string_append (text, "\n");
  g_strfreev (dirs);

#ifdef MOO_BROKEN_GTK_THEME
  g_string_append (text, "Broken gtk theme: yes\n");
#endif

  return g_string_free (text, FALSE);
}
