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

G_END_DECLS

#endif /* MOO_BUILDER_H */
