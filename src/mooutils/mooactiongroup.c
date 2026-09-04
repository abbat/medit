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
#include "mooutils/mooutils-misc.h"


G_DEFINE_TYPE (MooActionGroup, _moo_action_group, GTK_TYPE_ACTION_GROUP)


const char *
_moo_action_group_get_display_name (MooActionGroup *group)
{
    const char *name;

    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);

    name = group->display_name;

    if (!name)
        name = gtk_action_group_get_name (GTK_ACTION_GROUP (group));

    if (!name)
        name = "Actions";

    return name;
}


void
_moo_action_group_set_display_name (MooActionGroup *group,
                                    const char     *display_name)
{
    g_return_if_fail (MOO_IS_ACTION_GROUP (group));
    MOO_ASSIGN_STRING (group->display_name, display_name);
}


static void
_moo_action_group_init (G_GNUC_UNUSED MooActionGroup *group)
{
}


static void
moo_action_group_finalize (GObject *object)
{
    MooActionGroup *group = MOO_ACTION_GROUP (object);

    g_free (group->display_name);

    G_OBJECT_CLASS (_moo_action_group_parent_class)->finalize (object);
}


static void
_moo_action_group_class_init (MooActionGroupClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = moo_action_group_finalize;
}


MooActionGroup *
_moo_action_group_new (MooActionCollection *collection,
                       const char          *name,
                       const char          *display_name)
{
    MooActionGroup *group = MOO_ACTION_GROUP (g_object_new (MOO_TYPE_ACTION_GROUP, "name", name, (const char*) NULL));
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


