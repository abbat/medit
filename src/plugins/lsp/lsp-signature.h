/*
 *   plugins/lsp/lsp-signature.h
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
 * What the call being typed takes: the signature, with the parameter the
 * cursor is in picked out, in a popup under the cursor.
 *
 * A window of the plugin's own, like the completion popup and for the same
 * reason -- medit has nothing of the kind -- but one that takes no keys except
 * the Escape that closes it: what is being typed is the call, not the popup.
 */

#ifndef MOO_LSP_SIGNATURE_H
#define MOO_LSP_SIGNATURE_H

#include "plugins/lsp/lsp-json.h"

#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/*
 * Asks the server what the call under the cursor takes and shows the answer.
 * trigger_char is the character that brought it up, or NULL when the user
 * asked for it.
 */
void        lsp_signature_start         (MooEditView    *view,
                                         const char     *trigger_char);

void        lsp_signature_cancel        (void);
gboolean    lsp_signature_visible       (void);

/* Returns TRUE when the key belonged to the popup, which is Escape and
   nothing else: everything else being typed is the call. */
gboolean    lsp_signature_key_press     (MooEditView    *view,
                                         GdkEventKey    *event);

/* Called after text reaches the buffer: opens the popup on one of the
   characters the server named, and asks again on the next one. */
void        lsp_signature_text_inserted (MooEditView    *view,
                                         const char     *text);

/*
 * What one reply looks like on screen: the active signature with its active
 * parameter in bold, and a second line of documentation when there is one --
 * the parameter's own, or the signature's when the parameter has none. NULL
 * when there is nothing worth showing.
 *
 * Pango markup, so everything taken from the server is escaped: a C++
 * signature is full of & and <.
 */
char       *lsp_signature_markup        (JsonNode       *result);

G_END_DECLS

#endif /* MOO_LSP_SIGNATURE_H */
