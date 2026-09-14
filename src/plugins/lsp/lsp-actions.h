/*
 *   plugins/lsp/lsp-actions.h
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
 * What the server offers to do about where the cursor is: the fix for a
 * diagnostic, an import to add, a refactoring to run.
 *
 * The question is textDocument/codeAction, and what comes back is a list the
 * user picks from. Each entry carries an edit, a command, or both -- an edit
 * is applied here like any other, and a command is handed back to the server
 * to run, which is why answering workspace/applyEdit is part of this and not
 * a nicety: a command that produced changes would otherwise be a menu entry
 * that does nothing.
 */

#ifndef MOO_LSP_ACTIONS_H
#define MOO_LSP_ACTIONS_H

#include "plugins/lsp/lsp-doc.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/*
 * One entry of the reply. The protocol has two shapes for it -- a bare Command
 * from before LSP 3.8 and a CodeAction since -- and this is what is left of
 * either once the difference stops mattering.
 */
typedef struct {
    char     *title;
    char     *kind;         /* "quickfix", "refactor.extract", NULL if unsaid */
    char     *disabled;     /* why it cannot be used, NULL when it can */
    gboolean  preferred;    /* the one to offer first, isPreferred */
    JsonNode *edit;         /* a WorkspaceEdit, NULL when there is only a command */
    char     *command;      /* NULL when there is only an edit */
    JsonNode *arguments;    /* what the command takes, NULL when it takes none */
} LspCodeAction;

/* In the order the server listed them, which is the order they are offered in. */
GSList     *lsp_code_actions_parse   (JsonNode           *result);
void        lsp_code_action_free     (LspCodeAction      *action);
void        lsp_code_actions_free    (GSList             *actions);

/*
 * The diagnostics a request carries in its context: the ones the range
 * touches, as the server itself wrote them.
 *
 * Not rebuilt out of LspDiagnostic: a server recognises its own diagnostic by
 * the data member it hung on it, which medit does not read and must not drop
 * -- for many servers that member is the whole of how a quick fix is found
 * again.
 */
JsonArray  *_lsp_code_action_context (JsonArray          *diagnostics,
                                      int                 start_line,
                                      int                 start_character,
                                      int                 end_line,
                                      int                 end_character);

/*
 * Asks, and offers what comes back in a menu. view is the view whose context
 * menu asked, and NULL asks about the cursor; see lsp_ask_position().
 */
void        lsp_code_actions         (MooEditWindow      *window,
                                      MooEditView        *view);

G_END_DECLS

#endif /* MOO_LSP_ACTIONS_H */
