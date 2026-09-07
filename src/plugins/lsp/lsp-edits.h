/*
 *   plugins/lsp/lsp-edits.h
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
 * Everything that changes a document on a server's say-so: the edits
 * themselves, renaming a symbol, and laying a file out.
 *
 * The two features share a file because the reply is the same thing. A rename
 * answers with edits to several files and a formatting request with edits to
 * one, and applying either correctly is the same three rules: back to front
 * within a file, one file at a time, one undo step each.
 */

#ifndef MOO_LSP_EDITS_H
#define MOO_LSP_EDITS_H

#include "plugins/lsp/lsp-doc.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/* One TextEdit, in the coordinates the server wrote it in. */
typedef struct {
    char *path;
    int   start_line;
    int   start_character;
    int   end_line;
    int   end_character;
    char *new_text;
} LspTextEdit;

/*
 * The edits of a WorkspaceEdit, in the order they must be applied: grouped by
 * file, and within a file the last one first, so that applying one cannot move
 * the ranges of those still to come.
 *
 * Both shapes are read. "changes" is a map of uri to edits; "documentChanges"
 * is the newer array, which a server may send whether or not the client asked
 * for it, and whose file operations -- create, rename, delete -- are ignored
 * here, medit having nothing to do with them.
 */
GSList     *lsp_workspace_edit_parse (JsonNode          *result);

/*
 * The same, for a plain array of TextEdits over one file -- which is what a
 * formatting request comes back with. Sorted the same way, for the same
 * reason.
 */
GSList     *lsp_text_edits_parse     (JsonNode          *result,
                                      const char        *path);

/*
 * Applies them: each file is opened if it is not open, changed in one undo
 * step, and left modified. Nothing is saved -- what a server did to a file is
 * worth looking at before it is on disk.
 */
void        lsp_text_edits_apply     (MooEditWindow      *window,
                                      GSList             *edits,
                                      LspPositionEncoding encoding);

void        lsp_text_edit_free       (LspTextEdit       *edit);
void        lsp_text_edits_free      (GSList            *edits);

/*
 * Asks for a new name, asks the server what that means, and applies it. view
 * is the view a context menu belongs to, whose last right click is what is
 * being renamed; NULL renames what the cursor is in.
 */
void        lsp_rename               (MooEditWindow     *window,
                                      MooEditView       *view);

/* Asks the server to lay the active document out, and applies the answer. */
void        lsp_format               (MooEditWindow     *window);

G_END_DECLS

#endif /* MOO_LSP_EDITS_H */
