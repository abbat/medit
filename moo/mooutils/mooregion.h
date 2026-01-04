/*
 *   mooregion.h
 *
 *   Copyright (C) 2025-2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#ifndef MOO_REGION_H
#define MOO_REGION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#if GTK_CHECK_VERSION(3, 0, 0)

typedef struct
{
  cairo_t *cr;
  cairo_surface_t *cs;
} MooRegion;

MooRegion *moo_region_polygon (const GdkPoint *points, gint n_points);

gboolean moo_region_point_in (const MooRegion *region, int x, int y);

void moo_region_destroy (MooRegion *region);

#else

#define MooRegion GdkRegion

static inline MooRegion *
moo_region_polygon (const GdkPoint *points, gint n_points)
{
  return gdk_region_polygon (points, n_points, GDK_WINDING_RULE);
}

static inline gboolean
moo_region_point_in (const MooRegion *region, int x, int y)
{
  return gdk_region_point_in (region, x, y);
}

static inline void
moo_region_destroy (MooRegion *region)
{
  gdk_region_destroy (region);
}

#endif /* GTK-2/3 */

G_END_DECLS

#endif /* MOO_REGION_H */
