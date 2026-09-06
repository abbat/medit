/*
 *   plugins/lsp/lsp-prefs.cpp
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

#include "plugins/lsp/lsp-plugin.h"
#include "plugins/lsp/lsp-config.h"
#include "plugins/lsp/lsp-manager.h"

#include "mooedit/mooeditor.h"
#include "mooutils/moobuilder.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooprefspage.h"
#include "mooutils/moodialogs.h"


void
_moo_lsp_edit_config (GtkWidget *parent)
{
    MooEditor *editor = moo_editor_instance ();
    GError *error = NULL;
    char *filename;

    filename = lsp_config_ensure_user_file (&error);

    if (!filename)
    {
        moo_error_dialog (_("Could not create the LSP configuration file"),
                          error ? error->message : NULL, parent);
        g_clear_error (&error);
        return;
    }

    moo_editor_open_path (editor, filename, NULL, -1, NULL);

    g_free (filename);
}


static void
edit_config_clicked (GtkWidget *button)
{
    _moo_lsp_edit_config (gtk_widget_get_toplevel (button));
}


static void
restart_clicked (void)
{
    lsp_manager_reload ();
}


static void
prefs_page_apply (G_GNUC_UNUSED GtkBuilder *builder)
{
    /*
     * moo_prefs_page_bind_setting() has already written the keys; this makes
     * them take effect without waiting for a restart.
     */
    _moo_lsp_apply_prefs ();
}


GtkWidget *
_moo_lsp_prefs_page (G_GNUC_UNUSED MooPlugin *plugin)
{
    GtkWidget *page;
    GtkBuilder *builder;

    page = moo_prefs_page_new (_("Language Servers"), GTK_STOCK_INDEX);

    builder = moo_builder_new ("/ui/lsp-prefs.ui");
    g_return_val_if_fail (builder != NULL, NULL);

    moo_builder_reparent (builder, "PrefsPage", page);
    g_object_set_data_full (G_OBJECT (page), "moo-builder", builder, g_object_unref);

    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "diagnostics")),
                                 MOO_LSP_PREFS_DIAGNOSTICS);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "completion")),
                                 MOO_LSP_PREFS_COMPLETION);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "hover")),
                                 MOO_LSP_PREFS_HOVER);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "sync_delay")),
                                 MOO_LSP_PREFS_SYNC_DELAY);
    moo_prefs_page_bind_setting (MOO_PREFS_PAGE (page),
                                 GTK_WIDGET (moo_builder_get (builder, "debug")),
                                 MOO_LSP_PREFS_DEBUG);

    g_signal_connect_swapped (moo_builder_get (builder, "edit_config"), "clicked",
                              G_CALLBACK (edit_config_clicked),
                              moo_builder_get (builder, "edit_config"));
    g_signal_connect (moo_builder_get (builder, "restart"), "clicked",
                      G_CALLBACK (restart_clicked), NULL);

    g_signal_connect_swapped (page, "apply", G_CALLBACK (prefs_page_apply), builder);

    return page;
}
