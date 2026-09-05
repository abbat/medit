/*
 *   terminal-prefs.cpp
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugins/terminal/terminal-plugin.h"
#include "plugins/terminal/terminal-colors.h"

#include "mooutils/moobuilder.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooprefspage.h"
#include "mooutils/moostock.h"

#include <string.h>


static void
update_font_sensitivity (GtkBuilder *builder)
{
    GtkWidget *check = GTK_WIDGET (moo_builder_get (builder, "use_default_font"));
    GtkWidget *button = GTK_WIDGET (moo_builder_get (builder, "font"));

    gtk_widget_set_sensitive (button,
                              !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}


static void
default_font_toggled (G_GNUC_UNUSED GtkToggleButton *check,
                      GtkBuilder *builder)
{
    update_font_sensitivity (builder);
}


static void
prefs_page_init (GtkBuilder *builder)
{
    GtkComboBox *combo = GTK_COMBO_BOX (moo_builder_get (builder, "color_scheme"));
    GtkToggleButton *check = GTK_TOGGLE_BUTTON (moo_builder_get (builder, "use_default_font"));
    GtkWidget *button = GTK_WIDGET (moo_builder_get (builder, "font"));
    const MooTerminalColorScheme *schemes;
    const MooTerminalColorScheme *current;
    const char *font_name;
    guint n_schemes, i;

    schemes = _moo_terminal_color_schemes (&n_schemes);
    current = _moo_terminal_color_scheme_lookup (
        moo_prefs_get_string (MOO_TERMINAL_PREFS_COLOR_SCHEME));

    for (i = 0; i < n_schemes; ++i)
    {
        if (&schemes[i] == current)
        {
            gtk_combo_box_set_active (combo, (int) i);
            break;
        }
    }

    if (i == n_schemes)
        gtk_combo_box_set_active (combo, 0);

    font_name = moo_prefs_get_string (MOO_TERMINAL_PREFS_FONT);

    gtk_toggle_button_set_active (check, !font_name || !font_name[0]);

    if (font_name && font_name[0])
    {
        g_object_set (button, "font-name", font_name, nullptr);
    }
    else
    {
        /* so that clearing the check box starts from the font in the pane */
        char *fallback = _moo_terminal_get_default_font ();
        g_object_set (button, "font-name", fallback, nullptr);
        g_free (fallback);
    }

    update_font_sensitivity (builder);
}


static void
prefs_page_apply (GtkBuilder *builder)
{
    GtkComboBox *combo = GTK_COMBO_BOX (moo_builder_get (builder, "color_scheme"));
    GtkToggleButton *check = GTK_TOGGLE_BUTTON (moo_builder_get (builder, "use_default_font"));
    GtkWidget *button = GTK_WIDGET (moo_builder_get (builder, "font"));
    const MooTerminalColorScheme *schemes;
    guint n_schemes;
    int index;

    schemes = _moo_terminal_color_schemes (&n_schemes);
    index = gtk_combo_box_get_active (combo);

    if (index >= 0 && (guint) index < n_schemes)
        moo_prefs_set_string (MOO_TERMINAL_PREFS_COLOR_SCHEME, schemes[index].name);

    if (gtk_toggle_button_get_active (check))
    {
        moo_prefs_set_string (MOO_TERMINAL_PREFS_FONT, NULL);
    }
    else
    {
        char *font_name = NULL;
        g_object_get (button, "font-name", &font_name, nullptr);
        moo_prefs_set_string (MOO_TERMINAL_PREFS_FONT, font_name);
        g_free (font_name);
    }

    _moo_terminal_apply_prefs ();
}


GtkWidget *
_moo_terminal_prefs_page (G_GNUC_UNUSED MooPlugin *plugin)
{
    GtkWidget *page;
    GtkBuilder *builder;
    GtkComboBoxText *combo;
    const MooTerminalColorScheme *schemes;
    guint n_schemes, i;

    page = moo_prefs_page_new (_("Terminal"), MOO_STOCK_TERMINAL);

    builder = moo_builder_new ("/ui/terminal-prefs.ui");
    g_return_val_if_fail (builder != NULL, NULL);

    moo_builder_reparent (builder, "PrefsPage", page);
    g_object_set_data_full (G_OBJECT (page), "moo-builder", builder, g_object_unref);

    combo = GTK_COMBO_BOX_TEXT (moo_builder_get (builder, "color_scheme"));
    schemes = _moo_terminal_color_schemes (&n_schemes);

    for (i = 0; i < n_schemes; ++i)
        gtk_combo_box_text_append_text (combo, _(schemes[i].display_name));

    _moo_terminal_font_chooser_filter (GTK_WIDGET (moo_builder_get (builder, "font")));

    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "shell")),
                                 MOO_TERMINAL_PREFS_SHELL);

    g_signal_connect (moo_builder_get (builder, "use_default_font"), "toggled",
                      G_CALLBACK (default_font_toggled), builder);

    g_signal_connect_swapped (page, "init", G_CALLBACK (prefs_page_init), builder);
    g_signal_connect_swapped (page, "apply", G_CALLBACK (prefs_page_apply), builder);

    return page;
}
