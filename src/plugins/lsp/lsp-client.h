/*
 *   plugins/lsp/lsp-client.h
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
 * The transport: a child process spoken to in JSON-RPC 2.0 framed the way the
 * language server protocol wants it, a Content-Length header and a blank line
 * before every message. Knows nothing about what the messages mean.
 *
 * Everything here is asynchronous. The client is reference counted because a
 * reply can arrive after its owner is gone: every in-flight operation holds a
 * reference, and an owner that goes away calls lsp_client_disconnect() first,
 * which cancels the i/o and drops the callbacks.
 */

#ifndef MOO_LSP_CLIENT_H
#define MOO_LSP_CLIENT_H

#include "plugins/lsp/lsp-json.h"

G_BEGIN_DECLS

typedef struct LspClient LspClient;

/*
 * Exactly one of result and error is non-NULL. Neither is owned by the
 * callback; copy what has to outlive it.
 */
typedef void (*LspClientReplyFunc)   (JsonNode       *result,
                                      JsonObject     *error,
                                      gpointer        data);

typedef void (*LspClientNotifyFunc)  (const char     *method,
                                      JsonObject     *params,
                                      gpointer        data);

/* A request from the server. The id has to be given back to lsp_client_reply. */
typedef void (*LspClientRequestFunc) (JsonNode       *id,
                                      const char     *method,
                                      JsonObject     *params,
                                      gpointer        data);

/* Positive is an exit code, negative is minus the signal that killed it. */
typedef void (*LspClientExitFunc)    (int             status,
                                      gpointer        data);

LspClient  *lsp_client_new          (char          **argv,
                                     const char     *working_dir,
                                     char          **env,
                                     GError        **error);

LspClient  *lsp_client_ref          (LspClient      *client);
void        lsp_client_unref        (LspClient      *client);

void        lsp_client_set_callbacks (LspClient             *client,
                                      LspClientNotifyFunc    on_notify,
                                      LspClientRequestFunc   on_request,
                                      LspClientExitFunc      on_exit,
                                      gpointer               data);

/* Logs every message to stderr, for MOO_LSP_PREFS_DEBUG. */
void        lsp_client_set_logging  (LspClient      *client,
                                     gboolean        log);

/*
 * Takes ownership of params. Returns the request id, which is what
 * lsp_client_cancel() wants; zero when the client is no longer running, in
 * which case the callback is invoked with an error before returning.
 */
gint64      lsp_client_call         (LspClient          *client,
                                     const char         *method,
                                     JsonObject         *params,
                                     LspClientReplyFunc  callback,
                                     gpointer            data,
                                     GDestroyNotify      destroy);

/* Sends $/cancelRequest and forgets the reply. */
void        lsp_client_cancel       (LspClient      *client,
                                     gint64          id);

/* Takes ownership of params. */
void        lsp_client_notify       (LspClient      *client,
                                     const char     *method,
                                     JsonObject     *params);

/* Takes ownership of result. */
void        lsp_client_reply        (LspClient      *client,
                                     JsonNode       *id,
                                     JsonNode       *result);
void        lsp_client_reply_error  (LspClient      *client,
                                     JsonNode       *id,
                                     int             code,
                                     const char     *message);

gboolean    lsp_client_is_running   (LspClient      *client);

/*
 * Switches to blocking writes and writes out whatever is queued. Everything
 * else here is asynchronous, which is no use on the way out: once the plugin
 * is being unloaded the main loop is going away, and a queued write would
 * simply never happen, leaving the server to work out that its pipe closed
 * rather than being told to exit. Turn this on before the last few messages,
 * not before.
 */
void        lsp_client_set_sync_writes (LspClient   *client,
                                        gboolean     sync_writes);

/* Lets the process see end of file on its stdin, so that it can exit. */
void        lsp_client_close_stdin  (LspClient      *client);

/* SIGKILL, for a process that did not exit on its own. */
void        lsp_client_force_exit   (LspClient      *client);

/*
 * Cancels the i/o, drops the callbacks and fails every pending call. Call it
 * before dropping the last reference.
 */
void        lsp_client_disconnect   (LspClient      *client);

G_END_DECLS

#endif /* MOO_LSP_CLIENT_H */
