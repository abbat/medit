/*
 *   plugins/lsp/lsp-completion.cpp
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

#include "plugins/lsp/lsp-completion.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-plugin.h"
#include "plugins/lsp/lsp-symbols.h"

#include "mooedit/mootextview.h"
#include "mooutils/mooprefs.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

#define LSP_COMPLETION_MAX_ITEMS 200
#define LSP_COMPLETION_ROWS 10

enum {
    COLUMN_MARKUP,
    COLUMN_TEXT,        /* what goes into the buffer */
    COLUMN_FILTER,      /* what the typed prefix is matched against */
    N_COLUMNS
};

typedef struct {
    char *label;
    char *text;
    char *filter;
    char *sort;
    char *detail;
    int   kind;
    /* An explicit range from a textEdit, in the document's own coordinates. */
    gboolean have_range;
    int   start_line, start_character;
    int   end_line, end_character;
} LspCompletionItem;

static struct {
    GtkWidget    *window;
    GtkWidget    *tree;
    GtkListStore *store;

    MooEditView  *view;         /* weak */
    GSList       *items;        /* LspCompletionItem*, everything the server sent */

    /* Where the word being completed starts, as a mark in the buffer. */
    GtkTextMark  *start_mark;

    gint64        request;
    LspServer    *server;
    gboolean      visible;
    gboolean      inserting;    /* our own insertion, not the user's typing */
} popup;


static void     hide_popup      (void);
static void     refilter        (void);


/**********************************************************************/
/* Items
 */

static void
item_free (LspCompletionItem *item)
{
    if (!item)
        return;

    g_free (item->label);
    g_free (item->text);
    g_free (item->filter);
    g_free (item->sort);
    g_free (item->detail);
    g_free (item);
}


static void
clear_items (void)
{
    g_slist_free_full (popup.items, (GDestroyNotify) item_free);
    popup.items = NULL;
}


static int
compare_items (gconstpointer a,
               gconstpointer b)
{
    const LspCompletionItem *ia = (const LspCompletionItem*) a;
    const LspCompletionItem *ib = (const LspCompletionItem*) b;

    /* sortText is what the server wants the order to be; the label is the
       tie-break, and the fallback for servers that send neither. */
    return g_strcmp0 (ia->sort ? ia->sort : ia->label,
                      ib->sort ? ib->sort : ib->label);
}


static GSList *
parse_items (JsonNode *result)
{
    JsonArray *array = NULL;
    GSList *list = NULL;
    guint i, n;

    if (!result)
        return NULL;

    if (JSON_NODE_HOLDS_ARRAY (result))
    {
        array = json_node_get_array (result);
    }
    else if (JSON_NODE_HOLDS_OBJECT (result))
    {
        /* A CompletionList rather than a bare array. */
        array = lsp_json_get_array (json_node_get_object (result), "items");
    }

    if (!array)
        return NULL;

    n = json_array_get_length (array);

    if (n > LSP_COMPLETION_MAX_ITEMS)
        n = LSP_COMPLETION_MAX_ITEMS;

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        JsonObject *object;
        JsonObject *edit;
        LspCompletionItem *item;
        const char *label;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        object = json_node_get_object (node);
        label = lsp_json_get_string (object, "label");

        if (!label || !label[0])
            continue;

        item = g_new0 (LspCompletionItem, 1);
        item->label = g_strstrip (g_strdup (label));
        item->kind = (int) lsp_json_get_int (object, "kind", 0);
        item->detail = g_strdup (lsp_json_get_string (object, "detail"));
        item->sort = g_strdup (lsp_json_get_string (object, "sortText"));
        item->filter = g_strdup (lsp_json_get_string (object, "filterText"));

        if (!item->filter)
            item->filter = g_strdup (item->label);

        /*
         * A textEdit says both what to insert and what to replace, and is the
         * only correct answer when the two differ -- which they do for a
         * server completing in the middle of a word.
         */
        edit = lsp_json_get_object (object, "textEdit");

        if (edit)
        {
            const char *new_text = lsp_json_get_string (edit, "newText");
            JsonObject *range = lsp_json_get_object (edit, "range");

            if (!range)
                range = lsp_json_get_object (edit, "replace");

            if (new_text)
                item->text = g_strdup (new_text);

            item->have_range = lsp_json_get_range (range,
                                                   &item->start_line,
                                                   &item->start_character,
                                                   &item->end_line,
                                                   &item->end_character);
        }

        if (!item->text)
            item->text = g_strdup (lsp_json_get_string (object, "insertText"));

        if (!item->text)
            item->text = g_strdup (item->label);

        list = g_slist_prepend (list, item);
    }

    return g_slist_sort (g_slist_reverse (list), compare_items);
}


/**********************************************************************/
/* The window
 */

static void
create_popup (void)
{
    GtkWidget *swin;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    if (popup.window)
        return;

    popup.store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
                                      G_TYPE_STRING, G_TYPE_STRING);

    popup.tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (popup.store));
    g_object_unref (popup.store);

    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (popup.tree), FALSE);
    gtk_tree_view_set_enable_search (GTK_TREE_VIEW (popup.tree), FALSE);

    cell = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (NULL, cell,
                                                       "markup", COLUMN_MARKUP,
                                                       (const char*) NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (popup.tree), column);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (popup.tree));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER (swin), popup.tree);

    popup.window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_resizable (GTK_WINDOW (popup.window), FALSE);
    gtk_container_add (GTK_CONTAINER (popup.window), swin);

    gtk_widget_show_all (swin);
}


static void
place_popup (MooEditView *view)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    GdkWindow *window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
                                                  GTK_TEXT_WINDOW_TEXT);
    GtkTextIter iter;
    GdkRectangle rect;
    int window_x = 0, window_y = 0;
    int origin_x = 0, origin_y = 0;
    int height;

    if (!window)
        return;

    if (popup.start_mark)
        gtk_text_buffer_get_iter_at_mark (buffer, &iter, popup.start_mark);
    else
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));

    gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &rect);
    gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                           GTK_TEXT_WINDOW_TEXT,
                                           rect.x, rect.y + rect.height,
                                           &window_x, &window_y);
    gdk_window_get_origin (window, &origin_x, &origin_y);

    /* One row's worth of height per item, up to a screenful of ten. */
    height = LSP_COMPLETION_ROWS * (rect.height > 0 ? rect.height : 16) + 4;
    gtk_widget_set_size_request (popup.window, 320, height);

    gtk_window_move (GTK_WINDOW (popup.window), origin_x + window_x,
                     origin_y + window_y);
}


static void
select_row (int index)
{
    GtkTreePath *path = gtk_tree_path_new_from_indices (index, -1);

    gtk_tree_view_set_cursor (GTK_TREE_VIEW (popup.tree), path, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (popup.tree), path, NULL,
                                  FALSE, 0, 0);
    gtk_tree_path_free (path);
}


static void
hide_popup (void)
{
    if (popup.window)
        gtk_widget_hide (popup.window);

    if (popup.start_mark)
    {
        GtkTextBuffer *buffer = gtk_text_mark_get_buffer (popup.start_mark);

        if (buffer)
            gtk_text_buffer_delete_mark (buffer, popup.start_mark);

        popup.start_mark = NULL;
    }

    if (popup.view)
    {
        g_object_remove_weak_pointer (G_OBJECT (popup.view), (gpointer*) &popup.view);
        popup.view = NULL;
    }

    clear_items ();

    popup.visible = FALSE;
}


void
lsp_completion_cancel (void)
{
    if (popup.request && popup.server)
        lsp_server_cancel (popup.server, popup.request);

    popup.request = 0;
    popup.server = NULL;

    hide_popup ();
}


gboolean
lsp_completion_visible (void)
{
    return popup.visible;
}


/**********************************************************************/
/* Filtering and choosing
 */

/* What has been typed since the popup opened. */
static char *
current_prefix (void)
{
    GtkTextBuffer *buffer;
    GtkTextIter start, end;

    if (!popup.view || !popup.start_mark)
        return g_strdup ("");

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (popup.view));
    gtk_text_buffer_get_iter_at_mark (buffer, &start, popup.start_mark);
    gtk_text_buffer_get_iter_at_mark (buffer, &end,
                                      gtk_text_buffer_get_insert (buffer));

    if (gtk_text_iter_compare (&start, &end) > 0)
        return NULL;

    return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}


static char *
item_markup (LspCompletionItem *item)
{
    char *escaped = g_markup_escape_text (item->label, -1);
    const char *extra = item->detail && item->detail[0]
                            ? item->detail : lsp_symbol_kind_name (item->kind);
    char *markup;

    if (!extra || !extra[0])
        return escaped;

    {
        char *escaped_extra = g_markup_escape_text (extra, -1);

        markup = g_strdup_printf ("%s <span foreground=\"#888888\">%s</span>",
                                  escaped, escaped_extra);
        g_free (escaped_extra);
    }

    g_free (escaped);

    return markup;
}


static void
refilter (void)
{
    char *prefix = current_prefix ();
    GSList *l;
    int matches = 0;

    if (!prefix)
    {
        /* The cursor moved back past where the word started. */
        lsp_completion_cancel ();
        return;
    }

    gtk_list_store_clear (popup.store);

    for (l = popup.items; l != NULL; l = l->next)
    {
        LspCompletionItem *item = (LspCompletionItem*) l->data;
        GtkTreeIter iter;
        char *markup;

        if (prefix[0])
        {
            char *folded_filter = g_utf8_casefold (item->filter, -1);
            char *folded_prefix = g_utf8_casefold (prefix, -1);
            gboolean match = g_str_has_prefix (folded_filter, folded_prefix);

            g_free (folded_filter);
            g_free (folded_prefix);

            if (!match)
                continue;
        }

        markup = item_markup (item);

        gtk_list_store_append (popup.store, &iter);
        gtk_list_store_set (popup.store, &iter,
                            COLUMN_MARKUP, markup,
                            COLUMN_TEXT, item->text,
                            COLUMN_FILTER, item->filter,
                            -1);
        g_free (markup);
        matches++;
    }

    g_free (prefix);

    if (matches == 0)
    {
        lsp_completion_cancel ();
        return;
    }

    select_row (0);
}


static void
insert_selected (void)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTextBuffer *buffer;
    GtkTextIter start, end;
    char *text = NULL;
    MooEditView *view = popup.view;

    if (!view || !popup.start_mark)
    {
        lsp_completion_cancel ();
        return;
    }

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (popup.tree));

    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        lsp_completion_cancel ();
        return;
    }

    gtk_tree_model_get (model, &iter, COLUMN_TEXT, &text, -1);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_buffer_get_iter_at_mark (buffer, &start, popup.start_mark);
    gtk_text_buffer_get_iter_at_mark (buffer, &end,
                                      gtk_text_buffer_get_insert (buffer));

    /* One undo step for the whole replacement, and no re-entry through
       lsp_completion_text_inserted() while it happens. */
    popup.inserting = TRUE;
    gtk_text_buffer_begin_user_action (buffer);
    gtk_text_buffer_delete (buffer, &start, &end);

    if (text)
        gtk_text_buffer_insert (buffer, &start, text, -1);

    gtk_text_buffer_end_user_action (buffer);
    popup.inserting = FALSE;

    g_free (text);

    lsp_completion_cancel ();
}


/**********************************************************************/
/* Keys
 */

static int
selected_index (void)
{
    GtkTreePath *path = NULL;
    int index = 0;

    gtk_tree_view_get_cursor (GTK_TREE_VIEW (popup.tree), &path, NULL);

    if (path)
    {
        int *indices = gtk_tree_path_get_indices (path);

        if (indices)
            index = indices[0];

        gtk_tree_path_free (path);
    }

    return index;
}


static int
row_count (void)
{
    return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (popup.store), NULL);
}


static void
move_selection (int delta)
{
    int n = row_count ();
    int index;

    if (n == 0)
        return;

    index = selected_index () + delta;
    index = CLAMP (index, 0, n - 1);

    select_row (index);
}


gboolean
lsp_completion_key_press (MooEditView *view,
                          GdkEventKey *event)
{
    if (!popup.visible || view != popup.view)
        return FALSE;

    switch (event->keyval)
    {
        case GDK_KEY_Escape:
            lsp_completion_cancel ();
            return TRUE;

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            move_selection (-1);
            return TRUE;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            move_selection (1);
            return TRUE;

        case GDK_KEY_Page_Up:
        case GDK_KEY_KP_Page_Up:
            move_selection (-LSP_COMPLETION_ROWS);
            return TRUE;

        case GDK_KEY_Page_Down:
        case GDK_KEY_KP_Page_Down:
            move_selection (LSP_COMPLETION_ROWS);
            return TRUE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_Tab:
        case GDK_KEY_KP_Tab:
            insert_selected ();
            return TRUE;

        /* Anything that moves the cursor away ends it. */
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
        case GDK_KEY_Home:
        case GDK_KEY_End:
            lsp_completion_cancel ();
            return FALSE;

        default:
            return FALSE;
    }
}


/**********************************************************************/
/* Asking
 */

/* The start of the word the cursor is in the middle of. */
static void
find_word_start (GtkTextBuffer *buffer,
                 GtkTextIter   *iter)
{
    gtk_text_buffer_get_iter_at_mark (buffer, iter,
                                      gtk_text_buffer_get_insert (buffer));

    while (!gtk_text_iter_starts_line (iter))
    {
        GtkTextIter back = *iter;
        gunichar c;

        gtk_text_iter_backward_char (&back);
        c = gtk_text_iter_get_char (&back);

        if (!g_unichar_isalnum (c) && c != '_')
            break;

        *iter = back;
    }
}


static void
completion_reply (JsonNode   *result,
                  JsonObject *error,
                  gpointer    data)
{
    MooEditView *view = (MooEditView*) data;

    popup.request = 0;

    /* The view is the one the request was made from; popup.view holds it with
       a weak pointer, so a window closed in between leaves NULL here. */
    if (error || !popup.view || popup.view != view)
    {
        if (!popup.visible)
            lsp_completion_cancel ();
        return;
    }

    clear_items ();
    popup.items = parse_items (result);

    if (!popup.items)
    {
        lsp_completion_cancel ();
        return;
    }

    create_popup ();
    refilter ();

    if (!popup.items)
        return;

    if (row_count () == 0)
    {
        lsp_completion_cancel ();
        return;
    }

    place_popup (view);
    gtk_widget_show (popup.window);
    popup.visible = TRUE;
}


void
lsp_completion_start (MooEditView *view,
                      const char  *trigger_char)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    GtkTextBuffer *buffer;
    GtkTextIter start, cursor;
    JsonObject *params, *context;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_VIEW (view));

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_COMPLETION))
        return;

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "completionProvider"))
        return;

    lsp_completion_cancel ();

    /* The server must have the text the position refers to. */
    lsp_doc_flush (ldoc);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                      gtk_text_buffer_get_insert (buffer));

    /*
     * The word already typed stays where it is and is used to narrow the
     * list; the mark moves with the text, so an edit anywhere before it does
     * not throw the popup off.
     */
    find_word_start (buffer, &start);
    popup.start_mark = gtk_text_buffer_create_mark (buffer, NULL, &start, TRUE);

    popup.view = view;
    g_object_add_weak_pointer (G_OBJECT (view), (gpointer*) &popup.view);

    lsp_iter_to_position (&cursor, lsp_server_get_position_encoding (server),
                          &line, &character);

    context = json_object_new ();

    if (trigger_char)
    {
        lsp_json_set_int (context, "triggerKind", 2);   /* TriggerCharacter */
        lsp_json_set_string (context, "triggerCharacter", trigger_char);
    }
    else
    {
        lsp_json_set_int (context, "triggerKind", 1);   /* Invoked */
    }

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "position", lsp_json_position (line, character));
    lsp_json_set_object (params, "context", context);

    popup.server = server;
    popup.request = lsp_server_call (server, "textDocument/completion", params,
                                     completion_reply, view, NULL);
}


void
lsp_completion_text_inserted (MooEditView *view,
                              const char  *text)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    JsonObject *provider;
    JsonArray *triggers;
    guint i, n;

    if (!text || !text[0] || popup.inserting)
        return;

    if (popup.visible && view == popup.view)
    {
        refilter ();
        return;
    }

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_COMPLETION))
        return;

    /* One character at a time; a paste is not a trigger. */
    if (g_utf8_strlen (text, -1) != 1)
        return;

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server))
        return;

    provider = lsp_json_get_object (lsp_server_get_capabilities (server),
                                    "completionProvider");
    triggers = lsp_json_get_array (provider, "triggerCharacters");
    n = triggers ? json_array_get_length (triggers) : 0;

    for (i = 0; i < n; ++i)
    {
        const char *trigger = json_array_get_string_element (triggers, i);

        if (trigger && strcmp (trigger, text) == 0)
        {
            lsp_completion_start (view, trigger);
            return;
        }
    }
}
