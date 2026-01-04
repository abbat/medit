#ifndef MOO_GTK_H
#define MOO_GTK_H

#include <mooglib/moo-glib.h>
#include <gtk/gtk.h>


#if GTK_CHECK_VERSION(2,24,0) && defined(GTK_DISABLE_DEPRECATED)

inline static void
_moo_gdk_drawable_get_size (GdkDrawable *drawable,
                            gint        *width,
                            gint        *height)
{
    if (width)
        *width = gdk_window_get_width (GDK_WINDOW (drawable));
    if (height)
        *height = gdk_window_get_height (GDK_WINDOW (drawable));
}

#define gdk_drawable_get_size _moo_gdk_drawable_get_size

#else /* gtk-2.242.0 && DISABLE_DEPRECATED */

#endif /* gtk-2.24.0 && DISABLE_DEPRECATED */

#if !GTK_CHECK_VERSION(3,0,0)

#define gtk_widget_get_allocated_width(widget)  (widget->allocation.width)
#define gtk_widget_get_allocated_height(widget) (widget->allocation.height)

#endif /* gtk-3.x */

#endif /* MOO_GTK_H */
