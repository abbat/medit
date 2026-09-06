/*
 *   plugins/lsp/lsp-diagnostics.cpp
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

#include "plugins/lsp/lsp-diagnostics.h"
#include "plugins/lsp/lsp-doc.h"

#include "mooedit/mooedit.h"
#include "mooedit/mooeditview.h"
#include "mooedit/mootextbuffer.h"
#include "mooedit/moolinemark.h"
#include "mooutils/mooi18n.h"

#include <string.h>

/* Where the marks put on a document are remembered, to take them off again. */
#define LSP_MARKS_QUARK "moo-lsp-diagnostic-marks"

typedef struct {
    int          severity;
    const char  *tag_name;
    const char  *stock_id;
    const char  *color;     /* the underline colour, gtk3 only */
} LspSeverityInfo;

static const LspSeverityInfo severity_info[] = {
    { LSP_SEVERITY_ERROR,       "moo-lsp-error",       GTK_STOCK_DIALOG_ERROR,    "#c01c28" },
    { LSP_SEVERITY_WARNING,     "moo-lsp-warning",     GTK_STOCK_DIALOG_WARNING,  "#e5a50a" },
    { LSP_SEVERITY_INFORMATION, "moo-lsp-information", GTK_STOCK_DIALOG_INFO,     "#1c71d8" },
    { LSP_SEVERITY_HINT,        "moo-lsp-hint",        GTK_STOCK_DIALOG_INFO,     "#77767b" }
};


static const LspSeverityInfo *
get_severity_info (int severity)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (severity_info); ++i)
        if (severity_info[i].severity == severity)
            return &severity_info[i];

    /* A server may leave severity out, and the specification then leaves it to
       the client; an error is the reading that cannot be safely ignored. */
    return &severity_info[0];
}


const char *
lsp_severity_name (int severity)
{
    /* With a context: on its own "error" is too short a word to translate. */
    switch (severity)
    {
        case LSP_SEVERITY_WARNING:
            return C_("diagnostic severity", "warning");
        case LSP_SEVERITY_INFORMATION:
            return C_("diagnostic severity", "information");
        case LSP_SEVERITY_HINT:
            return C_("diagnostic severity", "hint");
        case LSP_SEVERITY_ERROR:
        default:
            return C_("diagnostic severity", "error");
    }
}


/**********************************************************************/
/* Parsing
 */

static void
lsp_diagnostic_free (LspDiagnostic *diagnostic)
{
    if (!diagnostic)
        return;

    g_free (diagnostic->message);
    g_free (diagnostic->source);
    g_free (diagnostic->code);
    g_free (diagnostic);
}


void
lsp_diagnostics_free (GSList *list)
{
    g_slist_free_full (list, (GDestroyNotify) lsp_diagnostic_free);
}


/* The code member is a number as often as it is a string. */
static char *
get_code (JsonObject *object)
{
    JsonNode *node = lsp_json_get_node (object, "code");
    GType type;

    if (!node || !JSON_NODE_HOLDS_VALUE (node))
        return NULL;

    type = json_node_get_value_type (node);

    if (type == G_TYPE_STRING)
        return g_strdup (json_node_get_string (node));

    if (type == G_TYPE_INT64 || type == G_TYPE_DOUBLE)
        return g_strdup_printf ("%" G_GINT64_FORMAT, json_node_get_int (node));

    return NULL;
}


GSList *
lsp_diagnostics_parse (JsonArray *array)
{
    GSList *list = NULL;
    guint i, n;

    if (!array)
        return NULL;

    n = json_array_get_length (array);

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        JsonObject *object;
        LspDiagnostic *diagnostic;
        int start_line = 0, start_character = 0, end_line = 0, end_character = 0;
        const char *message;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        object = json_node_get_object (node);
        message = lsp_json_get_string (object, "message");

        if (!message)
            continue;

        if (!lsp_json_get_range (lsp_json_get_object (object, "range"),
                                 &start_line, &start_character,
                                 &end_line, &end_character))
            continue;

        diagnostic = g_new0 (LspDiagnostic, 1);
        diagnostic->severity = (int) lsp_json_get_int (object, "severity",
                                                       LSP_SEVERITY_ERROR);
        diagnostic->start_line = start_line;
        diagnostic->start_character = start_character;
        diagnostic->end_line = end_line;
        diagnostic->end_character = end_character;
        diagnostic->message = g_strdup (message);
        diagnostic->source = g_strdup (lsp_json_get_string (object, "source"));
        diagnostic->code = get_code (object);

        list = g_slist_prepend (list, diagnostic);
    }

    return g_slist_reverse (list);
}


/**********************************************************************/
/* Showing
 */

static GtkTextTag *
get_tag (GtkTextBuffer         *buffer,
         const LspSeverityInfo *info)
{
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup (table, info->tag_name);

    if (tag)
        return tag;

    /*
     * PANGO_UNDERLINE_ERROR is the squiggly one, and it is what every other
     * editor draws under a diagnostic.
     */
    tag = gtk_text_buffer_create_tag (buffer, info->tag_name,
                                      "underline", PANGO_UNDERLINE_ERROR,
                                      (const char*) NULL);

#if GTK_CHECK_VERSION(3,16,0)
    /*
     * Only GTK+3 can colour an underline separately from the text, so on
     * GTK+2 every severity gets the same squiggle and the margin icon and the
     * pane are what tell them apart.
     */
    {
        GdkRGBA rgba;

        if (gdk_rgba_parse (&rgba, info->color))
            g_object_set (tag, "underline-rgba", &rgba, (const char*) NULL);
    }
#endif

    return tag;
}


static void
remove_marks (MooEdit *doc)
{
    GSList *marks = (GSList*) g_object_get_data (G_OBJECT (doc), LSP_MARKS_QUARK);
    MooTextBuffer *buffer;
    GSList *l;

    if (!marks)
        return;

    buffer = MOO_TEXT_BUFFER (moo_edit_get_buffer (doc));

    for (l = marks; l != NULL; l = l->next)
    {
        MooLineMark *mark = MOO_LINE_MARK (l->data);

        if (!moo_line_mark_get_deleted (mark))
            moo_text_buffer_delete_line_mark (buffer, mark);

        g_object_unref (mark);
    }

    g_slist_free (marks);
    g_object_set_data (G_OBJECT (doc), LSP_MARKS_QUARK, NULL);
}


static void
remove_tags (MooEdit *doc)
{
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
    GtkTextIter start, end;
    guint i;

    gtk_text_buffer_get_bounds (buffer, &start, &end);

    for (i = 0; i < G_N_ELEMENTS (severity_info); ++i)
    {
        GtkTextTag *tag = gtk_text_tag_table_lookup (table, severity_info[i].tag_name);

        if (tag)
            gtk_text_buffer_remove_tag (buffer, tag, &start, &end);
    }
}


void
lsp_diagnostics_clear (MooEdit *doc)
{
    g_return_if_fail (MOO_IS_EDIT (doc));

    remove_tags (doc);
    remove_marks (doc);
}


/*
 * One icon per line, for the worst thing on it: several diagnostics on the
 * same line are common, and stacking icons in a margin one icon wide is not
 * possible anyway.
 */
static void
add_marks (MooEdit *doc,
           GSList  *list)
{
    MooTextBuffer *buffer = MOO_TEXT_BUFFER (moo_edit_get_buffer (doc));
    GHashTable *worst = g_hash_table_new (g_direct_hash, g_direct_equal);
    GHashTableIter iter;
    gpointer key, value;
    GSList *marks = NULL, *l;
    MooEditViewArray *views;
    guint i;
    int line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));

    for (l = list; l != NULL; l = l->next)
    {
        LspDiagnostic *diagnostic = (LspDiagnostic*) l->data;
        gpointer line_key;
        gpointer previous;

        if (diagnostic->start_line < 0 || diagnostic->start_line >= line_count)
            continue;

        line_key = GINT_TO_POINTER (diagnostic->start_line);
        previous = g_hash_table_lookup (worst, line_key);

        /* Lower severity numbers are worse: error is 1, hint is 4. */
        if (!previous || ((LspDiagnostic*) previous)->severity > diagnostic->severity)
            g_hash_table_insert (worst, line_key, diagnostic);
    }

    if (g_hash_table_size (worst) == 0)
    {
        g_hash_table_destroy (worst);
        return;
    }

    /*
     * The margin is hidden until something wants to be in it, the same way
     * adding a bookmark turns it on. It has to be on before the marks arrive,
     * or the view has nowhere to draw them.
     */
    views = moo_edit_get_views (doc);

    for (i = 0; i < moo_edit_view_array_get_size (views); ++i)
        g_object_set (views->elms[i], "show-line-marks", TRUE, (const char*) NULL);

    moo_edit_view_array_free (views);

    g_hash_table_iter_init (&iter, worst);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        LspDiagnostic *diagnostic = (LspDiagnostic*) value;
        const LspSeverityInfo *info = get_severity_info (diagnostic->severity);
        /*
         * MooLineMark:visible defaults to FALSE, and a mark that is not
         * visible when the view picks it up never gets drawn -- that is what
         * _moo_line_mark_set_pretty() keys on. MooEditBookmark sets it in its
         * own init, which is why bookmarks appear without anyone asking.
         */
        MooLineMark *mark = MOO_LINE_MARK (g_object_new (MOO_TYPE_LINE_MARK,
                                                         "visible", TRUE,
                                                         (const char*) NULL));

        moo_line_mark_set_stock_id (mark, info->stock_id);
        moo_text_buffer_add_line_mark (buffer, mark, GPOINTER_TO_INT (key));

        marks = g_slist_prepend (marks, mark);
    }

    g_hash_table_destroy (worst);

    g_object_set_data (G_OBJECT (doc), LSP_MARKS_QUARK, marks);
}


void
lsp_diagnostics_apply (MooEdit             *doc,
                       GSList              *list,
                       LspPositionEncoding  encoding)
{
    GtkTextBuffer *buffer;
    GSList *l;

    g_return_if_fail (MOO_IS_EDIT (doc));

    lsp_diagnostics_clear (doc);

    if (!list)
        return;

    buffer = moo_edit_get_buffer (doc);

    for (l = list; l != NULL; l = l->next)
    {
        LspDiagnostic *diagnostic = (LspDiagnostic*) l->data;
        const LspSeverityInfo *info = get_severity_info (diagnostic->severity);
        GtkTextIter start, end;

        lsp_position_to_iter (buffer, diagnostic->start_line,
                              diagnostic->start_character, encoding, &start);
        lsp_position_to_iter (buffer, diagnostic->end_line,
                              diagnostic->end_character, encoding, &end);

        /*
         * A zero-width range is what a server sends for something missing
         * rather than something wrong -- an unclosed brace, a missing import.
         * There is nothing to underline, so the whole word under it is taken,
         * and failing that the rest of the line, so that the mark in the
         * margin is not the only sign of it.
         */
        if (gtk_text_iter_equal (&start, &end))
        {
            if (!gtk_text_iter_ends_line (&end))
            {
                if (!gtk_text_iter_forward_word_end (&end) ||
                    gtk_text_iter_get_line (&end) != gtk_text_iter_get_line (&start))
                {
                    end = start;
                    gtk_text_iter_forward_to_line_end (&end);
                }
            }
        }

        if (gtk_text_iter_compare (&start, &end) < 0)
            gtk_text_buffer_apply_tag (buffer, get_tag (buffer, info), &start, &end);
    }

    add_marks (doc, list);
}
