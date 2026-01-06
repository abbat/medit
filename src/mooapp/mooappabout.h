/*
 *   mooapp/mooappabout.h
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

#ifndef MOO_APP_ABOUT_H
#define MOO_APP_ABOUT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#include <mooglib/moo-glib.h>
#include <errno.h>
#include <gtk/gtk.h>

#if defined(HAVE_SYS_UTSNAME_H)

static char *
get_system_name (void)
{
    struct utsname name;

    if (uname (&name) != 0)
    {
        MGW_ERROR_IF_NOT_SHARED_LIBC
        mgw_errno_t err = { errno };
        g_critical ("%s", mgw_strerror (err));
        return g_strdup ("unknown");
    }

    return g_strdup_printf ("%s %s (%s), %s", name.sysname,
                            name.release, name.version, name.machine);
}

#else

static char *
get_system_name (void)
{
    char *string;

    if (g_spawn_command_line_sync ("uname -s -r -v -m", &string, NULL, NULL, NULL))
        return string;
    else
        return g_strdup ("unknown");
}

#endif

#endif /* MOO_APP_ABOUT_H */
