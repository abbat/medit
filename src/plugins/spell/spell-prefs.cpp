/*
 *   plugins/spell/spell-prefs.cpp
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

#include "plugins/spell/spell-plugin.h"

#include "mooutils/moobuilder.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooprefspage.h"


static void
prefs_page_apply (G_GNUC_UNUSED GtkBuilder *builder)
{
    /* The keys are written already; this makes them count without a restart. */
    _moo_spell_apply_prefs ();
}


GtkWidget *
_moo_spell_prefs_page (G_GNUC_UNUSED MooPlugin *plugin)
{
    GtkWidget *page;
    GtkBuilder *builder;

    page = moo_prefs_page_new (_("Spell Checking"), GTK_STOCK_SPELL_CHECK);

    builder = moo_builder_new ("/ui/spell-prefs.ui");
    g_return_val_if_fail (builder != NULL, NULL);

    moo_builder_reparent (builder, "PrefsPage", page);
    g_object_set_data_full (G_OBJECT (page), "moo-builder", builder, g_object_unref);

    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "languages")),
                                 MOO_SPELL_PREFS_LANGUAGES);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "extensions")),
                                 MOO_SPELL_PREFS_EXTENSIONS);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "check_code")),
                                 MOO_SPELL_PREFS_CHECK_CODE);

    g_signal_connect_swapped (page, "apply", G_CALLBACK (prefs_page_apply), builder);

    return page;
}
