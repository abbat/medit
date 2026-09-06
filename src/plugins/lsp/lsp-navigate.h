/*
 *   plugins/lsp/lsp-navigate.h
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
 * The two things a server can say about one position in a document: where the
 * thing under the cursor is defined, and what it is.
 */

#ifndef MOO_LSP_NAVIGATE_H
#define MOO_LSP_NAVIGATE_H

#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/*
 * Asks the server where what is under the cursor is defined, and goes there
 * when it answers. Which of the four questions is asked depends on the method:
 * "textDocument/definition", "declaration", "typeDefinition" or
 * "implementation".
 */
void        lsp_goto_definition     (MooEditWindow  *window,
                                     const char     *method);

/*
 * The same, but about the place the last button press landed rather than about
 * the cursor. GtkTextView does not move the cursor on a right click, so an
 * entry in the context menu that went by the cursor would answer about
 * wherever the cursor happened to be left, and would look as though it needed
 * the word selected first.
 */
void        lsp_goto_definition_at_click (MooEditView *view,
                                          const char  *method);

/*
 * Records where a right click landed, and forgets it again on anything else
 * that moves the cursor -- another click, a key -- so that a menu opened from
 * the keyboard goes by the cursor rather than by an old click.
 */
void        lsp_navigate_note_click (MooEditView    *view,
                                     int             x,
                                     int             y);
void        lsp_navigate_forget_click (void);

/* Whether the active document has a server that can answer that method. */
gboolean    lsp_can_goto            (MooEditWindow  *window,
                                     const char     *method);

/* The ::query-tooltip handler for a document view. */
gboolean    lsp_hover_query_tooltip (MooEditView    *view,
                                     int             x,
                                     int             y,
                                     gboolean        keyboard_mode,
                                     GtkTooltip     *tooltip);

/* Drops the cached hover and forgets any request still in flight. */
void        lsp_navigate_reset      (void);

G_END_DECLS

#endif /* MOO_LSP_NAVIGATE_H */
