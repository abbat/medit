/*
 *   terminal-colors.cpp
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugins/terminal/terminal-colors.h"
#include "mooutils/mooi18n.h"

#include <string.h>

/* Shamelessly stolen from Konsole, the best terminal emulator out there,
   by way of the python plugin this one replaces. */
static const MooTerminalColorScheme color_schemes[] = {
    { "Default", N_("Default"),
        { NULL } },
    { "Black on White", N_("Black on White"),
        { "#000000", "#ffffff", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#000000", "#ffffff",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Black on Light Yellow", N_("Black on Light Yellow"),
        { "#000000", "#ffffdd", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#000000", "#ffffdd",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Marble", N_("Marble"),
        { "#ffffff", "#000000", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#ffffff", "#000000",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Green on Black", N_("Green on Black"),
        { "#18f018", "#000000", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#18f018", "#000000",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Paper, Light", N_("Paper, Light"),
        { "#000000", "#ffffff", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#000000", "#ffffff",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Paper", N_("Paper"),
        { "#000000", "#ffffff", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#000000", "#ffffff",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "Linux Colors", N_("Linux Colors"),
        { "#b2b2b2", "#000000", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#ffffff", "#686868",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } },
    { "VIM Colors", N_("VIM Colors"),
        { "#000000", "#ffffff", "#000000", "#c00000", "#008000", "#808000",
          "#0000c0", "#c000c0", "#008080", "#c0c0c0", "#4d4d4d", "#ffffff",
          "#808080", "#ff6060", "#00ff00", "#ffff00", "#8080ff", "#ff40ff",
          "#00ffff", "#ffffff" } },
    { "White on Black", N_("White on Black"),
        { "#ffffff", "#000000", "#000000", "#b21818", "#18b218", "#b26818",
          "#1818b2", "#b218b2", "#18b2b2", "#b2b2b2", "#ffffff", "#000000",
          "#686868", "#ff5454", "#54ff54", "#ffff54", "#5454ff", "#ff54ff",
          "#54ffff", "#ffffff" } }
};


const MooTerminalColorScheme *
_moo_terminal_color_schemes (guint *n_schemes)
{
    if (n_schemes)
        *n_schemes = G_N_ELEMENTS (color_schemes);

    return color_schemes;
}


const MooTerminalColorScheme *
_moo_terminal_color_scheme_lookup (const char *name)
{
    guint i;

    if (!name || !name[0])
        return &color_schemes[0];

    for (i = 0; i < G_N_ELEMENTS (color_schemes); ++i)
        if (strcmp (color_schemes[i].name, name) == 0)
            return &color_schemes[i];

    return NULL;
}


/*
 * What the python plugin read as style->text[NORMAL] and style->base[NORMAL].
 * A bare widget context returns a fully transparent background on GTK+3, the
 * view class is what carries the base color, so ask for it under that class.
 */
static gboolean
get_theme_colors (GtkWidget *widget,
                  GdkRGBA   *fg,
                  GdkRGBA   *bg)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context (widget);

    gtk_style_context_save (ctx);
    gtk_style_context_add_class (ctx, GTK_STYLE_CLASS_VIEW);
    gtk_style_context_set_state (ctx, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_get_color (ctx, GTK_STATE_FLAG_NORMAL, fg);
    gtk_style_context_get_background_color (ctx, GTK_STATE_FLAG_NORMAL, bg);
    gtk_style_context_restore (ctx);

    /* A theme that paints its view background with a css image rather than a
       color leaves this transparent; vte's own defaults are better than a
       terminal one cannot read. */
    return bg->alpha > 0.0;
}


void
_moo_terminal_color_scheme_apply (const MooTerminalColorScheme *scheme,
                                  VteTerminal                  *terminal)
{
    GdkRGBA colors[MOO_TERMINAL_SCHEME_N_COLORS];
    GdkRGBA palette[MOO_TERMINAL_PALETTE_SIZE];
    guint i;

    g_return_if_fail (VTE_IS_TERMINAL (terminal));

    if (!scheme)
        scheme = &color_schemes[0];

    if (!scheme->colors[0])
    {
        GdkRGBA fg, bg;

        if (get_theme_colors (GTK_WIDGET (terminal), &fg, &bg))
            vte_terminal_set_colors (terminal, &fg, &bg, NULL, 0);
        else
            vte_terminal_set_default_colors (terminal);

        return;
    }

    for (i = 0; i < MOO_TERMINAL_SCHEME_N_COLORS; ++i)
    {
        if (!gdk_rgba_parse (&colors[i], scheme->colors[i]))
        {
            g_critical ("could not parse color '%s' of scheme '%s'",
                        scheme->colors[i], scheme->name);
            vte_terminal_set_default_colors (terminal);
            return;
        }
    }

    /* slots 2..9 are the normal colors, 12..19 the bright ones */
    memcpy (palette, colors + 2, 8 * sizeof palette[0]);
    memcpy (palette + 8, colors + 12, 8 * sizeof palette[0]);

    vte_terminal_set_colors (terminal, &colors[0], &colors[1],
                             palette, MOO_TERMINAL_PALETTE_SIZE);
}
