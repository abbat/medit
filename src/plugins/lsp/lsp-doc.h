/*
 *   plugins/lsp/lsp-doc.h
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
 * One open document as the server sees it: its uri, its language, its version
 * counter, and the timer that turns a burst of typing into one didChange.
 */

#ifndef MOO_LSP_DOC_H
#define MOO_LSP_DOC_H

#include "plugins/lsp/lsp-server.h"
#include "mooedit/mooedit.h"

G_BEGIN_DECLS

typedef struct LspDoc LspDoc;

LspDoc     *lsp_doc_new             (MooEdit    *doc,
                                     LspServer  *server);
void        lsp_doc_free            (LspDoc     *ldoc);

MooEdit    *lsp_doc_get_doc         (LspDoc     *ldoc);
LspServer  *lsp_doc_get_server      (LspDoc     *ldoc);
const char *lsp_doc_get_uri         (LspDoc     *ldoc);

/* Sends didOpen. Called again after a server restart, which forgets everything. */
void        lsp_doc_open            (LspDoc     *ldoc);

/* Sends the pending didChange now instead of when the timer fires. */
void        lsp_doc_flush           (LspDoc     *ldoc);

/*
 * Takes the diagnostics the server pushed, shows them on the document and
 * keeps them for the pane. The list belongs to the LspDoc.
 */
void        lsp_doc_set_diagnostics (LspDoc     *ldoc,
                                     JsonArray  *array);
GSList     *lsp_doc_get_diagnostics (LspDoc     *ldoc);

/* Shows the diagnostics already stored again, after a preference changed. */
void        lsp_doc_refresh_diagnostics (LspDoc *ldoc);

/*
 * Whether the document still has the file and the language this was built for.
 * Saving under a new name or picking another language invalidates it.
 */
gboolean    lsp_doc_is_current      (LspDoc     *ldoc);

/*
 * The protocol counts a Position in UTF-16 code units unless the server agreed
 * to something else, while a GtkTextIter counts characters. On anything but
 * ASCII the two disagree, so no position may cross this boundary unconverted.
 */
void        lsp_iter_to_position    (const GtkTextIter   *iter,
                                     LspPositionEncoding  encoding,
                                     int                 *line,
                                     int                 *character);
gboolean    lsp_position_to_iter    (GtkTextBuffer       *buffer,
                                     int                  line,
                                     int                  character,
                                     LspPositionEncoding  encoding,
                                     GtkTextIter         *iter);

/* The languageId the protocol expects for a medit language id. */
const char *lsp_language_id         (const char *moo_lang_id);

G_END_DECLS

#endif /* MOO_LSP_DOC_H */
