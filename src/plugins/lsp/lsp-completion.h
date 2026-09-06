/*
 *   plugins/lsp/lsp-completion.h
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
 * The completion popup. medit has no completion of its own, so this is a
 * window of its own with a list in it, placed under the cursor, taking the
 * keys it needs before the text view sees them.
 *
 * Only one can be open at a time, which is why nothing here is an object.
 */

#ifndef MOO_LSP_COMPLETION_H
#define MOO_LSP_COMPLETION_H

#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/*
 * Asks the server what could go at the cursor and shows the answer.
 * trigger_char is the character that brought it up, or NULL when the user
 * asked for it.
 */
void        lsp_completion_start        (MooEditView    *view,
                                         const char     *trigger_char);

void        lsp_completion_cancel       (void);
gboolean    lsp_completion_visible      (void);

/* Returns TRUE when the key belonged to the popup. */
gboolean    lsp_completion_key_press    (MooEditView    *view,
                                         GdkEventKey    *event);

/*
 * Called after text reaches the buffer: opens the popup on one of the
 * server's trigger characters, and narrows the list while it is open.
 */
void        lsp_completion_text_inserted (MooEditView   *view,
                                          const char    *text);

G_END_DECLS

#endif /* MOO_LSP_COMPLETION_H */
