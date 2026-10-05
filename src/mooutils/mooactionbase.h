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


G_BEGIN_DECLS


#define MOO_TYPE_ACTION_BASE                (moo_action_base_get_type ())
#define MOO_ACTION_BASE(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOO_TYPE_ACTION_BASE, MooActionBase))
#define MOO_IS_ACTION_BASE(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOO_TYPE_ACTION_BASE))
#define MOO_ACTION_BASE_GET_CLASS(inst)     (G_TYPE_INSTANCE_GET_INTERFACE ((inst), MOO_TYPE_ACTION_BASE, MooActionBaseClass))

typedef struct _MooActionBase       MooActionBase;
typedef struct _MooActionBaseClass  MooActionBaseClass;

struct _MooActionBaseClass {
    GTypeInterface parent;

    void (*connect_proxy)    (MooActionBase *action,
                              GtkWidget     *proxy);
    void (*disconnect_proxy) (MooActionBase *action,
                              GtkWidget     *proxy);
};


GType        moo_action_base_get_type     (void) G_GNUC_CONST;

/* Accessors for the properties of an action. They delegate to GtkAction for now,
   so that the day it goes the change is in one place. */
const char  *moo_action_get_name          (GtkAction       *action);
gboolean     moo_action_get_sensitive     (GtkAction       *action);
void         moo_action_set_sensitive     (GtkAction       *action,
                                           gboolean         sensitive);
gboolean     moo_action_get_visible       (GtkAction       *action);
gboolean     moo_action_is_visible        (GtkAction       *action);
void         moo_action_set_visible       (GtkAction       *action,
                                           gboolean         visible);
void         moo_action_activate          (GtkAction       *action);
const char  *moo_action_get_accel_path    (GtkAction       *action);
void         moo_action_set_accel_path    (GtkAction       *action,
                                           const char      *accel_path);
gboolean     moo_toggle_action_get_active (GtkToggleAction *action);
void         moo_toggle_action_set_active (GtkToggleAction *action,
                                           gboolean         active);


G_END_DECLS

#endif /* MOO_ACTION_BASE_H */
