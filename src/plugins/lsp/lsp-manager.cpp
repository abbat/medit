/*
 *   plugins/lsp/lsp-manager.cpp
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

#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-config.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooedit/mooeditfiltersettings.h"
#include "mooutils/mooutils-file.h"
#include "mooutils/mooprefs.h"

#include <string.h>

/*
 * A server with nothing open is kept this long before being shut down.
 * Starting clangd or gopls again costs a fresh index, so closing one tab of a
 * project and opening another must not pay for it.
 */
#define LSP_IDLE_TIMEOUT_SEC (5 * 60)

typedef struct {
    LspServer *server;
    guint      idle_timeout;
} LspServerEntry;

typedef struct {
    LspDiagnosticsNotifyFunc func;
    gpointer                 data;
} LspListener;

static struct {
    gboolean    initialized;
    GSList     *configs;        /* LspServerConfig* */
    GHashTable *servers;        /* "id\nroot" -> LspServerEntry* */
    GSList     *docs;           /* LspDoc* */
    GSList     *listeners;      /* LspListener* */
} manager;


static void     server_state_changed    (LspServer  *server,
                                         gpointer    data);


/**********************************************************************/
/* Servers
 */

static void
server_entry_free (gpointer data)
{
    LspServerEntry *entry = (LspServerEntry*) data;

    if (entry->idle_timeout)
        g_source_remove (entry->idle_timeout);

    lsp_server_set_callbacks (entry->server, NULL, NULL, NULL);
    lsp_server_shutdown (entry->server);
    lsp_server_unref (entry->server);

    g_free (entry);
}


static gboolean
server_idle_expired (gpointer data)
{
    LspServerEntry *entry = (LspServerEntry*) data;
    GHashTableIter iter;
    gpointer key, value;

    entry->idle_timeout = 0;

    g_hash_table_iter_init (&iter, manager.servers);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        if (value == entry)
        {
            g_hash_table_iter_remove (&iter);
            break;
        }
    }

    return G_SOURCE_REMOVE;
}


static guint
count_docs_of (LspServer *server)
{
    GSList *l;
    guint count = 0;

    for (l = manager.docs; l != NULL; l = l->next)
        if (lsp_doc_get_server ((LspDoc*) l->data) == server)
            count++;

    return count;
}


static void
update_idle_timeout (LspServerEntry *entry)
{
    gboolean idle = count_docs_of (entry->server) == 0;

    if (idle && !entry->idle_timeout)
        entry->idle_timeout = g_timeout_add_seconds (LSP_IDLE_TIMEOUT_SEC,
                                                     server_idle_expired, entry);
    else if (!idle && entry->idle_timeout)
    {
        g_source_remove (entry->idle_timeout);
        entry->idle_timeout = 0;
    }
}


/*
 * A restarted server has forgotten every document it had open, so each one is
 * announced again as soon as it is ready to hear about them.
 */
static void
server_state_changed (LspServer *server,
                      G_GNUC_UNUSED gpointer data)
{
    GSList *l;

    if (!lsp_server_is_ready (server))
        return;

    for (l = manager.docs; l != NULL; l = l->next)
    {
        LspDoc *ldoc = (LspDoc*) l->data;

        if (lsp_doc_get_server (ldoc) == server)
            lsp_doc_open (ldoc);
    }
}


void
lsp_manager_add_listener (LspDiagnosticsNotifyFunc  func,
                          gpointer                  data)
{
    LspListener *listener = g_new0 (LspListener, 1);

    listener->func = func;
    listener->data = data;

    manager.listeners = g_slist_prepend (manager.listeners, listener);
}


void
lsp_manager_remove_listener (LspDiagnosticsNotifyFunc  func,
                             gpointer                  data)
{
    GSList *l;

    for (l = manager.listeners; l != NULL; l = l->next)
    {
        LspListener *listener = (LspListener*) l->data;

        if (listener->func == func && listener->data == data)
        {
            manager.listeners = g_slist_delete_link (manager.listeners, l);
            g_free (listener);
            return;
        }
    }
}


static void
notify_listeners (MooEdit *doc)
{
    GSList *listeners = g_slist_copy (manager.listeners);
    GSList *l;

    /* Over a copy: a listener may attach or detach while being told. */
    for (l = listeners; l != NULL; l = l->next)
    {
        LspListener *listener = (LspListener*) l->data;

        if (g_slist_find (manager.listeners, listener))
            listener->func (doc, listener->data);
    }

    g_slist_free (listeners);
}


static void
server_diagnostics (LspServer                *server,
                    const char               *uri,
                    JsonArray                *diagnostics,
                    G_GNUC_UNUSED gpointer    data)
{
    GSList *l;

    /*
     * Servers push diagnostics for files that are not open too -- gopls
     * reports every file of a package after one of them changes -- and there
     * is nothing to show them on, so those are dropped.
     */
    for (l = manager.docs; l != NULL; l = l->next)
    {
        LspDoc *ldoc = (LspDoc*) l->data;

        if (lsp_doc_get_server (ldoc) != server)
            continue;
        if (strcmp (lsp_doc_get_uri (ldoc), uri) != 0)
            continue;

        lsp_doc_set_diagnostics (ldoc, diagnostics);
        notify_listeners (lsp_doc_get_doc (ldoc));
        return;
    }
}


void
lsp_manager_refresh_diagnostics (void)
{
    GSList *l;

    for (l = manager.docs; l != NULL; l = l->next)
    {
        LspDoc *ldoc = (LspDoc*) l->data;

        lsp_doc_refresh_diagnostics (ldoc);
        notify_listeners (lsp_doc_get_doc (ldoc));
    }
}


static LspServerEntry *
get_server (LspServerConfig *config,
            const char      *root_dir)
{
    char *key = g_strdup_printf ("%s\n%s", config->id, root_dir);
    LspServerEntry *entry = (LspServerEntry*) g_hash_table_lookup (manager.servers, key);

    if (entry)
    {
        g_free (key);
        return entry;
    }

    entry = g_new0 (LspServerEntry, 1);
    entry->server = lsp_server_new (config, root_dir);

    if (!entry->server)
    {
        g_free (entry);
        g_free (key);
        return NULL;
    }

    lsp_server_set_callbacks (entry->server, server_diagnostics,
                              server_state_changed, NULL);

    g_hash_table_insert (manager.servers, key, entry);

    return entry;
}


GSList *
lsp_manager_list_servers (void)
{
    GHashTableIter iter;
    gpointer key, value;
    GSList *list = NULL;

    if (!manager.servers)
        return NULL;

    g_hash_table_iter_init (&iter, manager.servers);

    while (g_hash_table_iter_next (&iter, &key, &value))
        list = g_slist_prepend (list, ((LspServerEntry*) value)->server);

    return list;
}


/**********************************************************************/
/* Matching a document
 */

/*
 * The root of the project the file belongs to: the nearest directory at or
 * above it holding one of the entry's markers. Without markers, and when
 * nothing matches all the way up, the file's own directory is the root, which
 * is what a server falls back to anyway.
 */
static char *
find_root_dir (const char  *file_dir,
               char       **markers)
{
    char *current;

    if (!markers || !markers[0])
        return g_strdup (file_dir);

    current = g_strdup (file_dir);

    while (TRUE)
    {
        char *parent;
        guint i;

        for (i = 0; markers[i]; ++i)
        {
            char *candidate = g_build_filename (current, markers[i], nullptr);
            gboolean found = g_file_test (candidate, G_FILE_TEST_EXISTS);

            g_free (candidate);

            if (found)
                return current;
        }

        parent = g_path_get_dirname (current);

        if (strcmp (parent, current) == 0)
        {
            g_free (parent);
            break;
        }

        g_free (current);
        current = parent;
    }

    g_free (current);

    return g_strdup (file_dir);
}


static LspServerConfig *
find_config (MooEdit *doc)
{
    GSList *l;

    for (l = manager.configs; l != NULL; l = l->next)
    {
        LspServerConfig *config = (LspServerConfig*) l->data;
        MooEditFilter *filter;
        gboolean matched;

        if (!config->enabled)
            continue;

        filter = _moo_edit_filter_new (config->filter, MOO_EDIT_FILTER_CONFIG);

        if (!filter)
            continue;

        matched = _moo_edit_filter_match (filter, doc);
        _moo_edit_filter_free (filter);

        if (matched)
            return config;
    }

    return NULL;
}


LspDoc *
lsp_manager_lookup_doc (MooEdit *doc)
{
    GSList *l;

    for (l = manager.docs; l != NULL; l = l->next)
        if (lsp_doc_get_doc ((LspDoc*) l->data) == doc)
            return (LspDoc*) l->data;

    return NULL;
}


void
lsp_manager_add_doc (MooEdit *doc)
{
    LspServerConfig *config;
    LspServerEntry *entry;
    LspDoc *ldoc;
    GFile *file;
    char *path, *dir, *root, *program;

    g_return_if_fail (MOO_IS_EDIT (doc));

    if (!manager.initialized)
        return;

    if (lsp_manager_lookup_doc (doc))
        return;

    /* A document with no file has no uri, and a server has nothing to say
       about text that is not anywhere yet. */
    file = moo_edit_get_file (doc);
    path = file ? g_file_get_path (file) : NULL;
    moo_file_free (file);

    if (!path)
        return;

    config = find_config (doc);

    if (!config)
    {
        g_free (path);
        return;
    }

    /* Not installed: the shipped configuration lists more servers than anyone
       has, and saying so on every file that opens would be noise. */
    program = lsp_config_find_program (config);

    if (!program)
    {
        g_free (path);
        return;
    }

    g_free (program);

    dir = g_path_get_dirname (path);
    root = find_root_dir (dir, config->root_markers);

    entry = get_server (config, root);

    if (entry && (ldoc = lsp_doc_new (doc, entry->server)) != NULL)
    {
        manager.docs = g_slist_prepend (manager.docs, ldoc);
        update_idle_timeout (entry);
    }

    g_free (root);
    g_free (dir);
    g_free (path);
}


void
lsp_manager_remove_doc (MooEdit *doc)
{
    LspDoc *ldoc = lsp_manager_lookup_doc (doc);
    LspServer *server;
    GHashTableIter iter;
    gpointer key, value;

    if (!ldoc)
        return;

    server = lsp_doc_get_server (ldoc);

    manager.docs = g_slist_remove (manager.docs, ldoc);
    lsp_doc_free (ldoc);

    g_hash_table_iter_init (&iter, manager.servers);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        LspServerEntry *entry = (LspServerEntry*) value;

        if (entry->server == server)
        {
            update_idle_timeout (entry);
            break;
        }
    }
}


/**********************************************************************/
/* The manager itself
 */

void
lsp_manager_init (void)
{
    if (manager.initialized)
        return;

    manager.servers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, server_entry_free);
    manager.configs = lsp_config_load ();
    manager.initialized = TRUE;
}


gboolean
lsp_manager_is_running (void)
{
    return manager.initialized;
}


void
lsp_manager_shutdown (void)
{
    if (!manager.initialized)
        return;

    manager.initialized = FALSE;

    /*
     * Blocking writes from here on. Closing the documents below sends a
     * didClose for each, and those go out before the main loop that would
     * have completed an asynchronous write is gone.
     */
    {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init (&iter, manager.servers);

        while (g_hash_table_iter_next (&iter, &key, &value))
            lsp_server_set_sync_writes (((LspServerEntry*) value)->server);
    }

    g_slist_free_full (manager.docs, (GDestroyNotify) lsp_doc_free);
    manager.docs = NULL;

    /* Every server is asked to exit here; anything still alive is killed by
       the timeout inside lsp_server_shutdown(). */
    g_hash_table_destroy (manager.servers);
    manager.servers = NULL;

    lsp_config_list_free (manager.configs);
    manager.configs = NULL;
}


void
lsp_manager_reload (void)
{
    GSList *docs = NULL, *l;

    if (!manager.initialized)
        return;

    /* Remember the documents, drop everything, then attach them again against
       the new configuration. */
    for (l = manager.docs; l != NULL; l = l->next)
        docs = g_slist_prepend (docs, lsp_doc_get_doc ((LspDoc*) l->data));

    lsp_manager_shutdown ();
    lsp_manager_init ();

    for (l = docs; l != NULL; l = l->next)
        lsp_manager_add_doc ((MooEdit*) l->data);

    g_slist_free (docs);
}
