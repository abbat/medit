#ifndef MOO_GTK_H
#define MOO_GTK_H

#include <mooglib/moo-glib.h>
#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(3,0,0)

#define gtk_widget_get_allocated_width(widget)  (widget->allocation.width)
#define gtk_widget_get_allocated_height(widget) (widget->allocation.height)

#endif /* gtk-3.x */

#endif /* MOO_GTK_H */
