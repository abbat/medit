/*
 *   plugins/lsp/lsp-plugin.cpp
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

/*
 * A client for the language server protocol, built around one server process
 * per project root as described in lsp.xml. The whole plugin is compiled out
 * when cmake does not find json-glib.
 *
 * Nothing here depends on the gtk version: unlike the terminal, this builds
 * for both.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugins/lsp/lsp-plugin.h"
#include "plugins/lsp/lsp-manager.h"

#include "mooedit/mooplugin-macro.h"
#include "mooutils/mooi18n.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooutils-misc.h"

gboolean
_moo_lsp_debug (void)
{
    return moo_getenv_bool ("MEDIT_LSP_DEBUG") ||
           moo_prefs_get_bool (MOO_LSP_PREFS_DEBUG);
}


typedef struct {
    MooPlugin parent;
} LspPlugin;

typedef struct {
    MooDocPlugin parent;
} LspDocPlugin;

MOO_PLUGIN_DEFINE_INFO (lsp,
                        N_("LSP"), N_("Language server protocol client"),
                        "Anton Batenev <antonbatenev@yandex.ru>",
                        MOO_VERSION)

MOO_DOC_PLUGIN_DEFINE (Lsp, lsp)


/*
 * Saving an untitled document, and choosing another language by hand, both
 * change which server applies -- or whether one applies at all -- so the
 * document is detached and attached again.
 *
 * Both signals also fire while a document is being opened, when nothing has
 * really changed yet, so an attached document that still matches is left
 * alone; otherwise every file opened would be announced to the server twice.
 */
static void
doc_changed_identity (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));
    LspDoc *ldoc = lsp_manager_lookup_doc (doc);

    if (ldoc && lsp_doc_is_current (ldoc))
        return;

    lsp_manager_remove_doc (doc);
    lsp_manager_add_doc (doc);
}


static gboolean
lsp_doc_plugin_create (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_connect_swapped (doc, "filename-changed",
                              G_CALLBACK (doc_changed_identity), plugin);
    g_signal_connect_swapped (doc, "notify::lang",
                              G_CALLBACK (doc_changed_identity), plugin);

    lsp_manager_add_doc (doc);

    return TRUE;
}


static void
lsp_doc_plugin_destroy (LspDocPlugin *plugin)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (plugin));

    g_signal_handlers_disconnect_by_data (doc, plugin);

    lsp_manager_remove_doc (doc);
}


static gboolean
lsp_plugin_init (G_GNUC_UNUSED LspPlugin *plugin)
{
    moo_prefs_new_key_bool (MOO_LSP_PREFS_ENABLED, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_DIAGNOSTICS, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_COMPLETION, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_HOVER, TRUE);
    moo_prefs_new_key_bool (MOO_LSP_PREFS_DEBUG, FALSE);
    moo_prefs_new_key_int (MOO_LSP_PREFS_SYNC_DELAY, MOO_LSP_SYNC_DELAY_DEFAULT);

    lsp_manager_init ();

    return TRUE;
}


static void
lsp_plugin_deinit (G_GNUC_UNUSED LspPlugin *plugin)
{
    /* Leaves no language server behind. */
    lsp_manager_shutdown ();
}


MOO_PLUGIN_DEFINE (Lsp, lsp,
                   NULL, NULL, NULL, NULL,
                   NULL,
                   0, lsp_doc_plugin_get_type ())


gboolean
moo_lsp_plugin_init (void)
{
    return moo_plugin_register (MOO_LSP_PLUGIN_ID,
                                lsp_plugin_get_type (),
                                &lsp_plugin_info,
                                NULL);
}
