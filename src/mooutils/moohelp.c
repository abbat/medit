/*
 *   moohelp.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "moohelp.h"
#include "mooaccel.h"
#include "mooutils-misc.h"
#include "mooi18n.h"
#include "moodialogs.h"

#include <gdk/gdkkeysyms.h>


void
moo_help_open (GtkWidget *parent)
{
    g_return_if_fail (!parent || GTK_IS_WIDGET (parent));

    if (!moo_open_url (MOO_WEBSITE))
        moo_error_dialog (_("Could not open the medit web site"), MOO_WEBSITE, parent);
}


static gboolean
moo_help_key_press (GtkWidget   *widget,
                    GdkEventKey *event)
{
    if (!moo_accel_check_event (widget, event,
                                MOO_ACCEL_HELP_KEY,
                                MOO_ACCEL_HELP_MODS))
        return FALSE;

    moo_help_open (widget);

    return TRUE;
}


void
moo_help_connect_keys (GtkWidget *widget)
{
    g_return_if_fail (GTK_IS_WIDGET (widget));

    g_signal_handlers_disconnect_by_func (widget, (gpointer) moo_help_key_press, NULL);
    g_signal_connect (widget, "key-press-event", G_CALLBACK (moo_help_key_press), NULL);
}
