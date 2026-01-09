/*
 *   mooapp/mooapp.h
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

#pragma once
#ifndef _mooapp_mooapp_h_
#define _mooapp_mooapp_h_

#include <mooedit/mooeditor.h>

G_BEGIN_DECLS

/*!< \brief Returns the GType for the MooApp object */
#define MOO_TYPE_APP (moo_app_get_type ())

/*!< \brief Casts a GObject to MooApp */
#define MOO_APP(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), MOO_TYPE_APP, MooApp))

/*!< \brief Casts a GObjectClass to MooAppClass */
#define MOO_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOO_TYPE_APP, MooAppClass))

/*!< \brief Checks if a GObject is an instance of MooApp or a subclass */
#define MOO_IS_APP(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), MOO_TYPE_APP))

/*!< \brief Checks if a GObjectClass is a MooAppClass or a subclass */
#define MOO_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOO_TYPE_APP))

/*!< \brief Returns the class structure for a MooApp instance */
#define MOO_APP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), MOO_TYPE_APP, MooAppClass))

/*!< \brief The main application object for medit */
typedef struct _MooApp MooApp;

/*!< \brief Private data for the MooApp object */
typedef struct _MooAppPrivate MooAppPrivate;

/*!< \brief Class structure for the MooApp object */
typedef struct _MooAppClass MooAppClass;

/*!
 * \brief The main application object for medit
 */
struct _MooApp
{
  GObject parent;      /*!< \brief The parent GObject */
  MooAppPrivate *priv; /*!< \brief Pointer to private data */
};

/*!
 * \brief Class structure for the MooApp object
 */
struct _MooAppClass
{
  GObjectClass parent_class; /*!< \brief The parent class structure */

  void (*started) (MooApp *app);      /*!< \brief Signal emitted when the application has started */
  void (*quit) (MooApp *app);         /*!< \brief Signal emitted when the application is about to quit */
  void (*load_session) (MooApp *app); /*!< \brief Signal emitted to load a session */
  void (*save_session) (MooApp *app); /*!< \brief Signal emitted to save a session */
  void (*init_plugins) (MooApp *app); /*!< \brief Signal emitted to initialize plugins */
};

/*!
 * \brief Returns the GType for the MooApp object
 * \return the GType for the MooApp object
 */
GType moo_app_get_type (void) G_GNUC_CONST;

/*!
 * \brief Returns the singleton instance of the MooApp object
 * \return (transfer none): the singleton instance of the MooApp object
 */
MooApp *moo_app_instance (void);

/*!
 * \brief Initializes the application
 * \param app a MooApp
 * \return TRUE on success, FALSE on failure
 */
gboolean moo_app_init (MooApp *app);

/*!
 * \brief Runs the main application loop
 * \param app a MooApp
 * \return the exit code
 */
int moo_app_run (MooApp *app);

/*!
 * \brief Quits the application
 * \param app a MooApp
 * \return TRUE on success, FALSE on failure
 */
gboolean moo_app_quit (MooApp *app);

/*!
 * \brief Loads the previous session
 * \param app a MooApp
 */
void moo_app_load_session (MooApp *app);

/*!
 * \brief Gets the editor instance associated with the application
 * \param app a MooApp
 * \return (transfer none): the editor instance
 */
MooEditor *moo_app_get_editor (MooApp *app);

/*!
 * \brief Sends a message to another process
 * \param pid the process ID to send the message to
 * \param data the message data
 * \param len the length of the message data
 * \return TRUE on success, FALSE on failure
 */
gboolean moo_app_send_msg (const char *pid, const char *data, gssize len);

/*!
 * \brief Sends files to another process
 * \param files an array of files to send
 * \param stamp a timestamp
 * \param pid the process ID to send the files to
 * \return TRUE on success, FALSE on failure
 */
gboolean moo_app_send_files (MooOpenInfoArray *files, guint32 stamp, const char *pid);

/*!
 * \brief Opens files in the application
 * \param app a MooApp
 * \param files an array of files to open
 * \param stamp a timestamp
 */
void moo_app_open_files (MooApp *app, MooOpenInfoArray *files, guint32 stamp);

/*!
 * \brief Gets system information including application version, OS details, and library versions
 * \param app a MooApp
 * \return (transfer full): a newly allocated string containing system information
 */
char *
moo_app_get_system_info (MooApp *app);

G_END_DECLS

#endif /* _mooapp_mooapp_h_ */
