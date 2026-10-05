/*
 *   mooactiongroup.c
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

#include "mooutils/mooactiongroup.h"
#include "mooutils/mooaction-private.h"
#include "mooutils/mooactionbase.h"
#include "mooutils/mooutils-misc.h"


G_DEFINE_TYPE (MooActionGroup, _moo_action_group, G_TYPE_OBJECT)


/* The group an action sits in: a plain pointer on the action, set by
   insert and cleared by remove or by the group's finalize. The group owns
   the action, not the other way round. */
static GQuark
group_quark (void)
{
    static GQuark quark;
    if (G_UNLIKELY (!quark))
        quark = g_quark_from_static_string ("moo-action-group");
    return quark;
}

MooActionGroup *
_moo_action_get_group (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);
    return (MooActionGroup*) g_object_get_qdata (G_OBJECT (action), group_quark ());
}


const char *
_moo_action_group_get_display_name (MooActionGroup *group)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);

    if (group->display_name)
        return group->display_name;
    if (group->name)
        return group->name;
    return "Actions";
}


void
_moo_action_group_set_display_name (MooActionGroup *group,
                                    const char     *display_name)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    MOO_ASSIGN_STRING (group->display_name, display_name);
}


static void
_moo_action_group_init (MooActionGroup *group)
{
    group->actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}


static void
moo_action_group_finalize (GObject *object)
{
    MooActionGroup *group = MOO_ACTION_GROUP (object);

    GHashTableIter iter;
    gpointer action;
    g_hash_table_iter_init (&iter, group->actions);
    while (g_hash_table_iter_next (&iter, NULL, &action))
        g_object_set_qdata (G_OBJECT (action), group_quark (), NULL);

    g_hash_table_destroy (group->actions);
    g_free (group->name);
    g_free (group->display_name);

    G_OBJECT_CLASS (_moo_action_group_parent_class)->finalize (object);
}


static void
_moo_action_group_class_init (MooActionGroupClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = moo_action_group_finalize;
}


MooActionGroup *
_moo_action_group_new (MooActionCollection *collection,
                       const char          *name,
                       const char          *display_name)
{
    MooActionGroup *group = MOO_ACTION_GROUP (g_object_new (MOO_TYPE_ACTION_GROUP, (const char*) NULL));
    group->name = g_strdup (name);
    group->display_name = g_strdup (display_name);
    group->collection = collection;
    return group;
}


MooActionCollection *
_moo_action_group_get_collection (MooActionGroup *group)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    return group->collection;
}


void
_moo_action_group_set_collection (MooActionGroup      *group,
                                  MooActionCollection *collection)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    g_return_if_fail (!collection || MOO_IS_ACTION_COLLECTION (collection));
    group->collection = collection;
}


const char *
moo_action_group_get_name (MooActionGroup *group)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    return group->name;
}

void
moo_action_group_insert_action (MooActionGroup *group,
                                MooAction      *action)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    g_return_if_fail (MOO_IS_ACTION (action));

    const char *name = moo_action_get_name (action);
    g_return_if_fail (name != NULL);

    /* an action of the same name is replaced, too */
    MooAction *old = (MooAction*) g_hash_table_lookup (group->actions, name);
    if (old == action)
        return;
    if (old)
        g_object_set_qdata (G_OBJECT (old), group_quark (), NULL);

    g_hash_table_replace (group->actions, (char*) name, g_object_ref (action));
    g_object_set_qdata (G_OBJECT (action), group_quark (), group);
}

void
moo_action_group_remove_action (MooActionGroup *group,
                                MooAction      *action)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    g_return_if_fail (MOO_IS_ACTION (action));

    const char *name = moo_action_get_name (action);
    if (g_hash_table_lookup (group->actions, name) != action)
        return;

    g_object_set_qdata (G_OBJECT (action), group_quark (), NULL);
    g_hash_table_remove (group->actions, name);
}

MooAction *
moo_action_group_get_action (MooActionGroup *group,
                             const char     *name)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    return (MooAction*) g_hash_table_lookup (group->actions, name);
}

GList *
moo_action_group_list_actions (MooActionGroup *group)
{
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    return g_hash_table_get_values (group->actions);
}

