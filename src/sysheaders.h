/*
 *   sysheaders.h
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

#pragma once
#ifndef _medit_sysheaders_h_
#define _medit_sysheaders_h_

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gmodule.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3,0,0)
#include <gtk/gtk-a11y.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <gobject/gvaluecollector.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <X11/Xatom.h>
#endif

#ifdef MOO_BUILD_TERMINAL
#include <vte/vte.h>
#endif

#ifdef MOO_BUILD_LSP
#include <json-glib/json-glib.h>
#endif

#ifdef __cplusplus
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#endif

#endif // _medit_sysheaders_h_
