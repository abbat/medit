/*
 *   moohelp.h
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#ifndef MOO_HELP_H
#define MOO_HELP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Help is the project page. The html manual that used to ship with medit
 * described the editor as it was in 2010 and had been wrong for years.
 */
void        moo_help_open               (GtkWidget     *parent);

/* Makes F1 on the widget open it. */
void        moo_help_connect_keys       (GtkWidget     *widget);

G_END_DECLS

#endif /* MOO_HELP_H */
