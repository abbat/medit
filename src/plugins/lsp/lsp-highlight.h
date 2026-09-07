/*
 *   plugins/lsp/lsp-highlight.h
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
 * The other uses of whatever the cursor is in, marked in the document.
 *
 * One cursor, one set of marks: they belong to where the cursor is now, and
 * are taken off the moment it moves rather than left to be replaced by the
 * next answer -- marks pointing at the uses of the word before last are worse
 * than no marks at all.
 */

#ifndef MOO_LSP_HIGHLIGHT_H
#define MOO_LSP_HIGHLIGHT_H

#include "plugins/lsp/lsp-doc.h"

G_BEGIN_DECLS

/* One range the server named, in its own coordinates. */
typedef struct {
    int start_line;
    int start_character;
    int end_line;
    int end_character;

    /* DocumentHighlightKind: 1 text, 2 read, 3 write. A server that sends
       none means text, which is the specification's own default. */
    int kind;
} LspHighlight;

/*
 * The two tags the marks are made of, by name: which range wears which is the
 * whole of the kind, and a name is the only handle on a tag there is.
 */
#define LSP_HIGHLIGHT_TAG_READ  "moo-lsp-highlight-read"
#define LSP_HIGHLIGHT_TAG_WRITE "moo-lsp-highlight-write"

/* Which of them a kind is drawn with. Anything that is not a write reads. */
const char *lsp_highlight_tag_name      (int         kind);

GSList     *lsp_highlight_parse         (JsonNode   *result);

/* Puts the marks on, converting each position against the text. */
void        lsp_highlight_apply         (GtkTextBuffer      *buffer,
                                         GSList             *highlights,
                                         LspPositionEncoding encoding);
void        lsp_highlight_free          (GSList     *highlights);

/* The cursor moved: takes the marks off and asks again after a moment. */
void        lsp_highlight_cursor_moved  (MooEdit    *doc);

/* Takes the marks off whatever is wearing them. */
void        lsp_highlight_clear         (void);

G_END_DECLS

#endif /* MOO_LSP_HIGHLIGHT_H */
