/*
 *   plugins/spell/spell-plugin.h
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#ifndef MOO_SPELL_PLUGIN_H
#define MOO_SPELL_PLUGIN_H

#include "mooedit/mooplugin.h"

G_BEGIN_DECLS

#define MOO_SPELL_PLUGIN_ID "Spell"

/* The plugin as a whole is its own enabled state, off until asked for. */
#define MOO_SPELL_PREFS_LANGUAGES   "Plugins/Spell/languages"      /* en_US,ru_RU */
#define MOO_SPELL_PREFS_EXTENSIONS  "Plugins/Spell/extensions"     /* txt,md,rst,tex */
#define MOO_SPELL_PREFS_CHECK_CODE  "Plugins/Spell/check_code"     /* comments and strings */

gboolean    moo_spell_plugin_init   (void);

/* Preferences changed: what is open is checked again. */
void        _moo_spell_apply_prefs  (void);
GtkWidget  *_moo_spell_prefs_page   (MooPlugin *plugin);

G_END_DECLS

#endif /* MOO_SPELL_PLUGIN_H */
