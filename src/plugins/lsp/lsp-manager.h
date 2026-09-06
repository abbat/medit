/*
 *   plugins/lsp/lsp-manager.h
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
 * Turns the configuration file into running servers: which entry applies to a
 * document, where the root of its project is, which server that adds up to,
 * and which documents each server has been told about.
 */

#ifndef MOO_LSP_MANAGER_H
#define MOO_LSP_MANAGER_H

#include "plugins/lsp/lsp-doc.h"

G_BEGIN_DECLS

void        lsp_manager_init            (void);
void        lsp_manager_shutdown        (void);

/* Re-reads the configuration file and starts over with every server. */
void        lsp_manager_reload          (void);

/*
 * Attaches a document, which starts a server when it is the first one that
 * needs it. Does nothing when no entry matches, when the document has no file
 * on disk, or when the entry names a command that is not installed.
 */
void        lsp_manager_add_doc         (MooEdit    *doc);
void        lsp_manager_remove_doc      (MooEdit    *doc);
LspDoc     *lsp_manager_lookup_doc      (MooEdit    *doc);

/* Borrowed, in no particular order. */
GSList     *lsp_manager_list_servers    (void);

/*
 * Told whenever a server pushes a new set of diagnostics for a document, so
 * that a window showing that document can refresh its pane.
 */
typedef void (*LspDiagnosticsNotifyFunc) (MooEdit    *doc,
                                          gpointer    data);

void        lsp_manager_add_listener    (LspDiagnosticsNotifyFunc  func,
                                         gpointer                  data);
void        lsp_manager_remove_listener (LspDiagnosticsNotifyFunc  func,
                                         gpointer                  data);

/* Re-applies what is already known, after a change of preferences. */
void        lsp_manager_refresh_diagnostics (void);

G_END_DECLS

#endif /* MOO_LSP_MANAGER_H */
