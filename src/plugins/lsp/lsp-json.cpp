/*
 *   plugins/lsp/lsp-json.cpp
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

#include "plugins/lsp/lsp-json.h"

#include <string.h>


JsonNode *
lsp_json_parse (const char  *data,
                gssize       len,
                GError     **error)
{
    JsonParser *parser;
    JsonNode *root = NULL;

    g_return_val_if_fail (data != NULL, NULL);

    parser = json_parser_new ();

    if (json_parser_load_from_data (parser, data, len, error))
    {
#if JSON_CHECK_VERSION (1, 4, 0)
        root = json_parser_steal_root (parser);
#else
        JsonNode *node = json_parser_get_root (parser);
        root = node ? json_node_copy (node) : NULL;
#endif
    }

    g_object_unref (parser);

    return root;
}


char *
lsp_json_to_string (JsonNode *node,
                    gsize    *len)
{
    JsonGenerator *generator;
    char *text;

    g_return_val_if_fail (node != NULL, NULL);

    generator = json_generator_new ();
    json_generator_set_root (generator, node);
    text = json_generator_to_data (generator, len);
    g_object_unref (generator);

    return text;
}


char *
lsp_json_object_to_string (JsonObject *object,
                           gsize      *len)
{
    JsonNode *node;
    char *text;

    g_return_val_if_fail (object != NULL, NULL);

    node = json_node_new (JSON_NODE_OBJECT);
    json_node_set_object (node, object);
    text = lsp_json_to_string (node, len);
    json_node_free (node);

    return text;
}


/*
 * json_object_get_member() is the only lookup that neither warns nor asserts
 * on a missing member, so every getter below goes through it.
 */
JsonNode *
lsp_json_get_node (JsonObject *object,
                   const char *member)
{
    if (!object || !member)
        return NULL;

    return json_object_get_member (object, member);
}


gboolean
lsp_json_has (JsonObject *object,
              const char *member)
{
    JsonNode *node = lsp_json_get_node (object, member);
    return node != NULL && !JSON_NODE_HOLDS_NULL (node);
}


const char *
lsp_json_get_string (JsonObject *object,
                     const char *member)
{
    JsonNode *node = lsp_json_get_node (object, member);

    if (!node || !JSON_NODE_HOLDS_VALUE (node))
        return NULL;
    if (json_node_get_value_type (node) != G_TYPE_STRING)
        return NULL;

    return json_node_get_string (node);
}


gint64
lsp_json_get_int (JsonObject *object,
                  const char *member,
                  gint64      dflt)
{
    JsonNode *node = lsp_json_get_node (object, member);
    GType type;

    if (!node || !JSON_NODE_HOLDS_VALUE (node))
        return dflt;

    /*
     * A JSON number with no fractional part still arrives as a double when the
     * server wrote it as 1.0, and json_node_get_int() converts either way.
     */
    type = json_node_get_value_type (node);

    if (type != G_TYPE_INT64 && type != G_TYPE_DOUBLE)
        return dflt;

    return json_node_get_int (node);
}


gboolean
lsp_json_get_bool (JsonObject *object,
                   const char *member,
                   gboolean    dflt)
{
    JsonNode *node = lsp_json_get_node (object, member);

    if (!node || !JSON_NODE_HOLDS_VALUE (node))
        return dflt;
    if (json_node_get_value_type (node) != G_TYPE_BOOLEAN)
        return dflt;

    return json_node_get_boolean (node);
}


JsonObject *
lsp_json_get_object (JsonObject *object,
                     const char *member)
{
    JsonNode *node = lsp_json_get_node (object, member);

    if (!node || !JSON_NODE_HOLDS_OBJECT (node))
        return NULL;

    return json_node_get_object (node);
}


JsonArray *
lsp_json_get_array (JsonObject *object,
                    const char *member)
{
    JsonNode *node = lsp_json_get_node (object, member);

    if (!node || !JSON_NODE_HOLDS_ARRAY (node))
        return NULL;

    return json_node_get_array (node);
}


/*
 * Walks every path component but the last, each of which has to be an object.
 * Returns the parent and stores the last component in *last.
 */
static JsonObject *
lookup_parent (JsonObject  *object,
               const char  *path,
               char       **last)
{
    char **parts;
    JsonObject *parent = object;
    guint i, n;

    *last = NULL;

    if (!object || !path)
        return NULL;

    parts = g_strsplit (path, "/", -1);
    n = g_strv_length (parts);

    if (n == 0)
    {
        g_strfreev (parts);
        return NULL;
    }

    for (i = 0; parent != NULL && i + 1 < n; ++i)
        parent = lsp_json_get_object (parent, parts[i]);

    if (parent)
        *last = g_strdup (parts[n - 1]);

    g_strfreev (parts);

    return parent;
}


JsonObject *
lsp_json_lookup_object (JsonObject *object,
                        const char *path)
{
    char *last = NULL;
    JsonObject *parent = lookup_parent (object, path, &last);
    JsonObject *result = parent ? lsp_json_get_object (parent, last) : NULL;

    g_free (last);

    return result;
}


const char *
lsp_json_lookup_string (JsonObject *object,
                        const char *path)
{
    char *last = NULL;
    JsonObject *parent = lookup_parent (object, path, &last);
    const char *result = parent ? lsp_json_get_string (parent, last) : NULL;

    g_free (last);

    return result;
}


gint64
lsp_json_lookup_int (JsonObject *object,
                     const char *path,
                     gint64      dflt)
{
    char *last = NULL;
    JsonObject *parent = lookup_parent (object, path, &last);
    gint64 result = parent ? lsp_json_get_int (parent, last, dflt) : dflt;

    g_free (last);

    return result;
}


gboolean
lsp_json_lookup_bool (JsonObject *object,
                      const char *path,
                      gboolean    dflt)
{
    char *last = NULL;
    JsonObject *parent = lookup_parent (object, path, &last);
    gboolean result = parent ? lsp_json_get_bool (parent, last, dflt) : dflt;

    g_free (last);

    return result;
}


gboolean
lsp_json_get_provider (JsonObject *object,
                       const char *member)
{
    JsonNode *node = lsp_json_get_node (object, member);

    if (!node)
        return FALSE;

    if (JSON_NODE_HOLDS_OBJECT (node))
        return TRUE;

    if (JSON_NODE_HOLDS_VALUE (node) &&
        json_node_get_value_type (node) == G_TYPE_BOOLEAN)
            return json_node_get_boolean (node);

    return FALSE;
}


void
lsp_json_set_string (JsonObject *object,
                     const char *member,
                     const char *value)
{
    g_return_if_fail (object != NULL && member != NULL);

    if (value)
        json_object_set_string_member (object, member, value);
    else
        json_object_set_null_member (object, member);
}


void
lsp_json_set_int (JsonObject *object,
                  const char *member,
                  gint64      value)
{
    g_return_if_fail (object != NULL && member != NULL);
    json_object_set_int_member (object, member, value);
}


void
lsp_json_set_bool (JsonObject *object,
                   const char *member,
                   gboolean    value)
{
    g_return_if_fail (object != NULL && member != NULL);
    json_object_set_boolean_member (object, member, value ? TRUE : FALSE);
}


void
lsp_json_set_object (JsonObject *object,
                     const char *member,
                     JsonObject *value)
{
    g_return_if_fail (object != NULL && member != NULL);

    if (value)
        json_object_set_object_member (object, member, value);
    else
        json_object_set_null_member (object, member);
}


void
lsp_json_set_array (JsonObject *object,
                    const char *member,
                    JsonArray  *value)
{
    g_return_if_fail (object != NULL && member != NULL);

    if (value)
        json_object_set_array_member (object, member, value);
    else
        json_object_set_null_member (object, member);
}


void
lsp_json_set_node (JsonObject *object,
                   const char *member,
                   JsonNode   *value)
{
    g_return_if_fail (object != NULL && member != NULL);

    if (value)
        json_object_set_member (object, member, value);
    else
        json_object_set_null_member (object, member);
}


void
lsp_json_set_null (JsonObject *object,
                   const char *member)
{
    g_return_if_fail (object != NULL && member != NULL);
    json_object_set_null_member (object, member);
}


JsonArray *
lsp_json_string_array (const char * const *strings)
{
    JsonArray *array = json_array_new ();
    guint i;

    for (i = 0; strings && strings[i]; ++i)
        json_array_add_string_element (array, strings[i]);

    return array;
}


JsonObject *
lsp_json_position (int line,
                   int character)
{
    JsonObject *object = json_object_new ();

    lsp_json_set_int (object, "line", line);
    lsp_json_set_int (object, "character", character);

    return object;
}


gboolean
lsp_json_get_position (JsonObject *object,
                       int        *line,
                       int        *character)
{
    if (!object)
        return FALSE;

    if (!lsp_json_has (object, "line") || !lsp_json_has (object, "character"))
        return FALSE;

    if (line)
        *line = (int) lsp_json_get_int (object, "line", 0);
    if (character)
        *character = (int) lsp_json_get_int (object, "character", 0);

    return TRUE;
}


JsonObject *
lsp_json_range (int start_line,
                int start_character,
                int end_line,
                int end_character)
{
    JsonObject *object = json_object_new ();

    lsp_json_set_object (object, "start", lsp_json_position (start_line, start_character));
    lsp_json_set_object (object, "end", lsp_json_position (end_line, end_character));

    return object;
}


gboolean
lsp_json_get_range (JsonObject *object,
                    int        *start_line,
                    int        *start_character,
                    int        *end_line,
                    int        *end_character)
{
    if (!object)
        return FALSE;

    if (!lsp_json_get_position (lsp_json_get_object (object, "start"),
                                start_line, start_character))
        return FALSE;

    if (!lsp_json_get_position (lsp_json_get_object (object, "end"),
                                end_line, end_character))
        return FALSE;

    return TRUE;
}


JsonObject *
lsp_json_text_document (const char *uri)
{
    JsonObject *object = json_object_new ();

    lsp_json_set_string (object, "uri", uri);

    return object;
}
