/*
 *   plugins/lsp/lsp-symbols.cpp
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

#include "plugins/lsp/lsp-symbols.h"
#include "plugins/lsp/lsp-doc.h"

#include "mooutils/mooi18n.h"

#include <string.h>

/*
 * SymbolKind, as numbered by the protocol. C_() concatenates its two literals
 * at compile time, so the translation has to happen here rather than through a
 * table of untranslated names.
 */
const char *
lsp_symbol_kind_name (int kind)
{
    switch (kind)
    {
        case 1:  return C_("symbol kind", "file");
        case 2:  return C_("symbol kind", "module");
        case 3:  return C_("symbol kind", "namespace");
        case 4:  return C_("symbol kind", "package");
        case 5:  return C_("symbol kind", "class");
        case 6:  return C_("symbol kind", "method");
        case 7:  return C_("symbol kind", "property");
        case 8:  return C_("symbol kind", "field");
        case 9:  return C_("symbol kind", "constructor");
        case 10: return C_("symbol kind", "enum");
        case 11: return C_("symbol kind", "interface");
        case 12: return C_("symbol kind", "function");
        case 13: return C_("symbol kind", "variable");
        case 14: return C_("symbol kind", "constant");
        case 15: return C_("symbol kind", "string");
        case 16: return C_("symbol kind", "number");
        case 17: return C_("symbol kind", "boolean");
        case 18: return C_("symbol kind", "array");
        case 19: return C_("symbol kind", "object");
        case 20: return C_("symbol kind", "key");
        case 21: return C_("symbol kind", "null");
        case 22: return C_("symbol kind", "enum member");
        case 23: return C_("symbol kind", "struct");
        case 24: return C_("symbol kind", "event");
        case 25: return C_("symbol kind", "operator");
        case 26: return C_("symbol kind", "type parameter");
        default: return NULL;
    }
}


GtkTreeStore *
lsp_symbols_new_store (void)
{
    return gtk_tree_store_new (LSP_SYMBOL_N_COLUMNS,
                               G_TYPE_STRING,   /* markup */
                               G_TYPE_STRING,   /* name */
                               G_TYPE_INT,      /* line */
                               G_TYPE_INT);     /* character */
}


/*
 * The name in plain text, and whatever the server offered about it in a dimmer
 * colour after it: the detail if there is one -- gopls puts the signature
 * there -- and the kind otherwise.
 */
static char *
make_markup (const char *name,
             const char *detail,
             int         kind)
{
    char *escaped_name = g_markup_escape_text (name, -1);
    const char *extra = detail && detail[0] ? detail : lsp_symbol_kind_name (kind);
    char *markup;

    if (!extra || !extra[0])
        return escaped_name;

    {
        char *escaped_extra = g_markup_escape_text (extra, -1);

        markup = g_strdup_printf ("%s <span foreground=\"#888888\">%s</span>",
                                  escaped_name, escaped_extra);
        g_free (escaped_extra);
    }

    g_free (escaped_name);

    return markup;
}


static void
append_symbol (GtkTreeStore        *store,
               GtkTreeIter         *parent,
               GtkTreeIter         *iter,
               const char          *name,
               const char          *detail,
               int                  kind,
               JsonObject          *range,
               GtkTextBuffer       *buffer,
               LspPositionEncoding  encoding)
{
    GtkTextIter position;
    char *markup;
    int line = 0, character = 0;

    lsp_json_get_position (lsp_json_get_object (range, "start"), &line, &character);
    lsp_position_to_iter (buffer, line, character, encoding, &position);

    markup = make_markup (name, detail, kind);

    gtk_tree_store_append (store, iter, parent);
    gtk_tree_store_set (store, iter,
                        LSP_SYMBOL_COLUMN_MARKUP, markup,
                        LSP_SYMBOL_COLUMN_NAME, name,
                        LSP_SYMBOL_COLUMN_LINE, gtk_text_iter_get_line (&position),
                        LSP_SYMBOL_COLUMN_CHARACTER, gtk_text_iter_get_line_offset (&position),
                        -1);

    g_free (markup);
}


static void
fill_document_symbols (GtkTreeStore        *store,
                       GtkTreeIter         *parent,
                       JsonArray           *array,
                       GtkTextBuffer       *buffer,
                       LspPositionEncoding  encoding,
                       int                  depth)
{
    guint i, n;

    /* A server that answers with a cycle would otherwise take the stack. */
    if (depth > 32)
        return;

    n = array ? json_array_get_length (array) : 0;

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        JsonObject *object;
        JsonObject *range;
        const char *name;
        GtkTreeIter iter;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        object = json_node_get_object (node);
        name = lsp_json_get_string (object, "name");

        if (!name)
            continue;

        /*
         * SymbolInformation puts the range inside a location and has no
         * children; DocumentSymbol has the range at the top level, a
         * selectionRange pointing at the name itself, and children.
         */
        if (lsp_json_has (object, "location"))
        {
            range = lsp_json_lookup_object (object, "location/range");

            append_symbol (store, parent, &iter, name,
                           lsp_json_get_string (object, "containerName"),
                           (int) lsp_json_get_int (object, "kind", 0),
                           range, buffer, encoding);
        }
        else
        {
            range = lsp_json_get_object (object, "selectionRange");

            if (!range)
                range = lsp_json_get_object (object, "range");

            append_symbol (store, parent, &iter, name,
                           lsp_json_get_string (object, "detail"),
                           (int) lsp_json_get_int (object, "kind", 0),
                           range, buffer, encoding);

            fill_document_symbols (store, &iter,
                                   lsp_json_get_array (object, "children"),
                                   buffer, encoding, depth + 1);
        }
    }
}


void
lsp_symbols_fill (GtkTreeStore        *store,
                  JsonNode            *result,
                  GtkTextBuffer       *buffer,
                  LspPositionEncoding  encoding)
{
    g_return_if_fail (GTK_IS_TREE_STORE (store));
    g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

    gtk_tree_store_clear (store);

    if (!result || !JSON_NODE_HOLDS_ARRAY (result))
        return;

    fill_document_symbols (store, NULL, json_node_get_array (result),
                           buffer, encoding, 0);
}
