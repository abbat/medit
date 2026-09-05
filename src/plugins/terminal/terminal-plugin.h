/*
 *   terminal-plugin.h
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *   Copyright (C) 2014 by Yannick Duchêne
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

#ifndef MOO_TERMINAL_PLUGIN_H
#define MOO_TERMINAL_PLUGIN_H

#include "mooedit/mooplugin.h"

G_BEGIN_DECLS

#define MOO_TERMINAL_PLUGIN_ID "Terminal"

/* The keys the python plugin used, kept so that an old prefs.xml still applies.
   Only the color scheme differs: it stored the translated name, this one stores
   the untranslated one, so that a scheme survives a change of locale. */
#define MOO_TERMINAL_PREFS_COLOR_SCHEME "Plugins/Terminal/color_scheme"
#define MOO_TERMINAL_PREFS_SHELL        "Plugins/Terminal/shell"
#define MOO_TERMINAL_PREFS_FONT         "Plugins/Terminal/font"

gboolean    moo_terminal_plugin_init    (void);

/* Re-reads the font and color scheme keys and applies them to every open pane. */
void        _moo_terminal_apply_prefs   (void);

GtkWidget  *_moo_terminal_prefs_page    (MooPlugin      *plugin);

/* The font a terminal uses when nothing is configured, as a pango string. */
char       *_moo_terminal_get_default_font (void);

/* Restricts a font chooser widget to monospace families. Takes a GtkWidget so
   that this header still compiles in the GTK+2 build, which has no
   GtkFontChooser and which includes it through mooplugin-builtin.cpp. */
void        _moo_terminal_font_chooser_filter (GtkWidget      *chooser);

G_END_DECLS

#endif /* MOO_TERMINAL_PLUGIN_H */
