/*
 *   plugins/lsp/lsp-server.cpp
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

#include "plugins/lsp/lsp-server.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooutils/mooi18n.h"
#include "mooutils/mooprefs.h"

#include <stdarg.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* A server that dies sooner than this never really started. */
#define LSP_QUICK_EXIT_USEC (2 * G_TIME_SPAN_SECOND)
#define LSP_MAX_QUICK_EXITS 3

/* How long to wait for the reply to initialize, and then for the process. */
#define LSP_INITIALIZE_TIMEOUT_SEC 30
#define LSP_EXIT_TIMEOUT_SEC 2

typedef struct {
    gboolean            is_call;
    char               *method;
    JsonObject         *params;
    LspClientReplyFunc  callback;
    gpointer            data;
    GDestroyNotify      destroy;
} LspQueuedMessage;

struct LspServer {
    int              ref_count;

    char            *id;
    char           **argv;
    char           **env;
    char            *init_options;
    char            *root_dir;
    char            *root_uri;

    LspClient       *client;
    LspServerState   state;
    char            *error_message;

    JsonObject      *capabilities;
    LspPositionEncoding encoding;
    LspSyncKind      sync_kind;
    gboolean         wants_did_save_text;

    GHashTable      *docs;              /* uri -> NULL, the set of open ones */
    GQueue          *queued;            /* LspQueuedMessage*, sent once ready */

    gint64           spawn_time;
    guint            quick_exits;
    gboolean         shutting_down;
    guint            init_timeout;
    guint            exit_timeout;

    LspServerDiagnosticsFunc on_diagnostics;
    LspServerStateFunc       on_state;
    gpointer                 cb_data;
};


static void     start_process       (LspServer  *server);
static void     send_initialize     (LspServer  *server);
static void     flush_queue         (LspServer  *server);


/**********************************************************************/
/* Bookkeeping
 */

static void
queued_message_free (LspQueuedMessage *message)
{
    if (!message)
        return;

    if (message->params)
        json_object_unref (message->params);
    if (message->destroy)
        message->destroy (message->data);

    g_free (message->method);
    g_free (message);
}


static void
clear_queue (LspServer *server)
{
    gpointer item;

    while ((item = g_queue_pop_head (server->queued)) != NULL)
        queued_message_free ((LspQueuedMessage*) item);
}


static void
set_state (LspServer      *server,
           LspServerState  state)
{
    if (server->state == state)
        return;

    server->state = state;

    if (server->on_state)
        server->on_state (server, server->cb_data);
}


static void
set_failed (LspServer  *server,
            const char *format,
            ...)
{
    va_list args;

    va_start (args, format);
    g_free (server->error_message);
    server->error_message = g_strdup_vprintf (format, args);
    va_end (args);

    clear_queue (server);
    g_hash_table_remove_all (server->docs);

    set_state (server, LSP_SERVER_FAILED);
}


LspServer *
lsp_server_ref (LspServer *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    server->ref_count++;
    return server;
}


static void
drop_client (LspServer *server)
{
    if (!server->client)
        return;

    lsp_client_disconnect (server->client);
    lsp_client_unref (server->client);
    server->client = NULL;
}


void
lsp_server_unref (LspServer *server)
{
    g_return_if_fail (server != NULL);

    if (--server->ref_count > 0)
        return;

    if (server->init_timeout)
        g_source_remove (server->init_timeout);
    if (server->exit_timeout)
        g_source_remove (server->exit_timeout);

    drop_client (server);
    clear_queue (server);
    g_queue_free (server->queued);
    g_hash_table_destroy (server->docs);

    if (server->capabilities)
        json_object_unref (server->capabilities);

    g_free (server->id);
    g_strfreev (server->argv);
    g_strfreev (server->env);
    g_free (server->init_options);
    g_free (server->root_dir);
    g_free (server->root_uri);
    g_free (server->error_message);
    g_free (server);
}


const char *
lsp_server_get_id (LspServer *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    return server->id;
}


const char *
lsp_server_get_root (LspServer *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    return server->root_dir;
}


LspServerState
lsp_server_get_state (LspServer *server)
{
    g_return_val_if_fail (server != NULL, LSP_SERVER_FAILED);
    return server->state;
}


gboolean
lsp_server_is_ready (LspServer *server)
{
    return server != NULL && server->state == LSP_SERVER_READY &&
           lsp_client_is_running (server->client);
}


const char *
lsp_server_get_error (LspServer *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    return server->error_message;
}


LspPositionEncoding
lsp_server_get_position_encoding (LspServer *server)
{
    g_return_val_if_fail (server != NULL, LSP_POSITION_ENCODING_UTF16);
    return server->encoding;
}


LspSyncKind
lsp_server_get_sync_kind (LspServer *server)
{
    g_return_val_if_fail (server != NULL, LSP_SYNC_NONE);
    return server->sync_kind;
}


JsonObject *
lsp_server_get_capabilities (LspServer *server)
{
    g_return_val_if_fail (server != NULL, NULL);
    return server->capabilities;
}


gboolean
lsp_server_has_provider (LspServer  *server,
                         const char *name)
{
    g_return_val_if_fail (server != NULL, FALSE);
    return lsp_json_get_provider (server->capabilities, name);
}


gboolean
lsp_server_has_doc (LspServer  *server,
                    const char *uri)
{
    g_return_val_if_fail (server != NULL, FALSE);
    return uri != NULL && g_hash_table_contains (server->docs, uri);
}


guint
lsp_server_count_docs (LspServer *server)
{
    g_return_val_if_fail (server != NULL, 0);
    return g_hash_table_size (server->docs);
}


void
lsp_server_set_callbacks (LspServer                *server,
                          LspServerDiagnosticsFunc  on_diagnostics,
                          LspServerStateFunc        on_state,
                          gpointer                  data)
{
    g_return_if_fail (server != NULL);

    server->on_diagnostics = on_diagnostics;
    server->on_state = on_state;
    server->cb_data = data;
}


/**********************************************************************/
/* Talking
 */

static void
queue_message (LspServer          *server,
               gboolean            is_call,
               const char         *method,
               JsonObject         *params,
               LspClientReplyFunc  callback,
               gpointer            data,
               GDestroyNotify      destroy)
{
    LspQueuedMessage *message = g_new0 (LspQueuedMessage, 1);

    message->is_call = is_call;
    message->method = g_strdup (method);
    message->params = params;
    message->callback = callback;
    message->data = data;
    message->destroy = destroy;

    g_queue_push_tail (server->queued, message);
}


static void
flush_queue (LspServer *server)
{
    gpointer item;

    while ((item = g_queue_pop_head (server->queued)) != NULL)
    {
        LspQueuedMessage *message = (LspQueuedMessage*) item;

        if (message->is_call)
            lsp_client_call (server->client, message->method, message->params,
                             message->callback, message->data, message->destroy);
        else
            lsp_client_notify (server->client, message->method, message->params);

        /* Ownership of params and of the callback data moved to the client. */
        message->params = NULL;
        message->destroy = NULL;
        queued_message_free (message);
    }
}


static void
notify (LspServer  *server,
        const char *method,
        JsonObject *params)
{
    if (server->state == LSP_SERVER_FAILED)
    {
        json_object_unref (params);
        return;
    }

    if (server->state == LSP_SERVER_STARTING)
        queue_message (server, FALSE, method, params, NULL, NULL, NULL);
    else
        lsp_client_notify (server->client, method, params);
}


gint64
lsp_server_call (LspServer          *server,
                 const char         *method,
                 JsonObject         *params,
                 LspClientReplyFunc  callback,
                 gpointer            data,
                 GDestroyNotify      destroy)
{
    g_return_val_if_fail (server != NULL, 0);
    g_return_val_if_fail (method != NULL, 0);

    if (server->state == LSP_SERVER_FAILED)
    {
        if (params)
            json_object_unref (params);
        if (destroy)
            destroy (data);
        return 0;
    }

    if (server->state == LSP_SERVER_STARTING)
    {
        queue_message (server, TRUE, method, params, callback, data, destroy);
        return 0;
    }

    return lsp_client_call (server->client, method, params, callback, data, destroy);
}


void
lsp_server_cancel (LspServer *server,
                   gint64     id)
{
    g_return_if_fail (server != NULL);

    if (id != 0 && server->client)
        lsp_client_cancel (server->client, id);
}


/**********************************************************************/
/* Document synchronization
 */

static JsonObject *
versioned_text_document (const char *uri,
                         int         version)
{
    JsonObject *object = lsp_json_text_document (uri);

    lsp_json_set_int (object, "version", version);

    return object;
}


void
lsp_server_did_open (LspServer  *server,
                     const char *uri,
                     const char *language_id,
                     int         version,
                     const char *text)
{
    JsonObject *params, *item;

    g_return_if_fail (server != NULL);
    g_return_if_fail (uri != NULL && text != NULL);

    if (server->sync_kind == LSP_SYNC_NONE && server->state == LSP_SERVER_READY)
        return;

    if (g_hash_table_contains (server->docs, uri))
        return;

    g_hash_table_add (server->docs, g_strdup (uri));

    item = versioned_text_document (uri, version);
    lsp_json_set_string (item, "languageId", language_id ? language_id : "plaintext");
    lsp_json_set_string (item, "text", text);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument", item);

    notify (server, "textDocument/didOpen", params);
}


void
lsp_server_did_change (LspServer  *server,
                       const char *uri,
                       int         version,
                       const char *text)
{
    JsonObject *params, *change;
    JsonArray *changes;

    g_return_if_fail (server != NULL);
    g_return_if_fail (uri != NULL && text != NULL);

    if (!g_hash_table_contains (server->docs, uri))
        return;

    if (server->sync_kind == LSP_SYNC_NONE)
        return;

    /*
     * Whole document every time. A change with no range is what the protocol
     * calls a full replacement, and every server accepts it whichever sync
     * kind it asked for.
     */
    change = json_object_new ();
    lsp_json_set_string (change, "text", text);

    changes = json_array_new ();
    json_array_add_object_element (changes, change);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument", versioned_text_document (uri, version));
    lsp_json_set_array (params, "contentChanges", changes);

    notify (server, "textDocument/didChange", params);
}


void
lsp_server_did_save (LspServer  *server,
                     const char *uri,
                     const char *text)
{
    JsonObject *params;

    g_return_if_fail (server != NULL);
    g_return_if_fail (uri != NULL);

    if (!g_hash_table_contains (server->docs, uri))
        return;

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument", lsp_json_text_document (uri));

    if (server->wants_did_save_text && text)
        lsp_json_set_string (params, "text", text);

    notify (server, "textDocument/didSave", params);
}


void
lsp_server_did_close (LspServer  *server,
                      const char *uri)
{
    JsonObject *params;

    g_return_if_fail (server != NULL);
    g_return_if_fail (uri != NULL);

    if (!g_hash_table_remove (server->docs, uri))
        return;

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument", lsp_json_text_document (uri));

    notify (server, "textDocument/didClose", params);
}


/**********************************************************************/
/* What the server sends us
 */

static void
handle_notification (const char *method,
                     JsonObject *params,
                     gpointer    data)
{
    LspServer *server = (LspServer*) data;

    if (strcmp (method, "textDocument/publishDiagnostics") == 0)
    {
        const char *uri = lsp_json_get_string (params, "uri");
        JsonArray *diagnostics = lsp_json_get_array (params, "diagnostics");

        if (uri && server->on_diagnostics)
            server->on_diagnostics (server, uri, diagnostics, server->cb_data);
    }
    else if (strcmp (method, "window/logMessage") == 0 ||
             strcmp (method, "window/showMessage") == 0)
    {
        const char *text = lsp_json_get_string (params, "message");

        if (text && _moo_lsp_debug ())
            g_printerr ("lsp: %s: %s\n", server->id, text);
    }
    else
    {
        /* $/progress, telemetry/event and whatever else; nothing to do. */
    }
}


static void
handle_request (JsonNode   *id,
                const char *method,
                JsonObject *params,
                gpointer    data)
{
    LspServer *server = (LspServer*) data;

    /*
     * Answering these matters: a server that asks for its configuration and
     * never hears back sits waiting instead of serving. Nothing is configured
     * per server beyond what initializationOptions carried, so the answer is
     * always "no opinion".
     */
    if (strcmp (method, "workspace/configuration") == 0)
    {
        JsonArray *items = lsp_json_get_array (params, "items");
        JsonArray *result = json_array_new ();
        guint i, n = items ? json_array_get_length (items) : 0;

        for (i = 0; i < n; ++i)
            json_array_add_null_element (result);

        {
            JsonNode *node = json_node_new (JSON_NODE_ARRAY);
            json_node_take_array (node, result);
            lsp_client_reply (server->client, id, node);
        }
    }
    else if (strcmp (method, "workspace/workspaceFolders") == 0)
    {
        JsonArray *folders = json_array_new ();
        JsonObject *folder = json_object_new ();
        JsonNode *node;

        lsp_json_set_string (folder, "uri", server->root_uri);
        lsp_json_set_string (folder, "name", server->id);
        json_array_add_object_element (folders, folder);

        node = json_node_new (JSON_NODE_ARRAY);
        json_node_take_array (node, folders);
        lsp_client_reply (server->client, id, node);
    }
    else if (strcmp (method, "client/registerCapability") == 0 ||
             strcmp (method, "client/unregisterCapability") == 0 ||
             strcmp (method, "window/workDoneProgress/create") == 0 ||
             strcmp (method, "workspace/semanticTokens/refresh") == 0 ||
             strcmp (method, "workspace/codeLens/refresh") == 0 ||
             strcmp (method, "workspace/diagnostic/refresh") == 0)
    {
        /* Accepted, with nothing to say back. */
        lsp_client_reply (server->client, id, json_node_new (JSON_NODE_NULL));
    }
    else if (strcmp (method, "window/showMessageRequest") == 0)
    {
        const char *text = lsp_json_get_string (params, "message");

        if (text && _moo_lsp_debug ())
            g_printerr ("lsp: %s: %s\n", server->id, text);

        /* No action was picked. */
        lsp_client_reply (server->client, id, json_node_new (JSON_NODE_NULL));
    }
    else
    {
        lsp_client_reply_error (server->client, id, -32601, "method not supported by medit");
    }
}


/**********************************************************************/
/* Starting and stopping
 */

static void
read_capabilities (LspServer  *server,
                   JsonObject *result)
{
    JsonObject *capabilities = lsp_json_get_object (result, "capabilities");
    JsonNode *sync;
    const char *encoding;

    if (server->capabilities)
        json_object_unref (server->capabilities);

    server->capabilities = capabilities ? json_object_ref (capabilities) : NULL;

    /*
     * positionEncoding is the LSP 3.17 answer to the offer made in
     * general.positionEncodings. A server that predates it says nothing, and
     * silence means UTF-16, which is what the protocol always meant.
     */
    encoding = lsp_json_get_string (server->capabilities, "positionEncoding");

    if (encoding && strcmp (encoding, "utf-8") == 0)
        server->encoding = LSP_POSITION_ENCODING_UTF8;
    else if (encoding && strcmp (encoding, "utf-32") == 0)
        server->encoding = LSP_POSITION_ENCODING_UTF32;
    else
        server->encoding = LSP_POSITION_ENCODING_UTF16;

    /*
     * textDocumentSync is either a number or an options object, depending on
     * how old the server is.
     */
    server->sync_kind = LSP_SYNC_FULL;
    server->wants_did_save_text = FALSE;
    sync = lsp_json_get_node (server->capabilities, "textDocumentSync");

    if (sync && JSON_NODE_HOLDS_VALUE (sync))
    {
        server->sync_kind = (LspSyncKind) lsp_json_get_int (server->capabilities,
                                                            "textDocumentSync",
                                                            LSP_SYNC_FULL);
    }
    else if (sync && JSON_NODE_HOLDS_OBJECT (sync))
    {
        JsonObject *options = json_node_get_object (sync);

        server->sync_kind = (LspSyncKind) lsp_json_get_int (options, "change", LSP_SYNC_FULL);

        if (lsp_json_get_provider (options, "save"))
            server->wants_did_save_text =
                lsp_json_lookup_bool (options, "save/includeText", FALSE);
    }

    /* Incremental is not implemented; a full change is legal either way. */
    if (server->sync_kind == LSP_SYNC_INCREMENTAL)
        server->sync_kind = LSP_SYNC_FULL;
}


static void
initialize_reply (JsonNode   *result,
                  JsonObject *error,
                  gpointer    data)
{
    LspServer *server = (LspServer*) data;

    if (server->init_timeout)
    {
        g_source_remove (server->init_timeout);
        server->init_timeout = 0;
    }

    if (error)
    {
        const char *message = lsp_json_get_string (error, "message");

        set_failed (server, _("%s failed to start: %s"), server->id,
                    message ? message : _("unknown error"));
        return;
    }

    if (!result || !JSON_NODE_HOLDS_OBJECT (result))
    {
        set_failed (server, _("%s answered the initialize request with no capabilities"),
                    server->id);
        return;
    }

    read_capabilities (server, json_node_get_object (result));

    lsp_client_notify (server->client, "initialized", json_object_new ());

    set_state (server, LSP_SERVER_READY);

    flush_queue (server);
}


static gboolean
initialize_timed_out (gpointer data)
{
    LspServer *server = (LspServer*) data;

    server->init_timeout = 0;

    set_failed (server, _("%s did not answer the initialize request in %d seconds"),
                server->id, LSP_INITIALIZE_TIMEOUT_SEC);

    drop_client (server);

    return G_SOURCE_REMOVE;
}


/*
 * What medit can do with what a server sends back. Each commit that implements
 * a feature adds its entry here; a capability medit claims but does not use
 * makes servers do work for nothing.
 */
static JsonObject *
client_capabilities (void)
{
    JsonObject *capabilities = json_object_new ();
    JsonObject *text_document = json_object_new ();
    JsonObject *synchronization = json_object_new ();
    JsonObject *publish_diagnostics = json_object_new ();

    lsp_json_set_bool (synchronization, "dynamicRegistration", FALSE);
    lsp_json_set_bool (synchronization, "willSave", FALSE);
    lsp_json_set_bool (synchronization, "willSaveWaitUntil", FALSE);
    lsp_json_set_bool (synchronization, "didSave", TRUE);

    /*
     * relatedInformation and codeDescription would each need somewhere to show
     * a second location or a url, and there is nowhere yet; saying no keeps
     * servers from computing them.
     */
    lsp_json_set_bool (publish_diagnostics, "relatedInformation", FALSE);
    lsp_json_set_bool (publish_diagnostics, "versionSupport", FALSE);
    lsp_json_set_bool (publish_diagnostics, "codeDescriptionSupport", FALSE);

    {
        JsonObject *document_symbol = json_object_new ();

        lsp_json_set_bool (document_symbol, "dynamicRegistration", FALSE);
        lsp_json_set_bool (document_symbol, "hierarchicalDocumentSymbolSupport", TRUE);

        lsp_json_set_object (text_document, "documentSymbol", document_symbol);
    }

    lsp_json_set_object (text_document, "synchronization", synchronization);
    lsp_json_set_object (text_document, "publishDiagnostics", publish_diagnostics);
    lsp_json_set_object (capabilities, "textDocument", text_document);

    return capabilities;
}


static void
send_initialize (LspServer *server)
{
    JsonObject *params = json_object_new ();
    JsonObject *client_info = json_object_new ();
    JsonObject *general = json_object_new ();
    JsonArray *folders = json_array_new ();
    JsonObject *folder = json_object_new ();
    static const char *encodings[] = { "utf-8", "utf-16", NULL };

    lsp_json_set_string (client_info, "name", "medit");
    lsp_json_set_string (client_info, "version", MOO_VERSION);

    lsp_json_set_array (general, "positionEncodings",
                        lsp_json_string_array (encodings));

    lsp_json_set_string (folder, "uri", server->root_uri);
    lsp_json_set_string (folder, "name", server->id);
    json_array_add_object_element (folders, folder);

#ifdef HAVE_UNISTD_H
    lsp_json_set_int (params, "processId", getpid ());
#else
    lsp_json_set_null (params, "processId");
#endif

    lsp_json_set_object (params, "clientInfo", client_info);
    lsp_json_set_string (params, "rootPath", server->root_dir);
    lsp_json_set_string (params, "rootUri", server->root_uri);
    lsp_json_set_array (params, "workspaceFolders", folders);
    lsp_json_set_object (params, "capabilities", client_capabilities ());
    lsp_json_set_object (params, "general", general);

    if (server->init_options && server->init_options[0])
    {
        GError *error = NULL;
        JsonNode *node = lsp_json_parse (server->init_options, -1, &error);

        if (node)
        {
            lsp_json_set_node (params, "initializationOptions", node);
        }
        else
        {
            g_warning ("%s: server '%s' has unparsable initialization options: %s",
                       G_STRFUNC, server->id, error ? error->message : "");
            g_clear_error (&error);
        }
    }

    server->init_timeout = g_timeout_add_seconds (LSP_INITIALIZE_TIMEOUT_SEC,
                                                  initialize_timed_out, server);

    lsp_client_call (server->client, "initialize", params,
                     initialize_reply, server, NULL);
}


static void
client_exited (int      status,
               gpointer data)
{
    LspServer *server = (LspServer*) data;
    gint64 lifetime = g_get_monotonic_time () - server->spawn_time;

    if (server->shutting_down)
        return;

    if (lifetime < LSP_QUICK_EXIT_USEC)
        server->quick_exits++;
    else
        server->quick_exits = 0;

    if (server->quick_exits >= LSP_MAX_QUICK_EXITS)
    {
        set_failed (server,
                    _("%s exited immediately %d times in a row; check the command "
                      "in the LSP configuration file"),
                    server->id, LSP_MAX_QUICK_EXITS);
        return;
    }

    if (_moo_lsp_debug ())
        g_printerr ("lsp: %s exited with status %d, restarting\n", server->id, status);

    /*
     * A restarted server knows nothing about the documents it had open. The
     * set is cleared here and the owner opens them again when it sees the
     * state go back to starting.
     */
    g_hash_table_remove_all (server->docs);
    clear_queue (server);

    if (server->capabilities)
    {
        json_object_unref (server->capabilities);
        server->capabilities = NULL;
    }

    set_state (server, LSP_SERVER_STARTING);

    start_process (server);
}


static void
start_process (LspServer *server)
{
    GError *error = NULL;

    drop_client (server);

    server->spawn_time = g_get_monotonic_time ();
    server->client = lsp_client_new (server->argv, server->root_dir,
                                     server->env, &error);

    if (!server->client)
    {
        set_failed (server, _("could not run %s: %s"), server->argv[0],
                    error ? error->message : _("unknown error"));
        g_clear_error (&error);
        return;
    }

    lsp_client_set_logging (server->client, _moo_lsp_debug ());
    lsp_client_set_callbacks (server->client, handle_notification,
                              handle_request, client_exited, server);

    send_initialize (server);
}


LspServer *
lsp_server_new (LspServerConfig *config,
                const char      *root_dir)
{
    LspServer *server;
    GFile *file;

    g_return_val_if_fail (config != NULL, NULL);
    g_return_val_if_fail (root_dir != NULL, NULL);

    server = g_new0 (LspServer, 1);
    server->ref_count = 1;
    server->id = g_strdup (config->id);
    server->argv = g_strdupv (config->argv);
    server->env = config->env ? g_strdupv (config->env) : NULL;
    server->init_options = g_strdup (config->init_options);
    server->root_dir = g_strdup (root_dir);
    server->state = LSP_SERVER_STARTING;
    server->encoding = LSP_POSITION_ENCODING_UTF16;
    server->sync_kind = LSP_SYNC_FULL;
    server->docs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    server->queued = g_queue_new ();

    file = g_file_new_for_path (root_dir);
    server->root_uri = g_file_get_uri (file);
    g_object_unref (file);

    start_process (server);

    return server;
}


static gboolean
exit_timed_out (gpointer data)
{
    LspServer *server = (LspServer*) data;

    server->exit_timeout = 0;

    if (server->client && lsp_client_is_running (server->client))
        lsp_client_force_exit (server->client);

    drop_client (server);
    lsp_server_unref (server);

    return G_SOURCE_REMOVE;
}


void
lsp_server_set_sync_writes (LspServer *server)
{
    g_return_if_fail (server != NULL);

    if (server->client)
        lsp_client_set_sync_writes (server->client, TRUE);
}


void
lsp_server_shutdown (LspServer *server)
{
    g_return_if_fail (server != NULL);

    if (server->shutting_down)
        return;

    server->shutting_down = TRUE;

    if (server->init_timeout)
    {
        g_source_remove (server->init_timeout);
        server->init_timeout = 0;
    }

    clear_queue (server);
    g_hash_table_remove_all (server->docs);

    if (!server->client || !lsp_client_is_running (server->client))
    {
        drop_client (server);
        return;
    }

    /*
     * The protocol wants the exit notification sent only after the shutdown
     * request has been answered. Waiting for that answer is not possible on
     * the way out of the application -- the main loop that would deliver it is
     * already going away -- so both go out together and are then written
     * synchronously. Every server tolerates this, and it beats the
     * alternative, which is letting it work out on its own that the pipe it
     * was reading from has closed.
     */
    if (server->state == LSP_SERVER_READY)
        lsp_client_call (server->client, "shutdown", NULL, NULL, NULL, NULL);

    lsp_client_notify (server->client, "exit", NULL);
    lsp_client_close_stdin (server->client);

    /*
     * Kills anything that ignored all of that. The reference is held by the
     * timeout; when the main loop is already gone the timeout never fires and
     * the server is leaked, which at that point costs nothing.
     */
    lsp_server_ref (server);
    server->exit_timeout = g_timeout_add_seconds (LSP_EXIT_TIMEOUT_SEC,
                                                  exit_timed_out, server);
}
