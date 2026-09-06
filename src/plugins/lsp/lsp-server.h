/*
 *   plugins/lsp/lsp-server.h
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
 * One running language server: the handshake, what it said it can do, the set
 * of documents it has been told about, and what happens when it dies.
 *
 * A server is started for one configuration entry and one project root, and is
 * shared by every document under that root.
 */

#ifndef MOO_LSP_SERVER_H
#define MOO_LSP_SERVER_H

#include "plugins/lsp/lsp-client.h"
#include "plugins/lsp/lsp-config.h"

G_BEGIN_DECLS

typedef struct LspServer LspServer;

/*
 * Which unit the character member of a Position counts. UTF-16 is what the
 * protocol has always meant; the other two exist only when the server accepts
 * the positionEncodings offer that LSP 3.17 added.
 */
typedef enum {
    LSP_POSITION_ENCODING_UTF16 = 0,
    LSP_POSITION_ENCODING_UTF8,
    LSP_POSITION_ENCODING_UTF32
} LspPositionEncoding;

typedef enum {
    LSP_SYNC_NONE        = 0,
    LSP_SYNC_FULL        = 1,
    LSP_SYNC_INCREMENTAL = 2
} LspSyncKind;

typedef enum {
    LSP_SERVER_STARTING = 0,
    LSP_SERVER_READY,
    LSP_SERVER_FAILED
} LspServerState;

/* The server pushed diagnostics for one document. */
typedef void (*LspServerDiagnosticsFunc) (LspServer  *server,
                                          const char *uri,
                                          JsonArray  *diagnostics,
                                          gpointer    data);

/*
 * The state changed. After a restart the server has forgotten every document,
 * so the owner has to open them again; lsp_server_is_ready() says whether it
 * is worth trying.
 */
typedef void (*LspServerStateFunc)       (LspServer  *server,
                                          gpointer    data);

LspServer  *lsp_server_new              (LspServerConfig    *config,
                                         const char         *root_dir);
LspServer  *lsp_server_ref              (LspServer          *server);
void        lsp_server_unref            (LspServer          *server);

void        lsp_server_set_callbacks    (LspServer                *server,
                                         LspServerDiagnosticsFunc  on_diagnostics,
                                         LspServerStateFunc        on_state,
                                         gpointer                  data);

const char *lsp_server_get_id           (LspServer          *server);
const char *lsp_server_get_root         (LspServer          *server);
LspServerState lsp_server_get_state     (LspServer          *server);
gboolean    lsp_server_is_ready         (LspServer          *server);

/* Why it is in LSP_SERVER_FAILED, ready to be shown to the user. */
const char *lsp_server_get_error        (LspServer          *server);

LspPositionEncoding lsp_server_get_position_encoding (LspServer *server);
LspSyncKind lsp_server_get_sync_kind    (LspServer          *server);

/* "hoverProvider", "definitionProvider" and the like. */
gboolean    lsp_server_has_provider     (LspServer          *server,
                                         const char         *name);
JsonObject *lsp_server_get_capabilities (LspServer          *server);

void        lsp_server_did_open         (LspServer          *server,
                                         const char         *uri,
                                         const char         *language_id,
                                         int                 version,
                                         const char         *text);
void        lsp_server_did_change       (LspServer          *server,
                                         const char         *uri,
                                         int                 version,
                                         const char         *text);
void        lsp_server_did_save         (LspServer          *server,
                                         const char         *uri,
                                         const char         *text);
void        lsp_server_did_close        (LspServer          *server,
                                         const char         *uri);

gboolean    lsp_server_has_doc          (LspServer          *server,
                                         const char         *uri);
guint       lsp_server_count_docs       (LspServer          *server);

/*
 * Takes ownership of params. Calls made before the handshake finishes are
 * queued and sent when it does; those return zero rather than a request id,
 * so a caller that intends to cancel has to wait for lsp_server_is_ready().
 */
gint64      lsp_server_call             (LspServer          *server,
                                         const char         *method,
                                         JsonObject         *params,
                                         LspClientReplyFunc  callback,
                                         gpointer            data,
                                         GDestroyNotify      destroy);
void        lsp_server_cancel           (LspServer          *server,
                                         gint64              id);

/*
 * Makes everything sent from now on go out with blocking writes. Called on the
 * way out of the application, before the documents are closed, so that the
 * last notifications actually reach the server.
 */
void        lsp_server_set_sync_writes  (LspServer          *server);

/* Asks it to exit, and kills it if it does not. */
void        lsp_server_shutdown         (LspServer          *server);

G_END_DECLS

#endif /* MOO_LSP_SERVER_H */
