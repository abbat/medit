/*
 *   plugins/lsp/lsp-symbols.h
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
 * What a document contains, as the server sees it: the answer to
 * textDocument/documentSymbol turned into a tree model.
 */

#ifndef MOO_LSP_SYMBOLS_H
#define MOO_LSP_SYMBOLS_H

#include "plugins/lsp/lsp-server.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

enum {
    LSP_SYMBOL_COLUMN_MARKUP,       /* what the row shows */
    LSP_SYMBOL_COLUMN_NAME,         /* the bare name, for interactive search */
    LSP_SYMBOL_COLUMN_LINE,         /* where to go, in the document's own */
    LSP_SYMBOL_COLUMN_CHARACTER,    /* coordinates rather than the protocol's */
    LSP_SYMBOL_N_COLUMNS
};

GtkTreeStore *lsp_symbols_new_store (void);

/*
 * Fills the store from a documentSymbol reply. The reply comes in two shapes
 * -- a flat SymbolInformation list from older servers, a nested DocumentSymbol
 * list from newer ones -- and both are handled.
 *
 * Positions are resolved against the buffer, so that what is stored is where
 * the cursor should go rather than what the protocol counts.
 */
void          lsp_symbols_fill      (GtkTreeStore        *store,
                                     JsonNode            *result,
                                     GtkTextBuffer       *buffer,
                                     LspPositionEncoding  encoding);

/* Translated name of a SymbolKind, or NULL for one medit does not know. */
const char   *lsp_symbol_kind_name  (int                  kind);

G_END_DECLS

#endif /* MOO_LSP_SYMBOLS_H */
