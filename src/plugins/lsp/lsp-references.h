/*
 *   plugins/lsp/lsp-references.h
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
 * Every place a symbol is used, listed in a pane.
 *
 * A list of Locations is what a server answers half its questions with, so the
 * parsing of one lives here rather than beside the code that goes to a
 * definition: a definition reply is the same list, of which only the first
 * element is any use.
 */

#ifndef MOO_LSP_REFERENCES_H
#define MOO_LSP_REFERENCES_H

#include "plugins/lsp/lsp-doc.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooeditview.h"

G_BEGIN_DECLS

/* One place in a file, in the coordinates the server named it in. */
typedef struct {
    char               *path;
    int                 line;
    int                 character;
    LspPositionEncoding encoding;

    /* What the References pane shows, filled in by lsp_locations_describe():
       the place, and the line of the file it is on. */
    char               *display;
    char               *text;
} LspLocation;

/*
 * A Location, a LocationLink, or an array of either -- which of the three a
 * server sends depends on the server and on the question. The list is in the
 * order the server gave, and anything whose uri does not name a local file is
 * dropped rather than listed as a place nothing can be done with.
 */
GSList     *lsp_locations_parse     (JsonNode           *result,
                                     LspPositionEncoding encoding);

/*
 * Gives every location the line the pane shows: the path relative to root,
 * the position resolved against the text of the file rather than left in the
 * server's UTF-16 counting, and that line of the file. The text comes from
 * the open document when the file is open and from the disk when it is not,
 * which is what makes a use in a file nobody has opened readable at all.
 */
void        lsp_locations_describe  (GSList             *locations,
                                     const char         *root);

void        lsp_location_free       (LspLocation        *location);
void        lsp_locations_free      (GSList             *locations);

/*
 * Asks where what is under the cursor is used, and lists the answer. view is
 * the view a context menu belongs to, whose last right click is what the
 * question is about; NULL asks about the cursor.
 */
void        lsp_find_references     (MooEditWindow      *window,
                                     MooEditView        *view);

G_END_DECLS

#endif /* MOO_LSP_REFERENCES_H */
