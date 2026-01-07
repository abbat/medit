/*
 *   plugins/ctags/ctags-plugin.h
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
#ifndef _plugins_ctags_ctags_plugin_h_
#define _plugins_ctags_ctags_plugin_h_

#include "sysheaders.h"

G_BEGIN_DECLS

/**
 * \brief Initialize the ctags plugin
 *
 * This function initializes the ctags plugin for medit. It sets up
 * necessary data structures and registers plugin functionality.
 *
 * \return TRUE if initialization was successful, FALSE otherwise
 */
gboolean moo_ctags_plugin_init (void);

G_END_DECLS

#endif /* _plugins_ctags_ctags_plugin_h_ */
