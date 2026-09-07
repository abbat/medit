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
 *
 * Where the question is about lives here too, and is used by everything that
 * asks one: the place of the last right click when a context menu is what
 * asked, and the cursor otherwise.
 */

#ifndef MOO_LSP_NAVIGATE_H
#define MOO_LSP_NAVIGATE_H

#include "plugins/lsp/lsp-doc.h"

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
gboolean    lsp_can_ask             (MooEditWindow  *window,
                                     const char     *method);

/*
 * The place a question is about, and the document it is about it in. view is
 * the view whose context menu asked -- the question is then about where the
 * right click that opened it landed -- and NULL asks about the cursor, which
 * is also the answer for a menu that was opened from the keyboard. iter, when
 * it is wanted, comes back at that place in the document's own buffer.
 */
gboolean    lsp_ask_position        (MooEditWindow  *window,
                                     MooEditView    *view,
                                     LspDoc        **ldoc,
                                     GtkTextIter    *iter,
                                     int            *line,
                                     int            *character);

/* The textDocument and position pair every such request carries. */
JsonObject *lsp_position_params     (LspDoc         *ldoc,
                                     int             line,
                                     int             character);

/*
 * Opens the file if it is not open, and puts the cursor at that place. The
 * character is in the server's own counting, and is converted against the
 * buffer of whatever document it ends up in -- which is why it is done here
 * and not by the caller, who has no such buffer until this has run.
 */
void        lsp_go_to_place         (MooEditWindow  *window,
                                     const char     *path,
                                     int             line,
                                     int             character,
                                     LspPositionEncoding encoding);

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
