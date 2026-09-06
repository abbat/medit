/*
 *   plugins/lsp/lsp-client.cpp
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

#include "plugins/lsp/lsp-client.h"

#include <gio/gio.h>
#include <string.h>

#define LSP_READ_CHUNK 8192

/*
 * A message larger than this is taken as a desynchronised stream rather than a
 * real reply: the header is then never going to arrive and the buffer would
 * grow until the machine gives up.
 */
#define LSP_MAX_MESSAGE (64 * 1024 * 1024)

/* JSON-RPC, and the two codes the protocol reserves that are used here. */
#define LSP_ERROR_INTERNAL     (-32603)
#define LSP_ERROR_NOT_RUNNING  (-32099)

typedef struct {
    LspClientReplyFunc  callback;
    gpointer            data;
    GDestroyNotify      destroy;
} LspPendingCall;

struct LspClient {
    int              ref_count;

    char            *name;              /* argv[0], for messages only */
    GSubprocess     *process;
    GOutputStream   *stdin_stream;
    GInputStream    *stdout_stream;
    GInputStream    *stderr_stream;
    GCancellable    *cancellable;

    GByteArray      *in_buf;
    guint8           read_chunk[LSP_READ_CHUNK];
    guint8           err_chunk[LSP_READ_CHUNK];
    gboolean         reading;
    gboolean         reading_stderr;

    GQueue          *out_queue;         /* GBytes*, oldest first */
    GBytes          *writing_bytes;     /* the buffer the current write reads from */
    gboolean         writing;
    gboolean         sync_writes;

    GHashTable      *pending;           /* gint64* -> LspPendingCall* */
    gint64           next_id;

    gboolean         running;
    gboolean         disconnected;
    gboolean         log;

    LspClientNotifyFunc  on_notify;
    LspClientRequestFunc on_request;
    LspClientExitFunc    on_exit;
    gpointer             cb_data;
};


static void     start_read          (LspClient  *client);
static void     start_read_stderr   (LspClient  *client);
static void     start_write         (LspClient  *client);
static void     process_in_buf      (LspClient  *client);


/**********************************************************************/
/* Bookkeeping
 */

static void
pending_call_free (gpointer data)
{
    LspPendingCall *call = (LspPendingCall*) data;

    if (call->destroy)
        call->destroy (call->data);

    g_free (call);
}


static JsonObject *
make_error_object (int         code,
                   const char *message)
{
    JsonObject *object = json_object_new ();

    lsp_json_set_int (object, "code", code);
    lsp_json_set_string (object, "message", message);

    return object;
}


/*
 * Invokes every pending callback with an error and empties the table. Called
 * when the process is gone and when the owner disconnects, so that no caller
 * is left waiting for a reply that can never come.
 */
static void
fail_pending_calls (LspClient  *client,
                    const char *message)
{
    GHashTable *pending = client->pending;
    GHashTableIter iter;
    gpointer key, value;
    GSList *calls = NULL, *l;

    if (!pending || g_hash_table_size (pending) == 0)
        return;

    /*
     * A callback may start a new call, so the table cannot be iterated while
     * the callbacks run. Take everything out first.
     */
    g_hash_table_iter_init (&iter, pending);

    while (g_hash_table_iter_next (&iter, &key, &value))
        calls = g_slist_prepend (calls, value);

    g_hash_table_steal_all (pending);

    for (l = calls; l != NULL; l = l->next)
    {
        LspPendingCall *call = (LspPendingCall*) l->data;
        JsonObject *error = make_error_object (LSP_ERROR_NOT_RUNNING, message);

        if (call->callback)
            call->callback (NULL, error, call->data);

        json_object_unref (error);
        pending_call_free (call);
    }

    g_slist_free (calls);
}


LspClient *
lsp_client_ref (LspClient *client)
{
    g_return_val_if_fail (client != NULL, NULL);
    client->ref_count++;
    return client;
}


void
lsp_client_unref (LspClient *client)
{
    g_return_if_fail (client != NULL);

    if (--client->ref_count > 0)
        return;

    fail_pending_calls (client, "the client is gone");

    if (client->pending)
        g_hash_table_destroy (client->pending);

    if (client->writing_bytes)
        g_bytes_unref (client->writing_bytes);

    if (client->out_queue)
    {
        gpointer bytes;

        while ((bytes = g_queue_pop_head (client->out_queue)) != NULL)
            g_bytes_unref ((GBytes*) bytes);

        g_queue_free (client->out_queue);
    }

    if (client->in_buf)
        g_byte_array_free (client->in_buf, TRUE);

    if (client->cancellable)
        g_object_unref (client->cancellable);
    if (client->stdin_stream)
        g_object_unref (client->stdin_stream);
    if (client->stdout_stream)
        g_object_unref (client->stdout_stream);
    if (client->stderr_stream)
        g_object_unref (client->stderr_stream);
    if (client->process)
        g_object_unref (client->process);

    g_free (client->name);
    g_free (client);
}


/**********************************************************************/
/* The process
 */

static void
process_exited (GObject      *source,
                GAsyncResult *result,
                gpointer      data)
{
    LspClient *client = (LspClient*) data;
    GSubprocess *process = G_SUBPROCESS (source);
    int status = 0;

    /*
     * g_subprocess_get_status() hands back the raw wait status, where an exit
     * code of 3 reads as 768. Report the code, or minus the signal.
     */
    if (g_subprocess_wait_finish (process, result, NULL))
    {
        if (g_subprocess_get_if_exited (process))
            status = g_subprocess_get_exit_status (process);
        else if (g_subprocess_get_if_signaled (process))
            status = -g_subprocess_get_term_sig (process);
    }

    client->running = FALSE;

    fail_pending_calls (client, "the language server exited");

    if (!client->disconnected && client->on_exit)
        client->on_exit (status, client->cb_data);

    lsp_client_unref (client);
}


LspClient *
lsp_client_new (char       **argv,
                const char  *working_dir,
                char       **env,
                GError     **error)
{
    LspClient *client;
    GSubprocessLauncher *launcher;
    GSubprocess *process;
    guint i;

    g_return_val_if_fail (argv != NULL && argv[0] != NULL, NULL);

    launcher = g_subprocess_launcher_new ((GSubprocessFlags)
                                          (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                           G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                           G_SUBPROCESS_FLAGS_STDERR_PIPE));

    if (working_dir)
        g_subprocess_launcher_set_cwd (launcher, working_dir);

    for (i = 0; env && env[i]; ++i)
    {
        char *eq = strchr (env[i], '=');

        if (!eq || eq == env[i])
        {
            g_warning ("%s: ignoring malformed environment entry '%s'",
                       G_STRFUNC, env[i]);
            continue;
        }

        {
            char *name = g_strndup (env[i], eq - env[i]);
            g_subprocess_launcher_setenv (launcher, name, eq + 1, TRUE);
            g_free (name);
        }
    }

    process = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) argv, error);
    g_object_unref (launcher);

    if (!process)
        return NULL;

    client = g_new0 (LspClient, 1);
    client->ref_count = 1;
    client->name = g_strdup (argv[0]);
    client->process = process;
    client->stdin_stream = G_OUTPUT_STREAM (g_object_ref (g_subprocess_get_stdin_pipe (process)));
    client->stdout_stream = G_INPUT_STREAM (g_object_ref (g_subprocess_get_stdout_pipe (process)));
    client->stderr_stream = G_INPUT_STREAM (g_object_ref (g_subprocess_get_stderr_pipe (process)));
    client->cancellable = g_cancellable_new ();
    client->in_buf = g_byte_array_new ();
    client->out_queue = g_queue_new ();
    client->pending = g_hash_table_new_full (g_int64_hash, g_int64_equal,
                                             g_free, pending_call_free);
    client->next_id = 1;
    client->running = TRUE;

    lsp_client_ref (client);
    g_subprocess_wait_async (process, client->cancellable, process_exited, client);

    start_read (client);
    start_read_stderr (client);

    return client;
}


void
lsp_client_set_callbacks (LspClient            *client,
                          LspClientNotifyFunc   on_notify,
                          LspClientRequestFunc  on_request,
                          LspClientExitFunc     on_exit,
                          gpointer              data)
{
    g_return_if_fail (client != NULL);

    client->on_notify = on_notify;
    client->on_request = on_request;
    client->on_exit = on_exit;
    client->cb_data = data;
}


void
lsp_client_set_logging (LspClient *client,
                        gboolean   log)
{
    g_return_if_fail (client != NULL);
    client->log = log != FALSE;
}


gboolean
lsp_client_is_running (LspClient *client)
{
    return client != NULL && client->running && !client->disconnected;
}


void
lsp_client_close_stdin (LspClient *client)
{
    g_return_if_fail (client != NULL);

    if (client->stdin_stream && !g_output_stream_is_closed (client->stdin_stream))
        g_output_stream_close (client->stdin_stream, NULL, NULL);
}


void
lsp_client_force_exit (LspClient *client)
{
    g_return_if_fail (client != NULL);

    if (client->process && client->running)
        g_subprocess_force_exit (client->process);
}


void
lsp_client_disconnect (LspClient *client)
{
    g_return_if_fail (client != NULL);

    if (client->disconnected)
        return;

    client->disconnected = TRUE;

    client->on_notify = NULL;
    client->on_request = NULL;
    client->on_exit = NULL;
    client->cb_data = NULL;

    g_cancellable_cancel (client->cancellable);

    fail_pending_calls (client, "the client was disconnected");
}


/**********************************************************************/
/* Writing
 */

static void
write_ready (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
    LspClient *client = (LspClient*) data;
    GError *error = NULL;

    client->writing = FALSE;
    g_clear_pointer (&client->writing_bytes, g_bytes_unref);

    if (!g_output_stream_write_all_finish (G_OUTPUT_STREAM (source), result, NULL, &error))
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("%s: could not write to %s: %s",
                       G_STRFUNC, client->name, error->message);
        g_clear_error (&error);
    }
    else
    {
        start_write (client);
    }

    lsp_client_unref (client);
}


static void
start_write (LspClient *client)
{
    GBytes *bytes;

    if (client->writing || client->disconnected || !client->running)
        return;

    bytes = (GBytes*) g_queue_pop_head (client->out_queue);

    if (!bytes)
        return;

    client->writing = TRUE;
    client->writing_bytes = bytes;
    lsp_client_ref (client);

    /*
     * write_all rather than write: a pipe takes what it takes, and a partial
     * write in the middle of a framed message desynchronises the server for
     * good. write_all_async does not copy the buffer either, so the bytes are
     * held in writing_bytes until the write completes.
     */
    g_output_stream_write_all_async (client->stdin_stream,
                                     g_bytes_get_data (bytes, NULL),
                                     g_bytes_get_size (bytes),
                                     G_PRIORITY_DEFAULT,
                                     client->cancellable,
                                     write_ready,
                                     client);
}


static void
write_bytes_sync (LspClient *client,
                  GBytes    *bytes)
{
    if (!client->stdin_stream || g_output_stream_is_closed (client->stdin_stream))
        return;

    g_output_stream_write_all (client->stdin_stream,
                               g_bytes_get_data (bytes, NULL),
                               g_bytes_get_size (bytes),
                               NULL, NULL, NULL);
}


void
lsp_client_set_sync_writes (LspClient *client,
                            gboolean   sync_writes)
{
    gint64 deadline;
    gpointer item;

    g_return_if_fail (client != NULL);

    client->sync_writes = sync_writes != FALSE;

    if (!client->sync_writes)
        return;

    /*
     * A write already in flight owns the stream, and a blocking write on top
     * of it fails with G_IO_ERROR_PENDING; give it a moment to finish. The
     * loop is bounded because by this point nobody else is running the main
     * context, so an unfinished write may never finish at all.
     */
    deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND / 5;

    while (client->writing && g_get_monotonic_time () < deadline)
    {
        if (!g_main_context_iteration (NULL, FALSE))
            g_usleep (1000);
    }

    while ((item = g_queue_pop_head (client->out_queue)) != NULL)
    {
        write_bytes_sync (client, (GBytes*) item);
        g_bytes_unref ((GBytes*) item);
    }
}


static void
send_message (LspClient  *client,
              JsonObject *message)
{
    char *body;
    gsize body_len = 0;
    GString *frame;
    GBytes *bytes;

    if (client->disconnected || !client->running)
    {
        json_object_unref (message);
        return;
    }

    body = lsp_json_object_to_string (message, &body_len);
    json_object_unref (message);

    if (!body)
        return;

    if (client->log)
        g_printerr ("lsp: %s <- %s\n", client->name, body);

    frame = g_string_sized_new (body_len + 64);
    g_string_append_printf (frame, "Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n", body_len);
    g_string_append_len (frame, body, body_len);
    g_free (body);

    bytes = g_bytes_new_take (frame->str, frame->len);
    g_string_free (frame, FALSE);

    if (client->sync_writes)
    {
        write_bytes_sync (client, bytes);
        g_bytes_unref (bytes);
        return;
    }

    g_queue_push_tail (client->out_queue, bytes);
    start_write (client);
}


static JsonObject *
new_message (void)
{
    JsonObject *message = json_object_new ();

    lsp_json_set_string (message, "jsonrpc", "2.0");

    return message;
}


gint64
lsp_client_call (LspClient          *client,
                 const char         *method,
                 JsonObject         *params,
                 LspClientReplyFunc  callback,
                 gpointer            data,
                 GDestroyNotify      destroy)
{
    JsonObject *message;
    LspPendingCall *call;
    gint64 *key;
    gint64 id;

    g_return_val_if_fail (client != NULL, 0);
    g_return_val_if_fail (method != NULL, 0);

    if (!lsp_client_is_running (client))
    {
        JsonObject *error = make_error_object (LSP_ERROR_NOT_RUNNING,
                                               "the language server is not running");

        if (params)
            json_object_unref (params);

        if (callback)
            callback (NULL, error, data);

        json_object_unref (error);

        if (destroy)
            destroy (data);

        return 0;
    }

    id = client->next_id++;

    call = g_new0 (LspPendingCall, 1);
    call->callback = callback;
    call->data = data;
    call->destroy = destroy;

    key = g_new (gint64, 1);
    *key = id;
    g_hash_table_insert (client->pending, key, call);

    message = new_message ();
    lsp_json_set_int (message, "id", id);
    lsp_json_set_string (message, "method", method);
    lsp_json_set_object (message, "params", params);

    send_message (client, message);

    return id;
}


void
lsp_client_cancel (LspClient *client,
                   gint64     id)
{
    JsonObject *params;

    g_return_if_fail (client != NULL);

    if (id == 0)
        return;

    /* Drops the callback whether or not the server honours the cancellation. */
    g_hash_table_remove (client->pending, &id);

    if (!lsp_client_is_running (client))
        return;

    params = json_object_new ();
    lsp_json_set_int (params, "id", id);

    lsp_client_notify (client, "$/cancelRequest", params);
}


void
lsp_client_notify (LspClient  *client,
                   const char *method,
                   JsonObject *params)
{
    JsonObject *message;

    g_return_if_fail (client != NULL);
    g_return_if_fail (method != NULL);

    if (!lsp_client_is_running (client))
    {
        if (params)
            json_object_unref (params);
        return;
    }

    message = new_message ();
    lsp_json_set_string (message, "method", method);
    lsp_json_set_object (message, "params", params);

    send_message (client, message);
}


void
lsp_client_reply (LspClient *client,
                  JsonNode  *id,
                  JsonNode  *result)
{
    JsonObject *message;

    g_return_if_fail (client != NULL);

    if (!lsp_client_is_running (client))
    {
        if (result)
            json_node_free (result);
        return;
    }

    message = new_message ();
    lsp_json_set_node (message, "id", id ? json_node_copy (id) : NULL);
    lsp_json_set_node (message, "result", result);

    send_message (client, message);
}


void
lsp_client_reply_error (LspClient  *client,
                        JsonNode   *id,
                        int         code,
                        const char *message_text)
{
    JsonObject *message;

    g_return_if_fail (client != NULL);

    if (!lsp_client_is_running (client))
        return;

    message = new_message ();
    lsp_json_set_node (message, "id", id ? json_node_copy (id) : NULL);
    lsp_json_set_object (message, "error", make_error_object (code, message_text));

    send_message (client, message);
}


/**********************************************************************/
/* Reading
 */

static void
dispatch_message (LspClient  *client,
                  const char *body,
                  gsize       len)
{
    JsonNode *node;
    JsonObject *object;
    JsonNode *id_node;
    const char *method;
    GError *error = NULL;

    if (client->log)
        g_printerr ("lsp: %s -> %.*s\n", client->name, (int) len, body);

    node = lsp_json_parse (body, len, &error);

    if (!node)
    {
        g_warning ("%s: %s sent unparsable JSON: %s", G_STRFUNC, client->name,
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        return;
    }

    if (!JSON_NODE_HOLDS_OBJECT (node))
    {
        g_warning ("%s: %s sent a JSON value that is not an object",
                   G_STRFUNC, client->name);
        json_node_free (node);
        return;
    }

    object = json_node_get_object (node);
    id_node = lsp_json_get_node (object, "id");
    method = lsp_json_get_string (object, "method");

    if (id_node && !JSON_NODE_HOLDS_NULL (id_node) && method)
    {
        /* A request from the server to us. */
        if (client->on_request)
            client->on_request (id_node, method,
                                lsp_json_get_object (object, "params"),
                                client->cb_data);
        else
            lsp_client_reply_error (client, id_node, LSP_ERROR_INTERNAL,
                                    "medit handles no server requests");
    }
    else if (id_node && !JSON_NODE_HOLDS_NULL (id_node))
    {
        /* A reply to one of ours. */
        gint64 id = json_node_get_value_type (id_node) == G_TYPE_STRING
                        ? g_ascii_strtoll (json_node_get_string (id_node), NULL, 10)
                        : json_node_get_int (id_node);
        gpointer key = NULL, value = NULL;

        /*
         * Taken out of the table before the callback runs: the callback may
         * issue new calls, and must not be able to see its own entry. A reply
         * with no entry belongs to a call that was cancelled.
         */
        if (g_hash_table_steal_extended (client->pending, &id, &key, &value))
        {
            LspPendingCall *call = (LspPendingCall*) value;

            if (call->callback)
                call->callback (lsp_json_get_node (object, "result"),
                                lsp_json_get_object (object, "error"),
                                call->data);

            g_free (key);
            pending_call_free (call);
        }
    }
    else if (method)
    {
        if (client->on_notify)
            client->on_notify (method, lsp_json_get_object (object, "params"),
                               client->cb_data);
    }
    else
    {
        g_warning ("%s: %s sent a message with neither id nor method",
                   G_STRFUNC, client->name);
    }

    json_node_free (node);
}


/*
 * Returns the length of one complete message including its header, zero when
 * the buffer does not hold one yet, and -1 when the stream is unusable.
 */
static gssize
find_message (const guint8 *data,
              gsize         size,
              gsize        *body_offset,
              gsize        *body_len)
{
    static const char *terminators[] = { "\r\n\r\n", "\n\n" };
    const char *header_end = NULL;
    gsize header_len = 0;
    gsize content_length = 0;
    gboolean have_length = FALSE;
    char **lines;
    char *header;
    guint i;

    for (i = 0; i < G_N_ELEMENTS (terminators); ++i)
    {
        /*
         * The specification says CRLF, and every server sends it; the bare LF
         * is accepted so that a hand-written test harness works too.
         */
        const char *found = g_strstr_len ((const char*) data, size, terminators[i]);

        if (found && (!header_end || found < header_end))
        {
            header_end = found;
            header_len = (found - (const char*) data) + strlen (terminators[i]);
        }
    }

    if (!header_end)
        return 0;

    header = g_strndup ((const char*) data, header_end - (const char*) data);
    lines = g_strsplit_set (header, "\r\n", -1);

    for (i = 0; lines[i]; ++i)
    {
        char *colon = strchr (lines[i], ':');

        if (!colon)
            continue;

        *colon = '\0';

        if (g_ascii_strcasecmp (lines[i], "Content-Length") == 0)
        {
            content_length = (gsize) g_ascii_strtoull (g_strstrip (colon + 1), NULL, 10);
            have_length = TRUE;
        }
    }

    g_strfreev (lines);
    g_free (header);

    if (!have_length)
        return -1;

    if (content_length > LSP_MAX_MESSAGE)
        return -1;

    if (size < header_len + content_length)
        return 0;

    *body_offset = header_len;
    *body_len = content_length;

    return (gssize) (header_len + content_length);
}


static void
process_in_buf (LspClient *client)
{
    while (!client->disconnected)
    {
        gsize body_offset = 0, body_len = 0;
        gssize total;
        char *body;

        if (client->in_buf->len == 0)
            break;

        total = find_message (client->in_buf->data, client->in_buf->len,
                              &body_offset, &body_len);

        if (total == 0)
        {
            if (client->in_buf->len > LSP_MAX_MESSAGE)
            {
                g_warning ("%s: %s sent %u bytes without a usable header, dropping the connection",
                           G_STRFUNC, client->name, client->in_buf->len);
                lsp_client_force_exit (client);
            }
            break;
        }

        if (total < 0)
        {
            g_warning ("%s: %s sent a malformed message header, dropping the connection",
                       G_STRFUNC, client->name);
            lsp_client_force_exit (client);
            g_byte_array_set_size (client->in_buf, 0);
            break;
        }

        /* The body is copied out before the callbacks run, since one of them
           may feed the client and reallocate the buffer under us. */
        body = g_strndup ((const char*) client->in_buf->data + body_offset, body_len);
        g_byte_array_remove_range (client->in_buf, 0, (guint) total);

        dispatch_message (client, body, body_len);

        g_free (body);
    }
}


static void
read_ready (GObject      *source,
            GAsyncResult *result,
            gpointer      data)
{
    LspClient *client = (LspClient*) data;
    GError *error = NULL;
    gssize count;

    client->reading = FALSE;

    count = g_input_stream_read_finish (G_INPUT_STREAM (source), result, &error);

    if (count > 0)
    {
        g_byte_array_append (client->in_buf, client->read_chunk, (guint) count);
        process_in_buf (client);
        start_read (client);
    }
    else if (count < 0)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("%s: could not read from %s: %s", G_STRFUNC, client->name,
                       error->message);
    }

    g_clear_error (&error);

    lsp_client_unref (client);
}


static void
start_read (LspClient *client)
{
    if (client->reading || client->disconnected)
        return;

    client->reading = TRUE;
    lsp_client_ref (client);

    g_input_stream_read_async (client->stdout_stream,
                               client->read_chunk, sizeof client->read_chunk,
                               G_PRIORITY_DEFAULT, client->cancellable,
                               read_ready, client);
}


/*
 * The stderr pipe is drained even when nothing looks at it: a server that
 * talks a lot -- clangd logs every request at its default verbosity -- fills
 * the pipe buffer otherwise and then blocks forever on its next write.
 */
static void
read_stderr_ready (GObject      *source,
                   GAsyncResult *result,
                   gpointer      data)
{
    LspClient *client = (LspClient*) data;
    gssize count;

    client->reading_stderr = FALSE;

    count = g_input_stream_read_finish (G_INPUT_STREAM (source), result, NULL);

    if (count > 0)
    {
        if (client->log)
            g_printerr ("lsp: %s stderr: %.*s", client->name,
                        (int) count, (const char*) client->err_chunk);

        start_read_stderr (client);
    }

    lsp_client_unref (client);
}


static void
start_read_stderr (LspClient *client)
{
    if (client->reading_stderr || client->disconnected)
        return;

    client->reading_stderr = TRUE;
    lsp_client_ref (client);

    g_input_stream_read_async (client->stderr_stream,
                               client->err_chunk, sizeof client->err_chunk - 1,
                               G_PRIORITY_LOW, client->cancellable,
                               read_stderr_ready, client);
}
