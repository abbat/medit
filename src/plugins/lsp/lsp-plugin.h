/*
 *   plugins/lsp/lsp-plugin.h
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

#ifndef MOO_LSP_PLUGIN_H
#define MOO_LSP_PLUGIN_H

#include "mooedit/mooplugin.h"

G_BEGIN_DECLS

#define MOO_LSP_PLUGIN_ID "Lsp"

/*
 * mooplugin-builtin.cpp includes this header even when MOO_BUILD_LSP is off,
 * so nothing from json-glib may appear in it. See the ENABLE_LSP block in the
 * top CMakeLists.txt.
 */

#define MOO_LSP_PREFS_ENABLED       "Plugins/Lsp/enabled"
#define MOO_LSP_PREFS_DIAGNOSTICS   "Plugins/Lsp/diagnostics"
#define MOO_LSP_PREFS_COMPLETION    "Plugins/Lsp/completion"
#define MOO_LSP_PREFS_HOVER         "Plugins/Lsp/hover"
#define MOO_LSP_PREFS_SYNC_DELAY    "Plugins/Lsp/sync_delay"
#define MOO_LSP_PREFS_DEBUG         "Plugins/Lsp/debug"

/* Milliseconds between the last edit and the didChange notification. */
#define MOO_LSP_SYNC_DELAY_DEFAULT 300

gboolean    moo_lsp_plugin_init     (void);

/*
 * Whether to log every message to stderr. Either the preference, or
 * MEDIT_LSP_DEBUG in the environment, which is the only way to get at it from
 * a container or a test run that has no preferences file.
 */
gboolean    _moo_lsp_debug          (void);

G_END_DECLS

#endif /* MOO_LSP_PLUGIN_H */
