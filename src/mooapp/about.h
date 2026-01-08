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
 * \brief Shows the about dialog
 * \param parent Parent widget for the dialog
 */
void show_about (GtkWidget *parent);

G_END_DECLS

#endif /* _mooapp_about_h_ */
