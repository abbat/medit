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

#include "mooutils/mooaccelbutton.h"
#include "mooutils/moocombo.h"
#include "mooutils/mooentry.h"
#include "mooutils/moofontsel.h"
#include "mooutils/moohistorycombo.h"
#include "mooutils/mooprefspage.h"

/*!
 * \brief Makes medit's own widget types known to GtkBuilder
 *
 * GtkBuilder looks types up by name, and a type that has not been used yet is
 * not registered, so an interface mentioning it fails to build with
 * "Invalid object type". Widgets from other directories register themselves the
 * same way before building an interface that uses them.
 */
static void
register_types (void)
{
    static gsize done = 0;

    if (g_once_init_enter (&done))
    {
        g_type_ensure (MOO_TYPE_ACCEL_BUTTON);
        g_type_ensure (MOO_TYPE_COMBO);
        g_type_ensure (MOO_TYPE_ENTRY);
        g_type_ensure (MOO_TYPE_FONT_BUTTON);
        g_type_ensure (MOO_TYPE_HISTORY_COMBO);
        g_type_ensure (MOO_TYPE_PREFS_PAGE);

        g_once_init_leave (&done, 1);
    }
}

char *
moo_resource_get_text (const char *resource_path, gsize *length)
{
    GBytes *data;
    gconstpointer contents;
    gsize size;
    char *text;
    GError *error = NULL;

    g_return_val_if_fail (resource_path != NULL, NULL);

    data = g_resources_lookup_data (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

    if (data == NULL)
    {
        g_critical ("could not read %s: %s", resource_path, error->message);
        g_error_free (error);
        return NULL;
    }

    contents = g_bytes_get_data (data, &size);
    text = g_strndup ((const char *) contents, size);
    g_bytes_unref (data);

    if (length != NULL)
        *length = size;

    return text;
}


GtkBuilder *
moo_builder_new (const char *resource_path)
{
    GtkBuilder *builder;
    char *xml;
    gsize size;
    GError *error = NULL;

    g_return_val_if_fail (resource_path != NULL, NULL);

    register_types ();

    builder = gtk_builder_new ();
    /* Without this the labels marked translatable in the .ui file are looked up
       in the default domain and come out untranslated. */
    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);

    /* The interface is read through gio and fed to the builder as a string:
       gtk_builder_add_from_resource() does not exist in gtk-2. */
    xml = moo_resource_get_text (resource_path, &size);

    if (xml == NULL)
    {
        g_object_unref (builder);
        return NULL;
    }

    if (!gtk_builder_add_from_string (builder, xml, size, &error))
    {
        g_critical ("could not build %s: %s", resource_path, error->message);
        g_error_free (error);
        g_free (xml);
        g_object_unref (builder);
        return NULL;
    }

    g_free (xml);

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

GtkWidget *
moo_builder_take (GtkBuilder *builder, const char *id)
{
    GtkWidget *widget;
    GtkWidget *placeholder;

    g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

    widget = GTK_WIDGET (moo_builder_get (builder, id));
    g_return_val_if_fail (widget != NULL, NULL);

    placeholder = gtk_widget_get_toplevel (widget);

    g_object_ref (widget);
    gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (widget)), widget);

    if (GTK_IS_WINDOW (placeholder))
        gtk_widget_destroy (placeholder);

    /* hand our reference over as a floating one, so the container the caller
       adds the widget to owns it, exactly as with a freshly created widget */
    g_object_force_floating (G_OBJECT (widget));

    return widget;
}


void
moo_builder_reparent (GtkBuilder *builder, const char *id, GtkWidget *parent)
{
    GtkWidget *widget;
    GtkWidget *placeholder;

    g_return_if_fail (GTK_IS_BUILDER (builder));
    g_return_if_fail (GTK_IS_CONTAINER (parent));

    widget = GTK_WIDGET (moo_builder_get (builder, id));
    g_return_if_fail (widget != NULL);

    placeholder = gtk_widget_get_toplevel (widget);

    g_object_ref (widget);
    gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (widget)), widget);
    gtk_container_add (GTK_CONTAINER (parent), widget);
    g_object_unref (widget);

    if (GTK_IS_WINDOW (placeholder))
        gtk_widget_destroy (placeholder);
}
