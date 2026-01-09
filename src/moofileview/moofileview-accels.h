/*
 *   moofileview-accels.h
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

#ifndef MOO_FILE_VIEW_ACCELS_H
#define MOO_FILE_VIEW_ACCELS_H

#include <mooutils/mooaccel.h>

#define MOO_FILE_VIEW_ACCEL_CUT   MOO_ACCEL_CUT
#define MOO_FILE_VIEW_ACCEL_COPY  MOO_ACCEL_COPY
#define MOO_FILE_VIEW_ACCEL_PASTE MOO_ACCEL_PASTE

#define MOO_FILE_VIEW_ACCEL_GO_UP       "<Alt>Up"
#define MOO_FILE_VIEW_ACCEL_GO_BACK     "<Alt>Left"
#define MOO_FILE_VIEW_ACCEL_GO_FORWARD  "<Alt>Right"
#define MOO_FILE_VIEW_ACCEL_GO_HOME     "<Alt>Home"
#define MOO_FILE_VIEW_ACCEL_DELETE      "<Alt>Delete"
#define MOO_FILE_VIEW_ACCEL_SHOW_HIDDEN "<Alt><Shift>H"
#define MOO_FILE_VIEW_ACCEL_PROPERTIES  "<Alt>Return"

#define MOO_FILE_VIEW_BINDING_GO_UP         GDK_KEY_Up,       GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_UP_KP      GDK_KEY_KP_Up,    GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_BACK       GDK_KEY_Left,     GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_BACK_KP    GDK_KEY_KP_Left,  GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_FORWARD    GDK_KEY_Right,    GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_FORWARD_KP GDK_KEY_KP_Right, GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_HOME       GDK_KEY_Home,     GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_GO_HOME_KP    GDK_KEY_KP_Home,  GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_DELETE        GDK_KEY_Delete,   GDK_MOD1_MASK
#define MOO_FILE_VIEW_BINDING_PROPERTIES    GDK_KEY_Return,   GDK_MOD1_MASK

#endif /* MOO_FILE_VIEW_ACCELS_H */
