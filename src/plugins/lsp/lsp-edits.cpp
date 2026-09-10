/*
 *   plugins/lsp/lsp-edits.cpp
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

#include "plugins/lsp/lsp-edits.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-navigate.h"

#include "mooedit/mooeditconfig.h"
#include "mooedit/mooeditor.h"
#include "mooutils/moodialogs.h"
#include "mooutils/mooi18n.h"

#include <string.h>


/**********************************************************************/
/* Reading a workspace edit
 */

static LspTextEdit *
text_edit_new (const char *path,
               JsonObject *object)
{
    LspTextEdit *edit;
    int start_line = 0, start_character = 0;
    int end_line = 0, end_character = 0;
    const char *new_text;

    if (!lsp_json_get_range (lsp_json_get_object (object, "range"),
                             &start_line, &start_character,
                             &end_line, &end_character))
        return NULL;

    new_text = lsp_json_get_string (object, "newText");
    if (!new_text)
        return NULL;

    edit = g_new0 (LspTextEdit, 1);
    edit->path = g_strdup (path);
    edit->start_line = start_line;
    edit->start_character = start_character;
    edit->end_line = end_line;
    edit->end_character = end_character;
    edit->new_text = g_strdup (new_text);

    return edit;
}


void
lsp_text_edit_free (LspTextEdit *edit)
{
    if (!edit)
        return;

    g_free (edit->path);
    g_free (edit->new_text);
    g_free (edit);
}


void
lsp_text_edits_free (GSList *edits)
{
    g_slist_free_full (edits, (GDestroyNotify) lsp_text_edit_free);
}


/*
 * By file, and within a file from the back. Applying an edit moves every
 * position after it in that file, so the ranges a server gave are only all
 * correct at once if the last one is applied first -- two names on one line
 * is enough to see it go wrong.
 *
 * Grouping by file matters as much: each file is opened once and changed in
 * one undo step, and that is only possible when its edits are together.
 */
static int
compare_edits (gconstpointer a,
               gconstpointer b)
{
    const LspTextEdit *first = (const LspTextEdit*) a;
    const LspTextEdit *second = (const LspTextEdit*) b;
    int by_path = strcmp (first->path, second->path);

    if (by_path != 0)
        return by_path;

    if (first->start_line != second->start_line)
        return second->start_line - first->start_line;

    return second->start_character - first->start_character;
}


static GSList *
edits_of_array (GSList     *edits,
                const char *path,
                JsonArray  *array)
{
    guint i, n;

    if (!path || !array)
        return edits;

    n = json_array_get_length (array);

    for (i = 0; i < n; ++i)
    {
        JsonNode *node = json_array_get_element (array, i);
        LspTextEdit *edit;

        if (!node || !JSON_NODE_HOLDS_OBJECT (node))
            continue;

        edit = text_edit_new (path, json_node_get_object (node));

        if (edit)
            edits = g_slist_prepend (edits, edit);
    }

    return edits;
}


GSList *
lsp_text_edits_parse (JsonNode   *result,
                      const char *path)
{
    if (!result || !JSON_NODE_HOLDS_ARRAY (result) || !path)
        return NULL;

    return g_slist_sort (edits_of_array (NULL, path, json_node_get_array (result)),
                         compare_edits);
}


GSList *
lsp_workspace_edit_parse (JsonNode *result)
{
    JsonObject *object;
    JsonObject *changes;
    JsonArray *document_changes;
    GSList *edits = NULL;

    if (!result || !JSON_NODE_HOLDS_OBJECT (result))
        return NULL;

    object = json_node_get_object (result);
    changes = lsp_json_get_object (object, "changes");

    if (changes)
    {
        GList *uris = json_object_get_members (changes);
        GList *l;

        for (l = uris; l != NULL; l = l->next)
        {
            const char *uri = (const char*) l->data;
            char *path = lsp_path_from_uri (uri);

            edits = edits_of_array (edits, path, lsp_json_get_array (changes, uri));
            g_free (path);
        }

        g_list_free (uris);
    }

    /*
     * The newer shape. medit does not ask for it -- it has nothing to do with
     * the file operations it can also carry, and says so in its capabilities
     * -- but a server may send it regardless, and an edit that was read as
     * "no changes" would look exactly like a rename that quietly did nothing.
     */
    document_changes = lsp_json_get_array (object, "documentChanges");

    if (document_changes)
    {
        guint i, n = json_array_get_length (document_changes);

        for (i = 0; i < n; ++i)
        {
            JsonNode *node = json_array_get_element (document_changes, i);
            JsonObject *change;
            char *path;

            if (!node || !JSON_NODE_HOLDS_OBJECT (node))
                continue;

            change = json_node_get_object (node);

            /* A create, rename or delete of a file, which medit does not do. */
            if (lsp_json_has (change, "kind"))
                continue;

            path = lsp_path_from_uri (lsp_json_lookup_string (change, "textDocument/uri"));
            edits = edits_of_array (edits, path, lsp_json_get_array (change, "edits"));
            g_free (path);
        }
    }

    return g_slist_sort (edits, compare_edits);
}


/**********************************************************************/
/* Applying it
 */

static void
apply_edit (GtkTextBuffer       *buffer,
            LspTextEdit         *edit,
            LspPositionEncoding  encoding)
{
    GtkTextIter start, end;

    if (!lsp_position_to_iter (buffer, edit->start_line, edit->start_character,
                               encoding, &start) ||
        !lsp_position_to_iter (buffer, edit->end_line, edit->end_character,
                               encoding, &end))
        return;

    gtk_text_buffer_delete (buffer, &start, &end);

    if (edit->new_text && edit->new_text[0])
        gtk_text_buffer_insert (buffer, &start, edit->new_text, -1);
}


/*
 * Nothing is saved here. A rename reaches files the user never opened, and
 * leaving them open and modified is what makes it something that can be looked
 * at, undone, or thrown away -- which is more than can be said for a set of
 * files rewritten on disk.
 */
void
lsp_text_edits_apply (MooEditWindow       *window,
                      GSList              *edits,
                      LspPositionEncoding  encoding)
{
    MooEditor *editor = moo_edit_window_get_editor (window);
    GSList *l = edits;

    while (l != NULL)
    {
        LspTextEdit *first = (LspTextEdit*) l->data;
        MooEdit *doc = moo_editor_open_path (editor, first->path, NULL, -1, window);
        GtkTextBuffer *buffer = doc ? moo_edit_get_buffer (doc) : NULL;

        if (buffer)
            gtk_text_buffer_begin_user_action (buffer);

        /* Every edit of this file, which the sort put together. */
        while (l != NULL && strcmp (((LspTextEdit*) l->data)->path, first->path) == 0)
        {
            if (buffer)
                apply_edit (buffer, (LspTextEdit*) l->data, encoding);

            l = l->next;
        }

        if (buffer)
            gtk_text_buffer_end_user_action (buffer);
    }
}


/**********************************************************************/
/* Asking for the new name
 */

/*
 * The word a position is in: letters, digits and the underscore, the same
 * rule the completion prefix uses. Offered as the name to change, because a
 * rename usually keeps most of what is there.
 */
static char *
word_at (const GtkTextIter *where)
{
    GtkTextIter start = *where;
    GtkTextIter end = *where;

    while (!gtk_text_iter_starts_line (&start))
    {
        GtkTextIter back = start;
        gunichar c;

        gtk_text_iter_backward_char (&back);
        c = gtk_text_iter_get_char (&back);

        if (!g_unichar_isalnum (c) && c != '_')
            break;

        start = back;
    }

    while (!gtk_text_iter_ends_line (&end))
    {
        gunichar c = gtk_text_iter_get_char (&end);

        if (!g_unichar_isalnum (c) && c != '_')
            break;

        gtk_text_iter_forward_char (&end);
    }

    if (gtk_text_iter_equal (&start, &end))
        return NULL;

    return gtk_text_iter_get_text (&start, &end);
}


static char *
ask_for_name (MooEditWindow *window,
              const char    *old_name)
{
    GtkWidget *dialog;
    GtkWidget *box;
    GtkWidget *label;
    GtkWidget *entry;
    char *text;
    char *name = NULL;

    dialog = gtk_dialog_new_with_buttons (_("Rename"), GTK_WINDOW (window),
                                          (GtkDialogFlags) (GTK_DIALOG_MODAL |
                                                            GTK_DIALOG_DESTROY_WITH_PARENT),
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          _("_Rename"), GTK_RESPONSE_OK,
                                          (const char*) NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);

    text = old_name && old_name[0] ? g_strdup_printf (_("New name for '%s':"), old_name)
                                   : g_strdup (_("New name:"));
    label = gtk_label_new (text);
    g_free (text);

    /* gtk_misc_set_alignment() is what GTK+2 has and what GTK+3 deprecated;
       the tree is trying to have fewer of those, not more. */
#if GTK_CHECK_VERSION(3,16,0)
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
#else
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
#endif

    entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (entry), old_name ? old_name : "");
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

    /* Selected, so that typing replaces it and Enter keeps it. */
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

    box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_set_border_width (GTK_CONTAINER (box), 6);
    gtk_box_set_spacing (GTK_BOX (box), 6);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);

    gtk_widget_show_all (box);
    gtk_widget_grab_focus (entry);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        name = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));

    gtk_widget_destroy (dialog);

    /* A name with nothing in it is a cancelled rename that lost its Escape. */
    if (name && !name[0])
    {
        g_free (name);
        name = NULL;
    }

    return name;
}


/**********************************************************************/
/* Asking the server
 */

typedef struct {
    MooEditWindow      *window;     /* weak */
    LspPositionEncoding encoding;   /* of the server that was asked */
} LspRenameRequest;


static void
rename_request_free (gpointer data)
{
    LspRenameRequest *request = (LspRenameRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);

    g_free (request);
}


static void
rename_reply (JsonNode   *result,
              JsonObject *error,
              gpointer    data)
{
    LspRenameRequest *request = (LspRenameRequest*) data;
    MooEditWindow *window = request->window;
    GSList *edits;

    if (!window || !MOO_IS_EDIT_WINDOW (window))
        return;

    /*
     * A rename that does nothing has to say so. The user filled in a dialog
     * and pressed a button; a client that swallowed the refusal would leave
     * them looking at a document that did not change and no reason why.
     */
    if (error)
    {
        const char *message = lsp_json_get_string (error, "message");

        moo_error_dialog (_("Rename failed"),
                          message && message[0] ? message : NULL,
                          GTK_WIDGET (window));
        return;
    }

    edits = lsp_workspace_edit_parse (result);

    if (!edits)
    {
        moo_error_dialog (_("Rename failed"),
                          _("The server had nothing to change."),
                          GTK_WIDGET (window));
        return;
    }

    lsp_text_edits_apply (window, edits, request->encoding);
    lsp_text_edits_free (edits);
}


void
lsp_rename (MooEditWindow *window,
            MooEditView   *view)
{
    LspDoc *ldoc = NULL;
    LspServer *server;
    LspRenameRequest *request;
    JsonObject *params;
    GtkTextIter iter;
    char *old_name;
    char *new_name;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));

    if (!lsp_can_ask (window, "textDocument/rename"))
        return;

    if (!lsp_ask_position (window, view, &ldoc, &iter, &line, &character))
        return;

    old_name = word_at (&iter);
    new_name = ask_for_name (window, old_name);
    g_free (old_name);

    if (!new_name)
        return;

    /*
     * After the dialog rather than before it: the position was taken before
     * the user was asked anything, and the document cannot have changed while
     * a modal dialog was up, so this is only about the server having the text.
     */
    lsp_doc_flush (ldoc);

    server = lsp_doc_get_server (ldoc);

    params = lsp_position_params (ldoc, line, character);
    lsp_json_set_string (params, "newName", new_name);

    request = g_new0 (LspRenameRequest, 1);
    request->window = window;
    request->encoding = lsp_server_get_position_encoding (server);
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &request->window);

    lsp_server_call (server, "textDocument/rename", params,
                     rename_reply, request, rename_request_free);

    g_free (new_name);
}


/**********************************************************************/
/* Laying a file out
 */

typedef struct {
    MooEditWindow      *window;     /* weak */
    LspPositionEncoding encoding;   /* of the server that was asked */
    char               *path;       /* the file the edits are over */
} LspFormatRequest;


static void
format_request_free (gpointer data)
{
    LspFormatRequest *request = (LspFormatRequest*) data;

    if (request->window)
        g_object_remove_weak_pointer (G_OBJECT (request->window),
                                      (gpointer*) &request->window);

    g_free (request->path);
    g_free (request);
}


static void
format_reply (JsonNode   *result,
              JsonObject *error,
              gpointer    data)
{
    LspFormatRequest *request = (LspFormatRequest*) data;
    MooEditWindow *window = request->window;
    GSList *edits;

    if (!window || !MOO_IS_EDIT_WINDOW (window))
        return;

    if (error)
    {
        const char *message = lsp_json_get_string (error, "message");

        moo_error_dialog (_("Formatting failed"),
                          message && message[0] ? message : NULL,
                          GTK_WIDGET (window));
        return;
    }

    /*
     * An empty answer means the file is already laid out the way the server
     * would lay it out, which is not a failure and is not worth a dialog: the
     * document simply does not change.
     */
    edits = lsp_text_edits_parse (result, request->path);

    lsp_text_edits_apply (window, edits, request->encoding);
    lsp_text_edits_free (edits);
}


/*
 * What medit would do itself, so that a server does not undo it. tabSize and
 * insertSpaces are what the document indents with, and the two that follow are
 * the settings medit applies when it saves -- a formatter told nothing about
 * them puts back the trailing whitespace the editor is about to strip.
 */
static JsonObject *
formatting_options (MooEdit *doc)
{
    JsonObject *options = json_object_new ();
    MooEditConfig *config = doc->config;
    guint width = config ? moo_edit_config_get_uint (config, "indent-width") : 8;

    lsp_json_set_int (options, "tabSize", width > 0 ? width : 8);
    lsp_json_set_bool (options, "insertSpaces",
                       config ? !moo_edit_config_get_bool (config, "indent-use-tabs")
                              : FALSE);
    lsp_json_set_bool (options, "trimTrailingWhitespace",
                       config ? moo_edit_config_get_bool (config, "strip") : FALSE);
    lsp_json_set_bool (options, "insertFinalNewline",
                       config ? moo_edit_config_get_bool (config, "add-newline") : FALSE);

    return options;
}


void
lsp_format (MooEditWindow *window)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    LspFormatRequest *request;
    JsonObject *params;
    char *path;

    g_return_if_fail (MOO_IS_EDIT_WINDOW (window));

    doc = moo_edit_window_get_active_doc (window);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    /*
     * The capability by name rather than through lsp_can_ask(), which derives
     * one from the method: this is the method where that does not work, the
     * capability for "textDocument/formatting" being
     * documentFormattingProvider rather than formattingProvider.
     */
    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "documentFormattingProvider"))
        return;

    path = lsp_path_from_uri (lsp_doc_get_uri (ldoc));

    if (!path)
        return;

    /* The server formats the text it has, so it had better have this one. */
    lsp_doc_flush (ldoc);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "options", formatting_options (doc));

    request = g_new0 (LspFormatRequest, 1);
    request->window = window;
    request->encoding = lsp_server_get_position_encoding (server);
    request->path = path;
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer*) &request->window);

    lsp_server_call (server, "textDocument/formatting", params,
                     format_reply, request, format_request_free);
}
