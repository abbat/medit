/*
 *   plugins/lsp/lsp-signature.cpp
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

#include "plugins/lsp/lsp-signature.h"
#include "plugins/lsp/lsp-completion.h"
#include "plugins/lsp/lsp-manager.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooutils/mooprefs.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

/* Only one call is being typed at a time, which is why nothing here is an
   object -- the same reason the completion popup is not one. */
static struct {
    GtkWidget   *window;
    GtkWidget   *label;

    MooEditView *view;          /* weak */
    gint64       request;
    LspServer   *server;
    gboolean     visible;
} popup;


/**********************************************************************/
/* What one reply looks like
 */

static JsonObject *
array_object (JsonArray *array,
              guint      index)
{
    JsonNode *node = json_array_get_element (array, index);

    return node && JSON_NODE_HOLDS_OBJECT (node) ? json_node_get_object (node) : NULL;
}


/* Documentation is a string from one server and a MarkupContent from another. */
static const char *
documentation_of (JsonObject *object)
{
    JsonNode *node = lsp_json_get_node (object, "documentation");

    if (!node)
        return NULL;

    if (JSON_NODE_HOLDS_VALUE (node) &&
        json_node_get_value_type (node) == G_TYPE_STRING)
            return json_node_get_string (node);

    if (JSON_NODE_HOLDS_OBJECT (node))
        return lsp_json_get_string (json_node_get_object (node), "value");

    return NULL;
}


/*
 * Where a UTF-16 offset into a string lands. The protocol counts these the way
 * it counts every other position, so a parameter named by offsets into a
 * signature with anything but ASCII in it is off by one per character outside
 * the basic plane -- and a label cut in half is what that looks like.
 */
static const char *
utf16_skip (const char *text,
            gint64      units)
{
    const char *p = text;

    if (units < 0)
        return NULL;

    while (units > 0)
    {
        gunichar c;

        if (!*p)
            return NULL;

        c = g_utf8_get_char (p);
        units -= c > 0xFFFF ? 2 : 1;
        p = g_utf8_next_char (p);
    }

    /* Below zero means the offset pointed inside a surrogate pair, which is
       not a place in the text at all. */
    return units == 0 ? p : NULL;
}


static gboolean
array_int (JsonArray *array,
           guint      index,
           gint64    *out)
{
    JsonNode *node = json_array_get_element (array, index);
    GType type;

    if (!node || !JSON_NODE_HOLDS_VALUE (node))
        return FALSE;

    type = json_node_get_value_type (node);

    if (type != G_TYPE_INT64 && type != G_TYPE_DOUBLE)
        return FALSE;

    *out = json_node_get_int (node);

    return TRUE;
}


/*
 * Which part of the signature the parameter is. Its label is either the text
 * of it, which is then found inside the signature, or a pair of offsets into
 * the signature -- and a server picks whichever it likes.
 */
static gboolean
parameter_range (const char  *label,
                 JsonObject  *parameter,
                 const char **start,
                 const char **end)
{
    JsonNode *node = lsp_json_get_node (parameter, "label");

    if (!node)
        return FALSE;

    if (JSON_NODE_HOLDS_VALUE (node) &&
        json_node_get_value_type (node) == G_TYPE_STRING)
    {
        const char *text = json_node_get_string (node);
        const char *found = text && text[0] ? strstr (label, text) : NULL;

        if (!found)
            return FALSE;

        *start = found;
        *end = found + strlen (text);

        return TRUE;
    }

    if (JSON_NODE_HOLDS_ARRAY (node))
    {
        JsonArray *pair = json_node_get_array (node);
        gint64 from = 0, to = 0;
        const char *first;
        const char *last;

        if (json_array_get_length (pair) != 2 ||
            !array_int (pair, 0, &from) || !array_int (pair, 1, &to))
            return FALSE;

        first = utf16_skip (label, from);
        last = utf16_skip (label, to);

        if (!first || !last || last < first)
            return FALSE;

        *start = first;
        *end = last;

        return TRUE;
    }

    return FALSE;
}


static void
append_escaped (GString    *markup,
                const char *text,
                gssize      len)
{
    char *escaped = g_markup_escape_text (text, len);

    g_string_append (markup, escaped);
    g_free (escaped);
}


char *
lsp_signature_markup (JsonNode *result)
{
    JsonObject *object;
    JsonObject *signature;
    JsonObject *parameter = NULL;
    JsonArray *signatures;
    JsonArray *parameters;
    GString *markup;
    const char *label;
    const char *doc;
    const char *start = NULL;
    const char *end = NULL;
    gint64 active_signature;
    gint64 active_parameter;
    guint n;

    if (!result || !JSON_NODE_HOLDS_OBJECT (result))
        return NULL;

    object = json_node_get_object (result);
    signatures = lsp_json_get_array (object, "signatures");
    n = signatures ? json_array_get_length (signatures) : 0;

    if (n == 0)
        return NULL;

    active_signature = lsp_json_get_int (object, "activeSignature", 0);

    if (active_signature < 0 || (guint) active_signature >= n)
        active_signature = 0;

    signature = array_object (signatures, (guint) active_signature);
    label = signature ? lsp_json_get_string (signature, "label") : NULL;

    if (!label || !label[0])
        return NULL;

    /*
     * Since 3.16 a signature may name the parameter being typed itself, and
     * that is the only correct answer when a reply carries several overloads:
     * one activeParameter for all of them cannot be right for all of them.
     */
    active_parameter = lsp_json_get_int (signature, "activeParameter",
                                         lsp_json_get_int (object, "activeParameter", 0));

    parameters = lsp_json_get_array (signature, "parameters");

    if (parameters && active_parameter >= 0 &&
        (guint) active_parameter < json_array_get_length (parameters))
            parameter = array_object (parameters, (guint) active_parameter);

    if (parameter && !parameter_range (label, parameter, &start, &end))
    {
        start = NULL;
        end = NULL;
    }

    markup = g_string_new (NULL);

    if (start && end)
    {
        append_escaped (markup, label, start - label);
        g_string_append (markup, "<b>");
        append_escaped (markup, start, end - start);
        g_string_append (markup, "</b>");
        append_escaped (markup, end, -1);
    }
    else
    {
        append_escaped (markup, label, -1);
    }

    /* The parameter's own documentation says more than the signature's, which
       is about the call as a whole; the signature's is the fallback. */
    doc = parameter ? documentation_of (parameter) : NULL;

    if (!doc || !doc[0])
        doc = signature ? documentation_of (signature) : NULL;

    if (doc && doc[0])
    {
        g_string_append (markup, "\n<span foreground=\"#888888\">");
        append_escaped (markup, doc, -1);
        g_string_append (markup, "</span>");
    }

    return g_string_free (markup, FALSE);
}


/**********************************************************************/
/* The popup
 */

static void
create_popup (void)
{
    GtkWidget *frame;

    if (popup.window)
        return;

    popup.label = gtk_label_new (NULL);
    gtk_label_set_use_markup (GTK_LABEL (popup.label), TRUE);
    gtk_label_set_selectable (GTK_LABEL (popup.label), FALSE);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
    gtk_container_add (GTK_CONTAINER (frame), popup.label);

    popup.window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_resizable (GTK_WINDOW (popup.window), FALSE);
    gtk_container_add (GTK_CONTAINER (popup.window), frame);

    gtk_widget_show_all (frame);
}


/*
 * Under the cursor, where the completion popup goes -- the two are never up at
 * once, and putting this one above the line instead would need the height of a
 * window that has not been shown yet, which the two toolkits answer
 * differently.
 */
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

    if (!window)
        return;

    gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                      gtk_text_buffer_get_insert (buffer));

    gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &rect);
    gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                           GTK_TEXT_WINDOW_TEXT,
                                           rect.x, rect.y + rect.height,
                                           &window_x, &window_y);
    gdk_window_get_origin (window, &origin_x, &origin_y);

    gtk_window_move (GTK_WINDOW (popup.window), origin_x + window_x,
                     origin_y + window_y);
}


static void
forget_view (void)
{
    if (popup.view)
    {
        g_object_remove_weak_pointer (G_OBJECT (popup.view), (gpointer*) &popup.view);
        popup.view = NULL;
    }
}


void
lsp_signature_cancel (void)
{
    if (popup.request && popup.server)
        lsp_server_cancel (popup.server, popup.request);

    popup.request = 0;
    popup.server = NULL;

    if (popup.window)
        gtk_widget_hide (popup.window);

    forget_view ();

    popup.visible = FALSE;
}


gboolean
lsp_signature_visible (void)
{
    return popup.visible;
}


gboolean
lsp_signature_key_press (MooEditView *view,
                         GdkEventKey *event)
{
    /*
     * Escape and nothing else. What is being typed is the call, so every other
     * key belongs to the document -- unlike the completion popup, which is
     * being chosen from and takes the keys that choose.
     */
    if (!popup.visible || view != popup.view)
        return FALSE;

    if (event->keyval == GDK_KEY_Escape)
    {
        lsp_signature_cancel ();
        return TRUE;
    }

    return FALSE;
}


/**********************************************************************/
/* Asking
 */

static void
signature_reply (JsonNode   *result,
                 JsonObject *error,
                 gpointer    data)
{
    MooEditView *view = (MooEditView*) data;
    char *markup;

    popup.request = 0;

    /* The view is the one the request was made from, held with a weak pointer:
       a window closed in between leaves NULL here. */
    if (error || !popup.view || popup.view != view)
    {
        lsp_signature_cancel ();
        return;
    }

    markup = lsp_signature_markup (result);

    /*
     * Nothing to show is how a server says the call is over -- the cursor has
     * left it, or what is being typed was never one -- and that is what closes
     * the popup. The client does not guess at it.
     */
    if (!markup)
    {
        lsp_signature_cancel ();
        return;
    }

    /* Not over the completion popup, which is placed in the same spot and is
       the one being chosen from. */
    if (lsp_completion_visible ())
    {
        g_free (markup);
        lsp_signature_cancel ();
        return;
    }

    create_popup ();
    gtk_label_set_markup (GTK_LABEL (popup.label), markup);
    g_free (markup);

    place_popup (view);
    gtk_widget_show (popup.window);
    popup.visible = TRUE;
}


/*
 * kind is the protocol's SignatureHelpTriggerKind: 1 the user asked, 2 a
 * character the server named was typed, 3 the text changed while the popup was
 * up. The last is what makes a stale popup impossible: every key asks again,
 * and the server's answer decides whether it stays.
 */
static void
ask (MooEditView *view,
     const char  *trigger_char,
     int          kind)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    GtkTextBuffer *buffer;
    GtkTextIter cursor;
    JsonObject *params;
    JsonObject *context;
    gboolean retrigger = popup.visible;
    int line = 0, character = 0;

    g_return_if_fail (MOO_IS_EDIT_VIEW (view));

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_SIGNATURE))
        return;

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server) ||
        !lsp_server_has_provider (server, "signatureHelpProvider"))
        return;

    if (popup.request && popup.server)
        lsp_server_cancel (popup.server, popup.request);

    /* The server must have the text the position refers to. */
    lsp_doc_flush (ldoc);

    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor,
                                      gtk_text_buffer_get_insert (buffer));

    lsp_iter_to_position (&cursor, lsp_server_get_position_encoding (server),
                          &line, &character);

    forget_view ();
    popup.view = view;
    g_object_add_weak_pointer (G_OBJECT (view), (gpointer*) &popup.view);

    context = json_object_new ();
    lsp_json_set_int (context, "triggerKind", kind);
    lsp_json_set_bool (context, "isRetrigger", retrigger);

    if (trigger_char)
        lsp_json_set_string (context, "triggerCharacter", trigger_char);

    params = json_object_new ();
    lsp_json_set_object (params, "textDocument",
                         lsp_json_text_document (lsp_doc_get_uri (ldoc)));
    lsp_json_set_object (params, "position", lsp_json_position (line, character));
    lsp_json_set_object (params, "context", context);

    popup.server = server;
    popup.request = lsp_server_call (server, "textDocument/signatureHelp", params,
                                     signature_reply, view, NULL);
}


void
lsp_signature_start (MooEditView *view,
                     const char  *trigger_char)
{
    ask (view, trigger_char, trigger_char ? 2 : 1);
}


/* One of the characters the server said it wants to be asked on. */
static const char *
trigger_for (LspServer  *server,
             const char *text)
{
    JsonObject *provider = lsp_json_get_object (lsp_server_get_capabilities (server),
                                                "signatureHelpProvider");
    static const char *lists[] = { "triggerCharacters", "retriggerCharacters" };
    guint i, j;

    for (j = 0; j < G_N_ELEMENTS (lists); ++j)
    {
        JsonArray *characters = lsp_json_get_array (provider, lists[j]);
        guint n = characters ? json_array_get_length (characters) : 0;

        for (i = 0; i < n; ++i)
        {
            const char *trigger = json_array_get_string_element (characters, i);

            if (trigger && strcmp (trigger, text) == 0)
                return trigger;
        }
    }

    return NULL;
}


void
lsp_signature_text_inserted (MooEditView *view,
                             const char  *text)
{
    MooEdit *doc;
    LspDoc *ldoc;
    LspServer *server;
    const char *trigger;

    if (!text || !text[0])
        return;

    if (!moo_prefs_get_bool (MOO_LSP_PREFS_SIGNATURE))
        return;

    /* One character at a time; a paste is not somebody typing a call. */
    if (g_utf8_strlen (text, -1) != 1)
        return;

    doc = moo_edit_view_get_doc (view);
    ldoc = doc ? lsp_manager_lookup_doc (doc) : NULL;
    server = ldoc ? lsp_doc_get_server (ldoc) : NULL;

    if (!server || !lsp_server_is_ready (server))
        return;

    trigger = trigger_for (server, text);

    if (popup.visible && view == popup.view)
    {
        /* Up already: ask on everything, and let the answer decide. */
        ask (view, trigger, trigger ? 2 : 3);
        return;
    }

    /*
     * Closed: only the characters the server named open it, or every key
     * pressed in a document would be a request.
     */
    if (trigger)
        ask (view, trigger, 2);
}
