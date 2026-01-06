/*
 *   mooapp/mooappabout.h
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

#ifndef MOO_APP_ABOUT_H
#define MOO_APP_ABOUT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <gtk/gtk.h>
#include <mooglib/moo-glib.h>

G_BEGIN_DECLS

void
moo_app_about_dialog (GtkWidget *parent);

G_END_DECLS

#endif /* MOO_APP_ABOUT_H */
