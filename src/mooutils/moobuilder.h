/*
 *   moobuilder.h
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

#ifndef MOO_BUILDER_H
#define MOO_BUILDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*!
 * \brief Reads a text file out of a GResource
 * \param resource_path resource path, e.g. "/ui/medit.xml"
 * \param length where to store the length in bytes, or NULL
 * \return the contents, to be freed with g_free(); NULL on error, reported as a critical
 */
char *moo_resource_get_text (const char *resource_path, gsize *length);

/*!
 * \brief Builds an interface from a GResource, in the application's text domain
 * \param resource_path resource path of the .ui file, e.g. "/ui/mootextgotoline.ui"
 * \return the builder, or NULL on error (which is reported as a critical)
 */
GtkBuilder *moo_builder_new (const char *resource_path);

/*!
 * \brief Looks up an object built by moo_builder_new()
 * \param builder the builder
 * \param id object id, as written in the .ui file
 * \return the object; a missing id is a programming error and is reported as a critical
 */
gpointer moo_builder_get (GtkBuilder *builder, const char *id);

/*!
 * \brief Moves a widget built by moo_builder_new() into a parent of your own
 * \param builder the builder
 * \param id id of the widget to move
 * \param parent container to put it in
 *
 * Interfaces that describe a piece of a window rather than a whole one keep it
 * inside a placeholder window; this takes the piece out and drops the
 * placeholder.
 */
void moo_builder_reparent (GtkBuilder *builder, const char *id, GtkWidget *parent);

/*!
 * \brief Takes a widget out of the interface, to be put somewhere by the caller
 * \param builder the builder
 * \param id id of the widget
 * \return the widget, with a floating reference, so that whatever container it
 *         is handed to takes ownership of it in the usual way
 *
 * Use this when the widget is returned to code that will pack it itself. Handing
 * out a widget that still sits in its placeholder window makes that code fail
 * with "Can't set a parent on widget which has a parent", and the packing
 * properties then land on the placeholder instead.
 */
GtkWidget *moo_builder_take (GtkBuilder *builder, const char *id);

G_END_DECLS

#endif /* MOO_BUILDER_H */
