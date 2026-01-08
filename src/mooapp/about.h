/*
 *   mooapp/about.h
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

#pragma once
#ifndef _mooapp_about_h_
#define _mooapp_about_h_

#include "sysheaders.h"

G_BEGIN_DECLS

/*!
 * \brief Structure containing all widgets from the about dialog
 */
typedef struct _AboutDialog AboutDialog;

/*!
 * \brief Creates a new AboutDialog structure with all widgets initialized
 * \param parent Parent widget for the dialog
 * \return A newly allocated AboutDialog structure
 */
AboutDialog *about_dialog_new (GtkWidget *parent);

/*!
 * \brief Frees the AboutDialog structure and all its widgets
 * \param dialog An AboutDialog structure
 */
void about_dialog_free (AboutDialog *dialog);

G_END_DECLS

#endif /* _mooapp_about_h_ */
