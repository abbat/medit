/*
 *   moobuilder.c
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

#include <config.h>

#include "mooutils/moobuilder.h"

GtkBuilder *
moo_builder_new (const char *resource_path)
{
    GtkBuilder *builder;
    GBytes *data;
    const char *xml;
    gsize size;
    GError *error = NULL;

    g_return_val_if_fail (resource_path != NULL, NULL);

    builder = gtk_builder_new ();
    /* Without this the labels marked translatable in the .ui file are looked up
       in the default domain and come out untranslated. */
    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);

    /* The resource is read through gio rather than with
       gtk_builder_add_from_resource(), which gtk-2 does not have. */
    data = g_resources_lookup_data (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

    if (data == NULL)
    {
        g_critical ("could not read %s: %s", resource_path, error->message);
        g_error_free (error);
        g_object_unref (builder);
        return NULL;
    }

    xml = (const char *) g_bytes_get_data (data, &size);

    if (!gtk_builder_add_from_string (builder, xml, size, &error))
    {
        g_critical ("could not build %s: %s", resource_path, error->message);
        g_error_free (error);
        g_bytes_unref (data);
        g_object_unref (builder);
        return NULL;
    }

    g_bytes_unref (data);

    return builder;
}

gpointer
moo_builder_get (GtkBuilder *builder, const char *id)
{
    GObject *object;

    g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
    g_return_val_if_fail (id != NULL, NULL);

    object = gtk_builder_get_object (builder, id);

    if (object == NULL)
        g_critical ("no object named '%s' in the interface", id);

    return object;
}
