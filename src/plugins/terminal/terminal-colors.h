/*
 *   terminal-colors.h
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

#ifndef MOO_TERMINAL_COLORS_H
#define MOO_TERMINAL_COLORS_H

#include <gtk/gtk.h>
#include <vte/vte.h>

G_BEGIN_DECLS

/*
 * The twenty colors of a scheme, in the order the python plugin stored them:
 * foreground, background, the eight normal colors, a second foreground and
 * background that nothing ever used, and the eight bright colors. vte gets
 * slots 2..9 and 12..19 as its sixteen entry palette, which is the same slice
 * the python plugin passed.
 *
 * A scheme whose colors are all NULL takes its two colors from the gtk theme.
 */
#define MOO_TERMINAL_SCHEME_N_COLORS 20
#define MOO_TERMINAL_PALETTE_SIZE    16

typedef struct {
    const char *name;         /* stored in the prefs, never translated */
    const char *display_name; /* N_(), shown in the preferences page */
    const char *colors[MOO_TERMINAL_SCHEME_N_COLORS];
} MooTerminalColorScheme;

const MooTerminalColorScheme   *_moo_terminal_color_schemes         (guint          *n_schemes);
const MooTerminalColorScheme   *_moo_terminal_color_scheme_lookup   (const char     *name);
void                            _moo_terminal_color_scheme_apply    (const MooTerminalColorScheme *scheme,
                                                                     VteTerminal    *terminal);

G_END_DECLS

#endif /* MOO_TERMINAL_COLORS_H */
