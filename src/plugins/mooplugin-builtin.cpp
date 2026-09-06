/*
 *   mooplugin-builtin.c
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

#include "mooplugin-builtin.h"

#include "mooedit/mooplugin.h"
#include "plugins/mooplugin-builtin.h"
#include "mooutils/mooutils-misc.h"
#include "plugins/ctags/ctags-plugin.h"
#include "plugins/terminal/terminal-plugin.h"
#include "plugins/lsp/lsp-plugin.h"

void
moo_plugin_init (void)
{
    _moo_file_selector_plugin_init ();
    _moo_file_list_plugin_init ();
    _moo_find_plugin_init ();
#ifdef MOO_BUILD_CTAGS
    moo_ctags_plugin_init ();
#endif
#ifdef MOO_BUILD_TERMINAL
    moo_terminal_plugin_init ();
#endif
#ifdef MOO_BUILD_LSP
    moo_lsp_plugin_init ();
#endif

    moo_plugin_read_dirs ();

    _moo_user_tools_plugin_init ();
}
