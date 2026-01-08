/*
 *   mooapp/license.h
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
#ifndef _mooapp_license_h_
#define _mooapp_license_h_

#include "sysheaders.h"

G_BEGIN_DECLS

/*!
 * \brief Structure containing all widgets from the license dialog
 */
typedef struct _LicenseDialog LicenseDialog;

/*!
 * \brief Creates a new LicenseDialog structure with all widgets initialized
 * \param parent The parent widget for the dialog
 * \return A newly allocated LicenseDialog structure
 */
LicenseDialog *license_dialog_new (GtkWidget *parent);

/*!
 * \brief Frees the LicenseDialog structure and all its widgets
 * \param dialog A LicenseDialog structure
 */
void license_dialog_free (LicenseDialog *dialog);

/*!
 * \brief Sets the license text in the dialog
 * \param dialog A LicenseDialog structure
 * \param text The license text to display
 */
void license_dialog_set_text (LicenseDialog *dialog, const gchar *text);

G_END_DECLS

#endif /* _mooapp_license_h_ */
