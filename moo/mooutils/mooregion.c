/*
 *   mooregion.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mooregion.h"

#if GTK_CHECK_VERSION(3, 0, 0)
MooRegion *
moo_region_polygon (const GdkPoint *points, gint n_points)
{
  gint i;
  MooRegion *region;

  g_assert (n_points > 2);
  g_assert (points != NULL);

  region = g_malloc (sizeof (MooRegion));
  if (region == NULL)
    {
      g_warning ("moo_region_polygon: out of memory");
      exit (EXIT_FAILURE);
    }

  region->cs = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
  region->cr = cairo_create (region->cs);
  if (cairo_status (region->cr) != CAIRO_STATUS_SUCCESS)
    {
      g_warning ("moo_region_polygon: cairo_create failed");
      exit (EXIT_FAILURE);
    }

  cairo_new_path (region->cr);
  cairo_move_to (region->cr, points[0].x, points[0].y);

  for (i = 1; i < n_points; i++)
    cairo_line_to (region->cr, points[i].x, points[i].y);

  cairo_close_path (region->cr);

  return region;
}
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
gboolean
moo_region_point_in (const MooRegion *region, int x, int y)
{
  g_assert (region != NULL);
  g_assert (region->cr != NULL);

  return cairo_in_fill (region->cr, x, y);
}
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
void
moo_region_destroy (MooRegion *region)
{
  g_assert (region != NULL);
  g_assert (region->cr != NULL);
  g_assert (region->cs != NULL);

  cairo_destroy (region->cr);
  cairo_surface_destroy (region->cs);
  g_free (region);
}
#endif
