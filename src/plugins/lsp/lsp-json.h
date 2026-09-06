/*
 *   plugins/lsp/lsp-json.h
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
 * Everything that knows about json-glib lives behind this header, so that the
 * rest of the plugin never calls it directly. The getters exist because a
 * language server may omit any optional field and several send null where the
 * specification says object; json_object_get_*_member() warns or asserts on
 * both, while these quietly return the default.
 */

#ifndef MOO_LSP_JSON_H
#define MOO_LSP_JSON_H

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

JsonNode    *lsp_json_parse             (const char     *data,
                                         gssize          len,
                                         GError        **error);
char        *lsp_json_to_string         (JsonNode       *node,
                                         gsize          *len);
char        *lsp_json_object_to_string  (JsonObject     *object,
                                         gsize          *len);

/* Reading. Every one of these tolerates a NULL object. */
gboolean     lsp_json_has               (JsonObject     *object,
                                         const char     *member);
JsonNode    *lsp_json_get_node          (JsonObject     *object,
                                         const char     *member);
const char  *lsp_json_get_string        (JsonObject     *object,
                                         const char     *member);
gint64       lsp_json_get_int           (JsonObject     *object,
                                         const char     *member,
                                         gint64          dflt);
gboolean     lsp_json_get_bool          (JsonObject     *object,
                                         const char     *member,
                                         gboolean        dflt);
JsonObject  *lsp_json_get_object        (JsonObject     *object,
                                         const char     *member);
JsonArray   *lsp_json_get_array         (JsonObject     *object,
                                         const char     *member);

/* Slash-separated path, e.g. "capabilities/completionProvider". */
JsonObject  *lsp_json_lookup_object     (JsonObject     *object,
                                         const char     *path);
const char  *lsp_json_lookup_string     (JsonObject     *object,
                                         const char     *path);
gint64       lsp_json_lookup_int        (JsonObject     *object,
                                         const char     *path,
                                         gint64          dflt);
gboolean     lsp_json_lookup_bool       (JsonObject     *object,
                                         const char     *path,
                                         gboolean        dflt);

/*
 * A capability the server may report either as a bare boolean or as an options
 * object: "hoverProvider": true and "hoverProvider": {} both mean yes, while
 * false, null and a missing member all mean no.
 */
gboolean     lsp_json_get_provider      (JsonObject     *object,
                                         const char     *member);

/* Writing. The object and array setters take ownership of their value. */
void         lsp_json_set_string        (JsonObject     *object,
                                         const char     *member,
                                         const char     *value);
void         lsp_json_set_int           (JsonObject     *object,
                                         const char     *member,
                                         gint64          value);
void         lsp_json_set_bool          (JsonObject     *object,
                                         const char     *member,
                                         gboolean        value);
void         lsp_json_set_object        (JsonObject     *object,
                                         const char     *member,
                                         JsonObject     *value);
void         lsp_json_set_array         (JsonObject     *object,
                                         const char     *member,
                                         JsonArray      *value);
void         lsp_json_set_node          (JsonObject     *object,
                                         const char     *member,
                                         JsonNode       *value);
void         lsp_json_set_null          (JsonObject     *object,
                                         const char     *member);

JsonArray   *lsp_json_string_array      (const char * const *strings);

/* The LSP structures used everywhere. */
JsonObject  *lsp_json_position          (int             line,
                                         int             character);
gboolean     lsp_json_get_position      (JsonObject     *object,
                                         int            *line,
                                         int            *character);
JsonObject  *lsp_json_range             (int             start_line,
                                         int             start_character,
                                         int             end_line,
                                         int             end_character);
gboolean     lsp_json_get_range         (JsonObject     *object,
                                         int            *start_line,
                                         int            *start_character,
                                         int            *end_line,
                                         int            *end_character);
JsonObject  *lsp_json_text_document     (const char     *uri);

G_END_DECLS

#endif /* MOO_LSP_JSON_H */
