/*
 *   mooactionbase.h
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

#ifndef MOO_ACTION_BASE_H
#define MOO_ACTION_BASE_H


#include "mooutils/mooaction.h"

G_BEGIN_DECLS


#define MOO_TYPE_ACTION_BASE                (moo_action_base_get_type ())
#define MOO_ACTION_BASE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOO_TYPE_ACTION_BASE, MooActionBase))
#define MOO_IS_ACTION_BASE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOO_TYPE_ACTION_BASE))
#define MOO_ACTION_BASE_GET_CLASS(inst)     (G_TYPE_INSTANCE_GET_INTERFACE ((inst), MOO_TYPE_ACTION_BASE, MooActionBaseClass))

typedef struct _MooActionBase       MooActionBase;
typedef struct _MooActionBaseClass  MooActionBaseClass;

struct _MooActionBaseClass {
    GTypeInterface parent;
};


GType        moo_action_base_get_type     (void) G_GNUC_CONST;

/* Accessors for the properties of an action. They read and write the fields of
   MooAction; the toggle ones are defined in mooaction.cpp. */
const char  *moo_action_get_name          (MooAction       *action);
gboolean     moo_action_get_sensitive     (MooAction       *action);
void         moo_action_set_sensitive     (MooAction       *action,
                                           gboolean         sensitive);
gboolean     moo_action_get_visible       (MooAction       *action);
gboolean     moo_action_is_visible        (MooAction       *action);
void         moo_action_set_visible       (MooAction       *action,
                                           gboolean         visible);
void         moo_action_activate          (MooAction       *action);
const char  *moo_action_get_accel_path    (MooAction       *action);
void         moo_action_set_accel_path    (MooAction       *action,
                                           const char      *accel_path);
gboolean     moo_toggle_action_get_active (MooToggleAction *action);
void         moo_toggle_action_set_active (MooToggleAction *action,
                                           gboolean         active);

/* Proxies: the menu items and tool buttons that mirror an action. They are
   created and kept in step here, not by MooAction. */
GtkWidget   *moo_action_create_menu_item  (MooAction       *action);
/* the bare item, for create_menu_item overrides to start from */
GtkWidget   *moo_action_create_default_menu_item (MooAction *action);
GtkWidget   *moo_action_create_tool_item  (MooAction       *action);
void         moo_action_sync_proxies      (MooAction       *action);


G_END_DECLS

#endif /* MOO_ACTION_BASE_H */
