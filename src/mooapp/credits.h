/*
 *   mooapp/credits.h
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
#ifndef _mooapp_credits_h_
#define _mooapp_credits_h_

#include "sysheaders.h"

G_BEGIN_DECLS

/*!
 * \brief Structure containing all widgets from the credits dialog
 */
typedef struct _CreditsDialog CreditsDialog;

/*!
 * \brief Creates a new CreditsDialog structure with all widgets initialized
 * \return A newly allocated CreditsDialog structure
 */
CreditsDialog* credits_dialog_new (void);

/*!
 * \brief Frees the CreditsDialog structure and all its widgets
 * \param dialog A CreditsDialog structure
 */
void credits_dialog_free (CreditsDialog *dialog);

G_END_DECLS

#endif /* _mooapp_credits_h_ */
