/*
 *   plugins/lsp/lsp-doc.cpp
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

#include "plugins/lsp/lsp-doc.h"
#include "plugins/lsp/lsp-diagnostics.h"
#include "plugins/lsp/lsp-plugin.h"

#include "mooedit/moolang.h"
#include "mooutils/mooutils-file.h"
#include "mooutils/mooprefs.h"

#include <string.h>

struct LspDoc {
    MooEdit       *doc;
    LspServer     *server;          /* not owned; the manager keeps it alive */
    GtkTextBuffer *buffer;

    char          *uri;
    char          *language_id;

    int            version;
    guint          change_timeout;
    gboolean       opened;

    GSList        *diagnostics;     /* LspDiagnostic* */
};


/*
 * medit names its languages after the syntax files it inherited from
 * gtksourceview; the protocol has its own list. Only the ones that differ are
 * here, anything else is passed through, which is right for c, cpp, python,
 * go, rust and most of the rest.
 */
static const struct {
    const char *moo_id;
    const char *lsp_id;
} language_map[] = {
    { "chdr",       "c" },
    { "cpphdr",     "cpp" },
    { "c-sharp",    "csharp" },
    { "js",         "javascript" },
    { "sh",         "shellscript" },
    { "python3",    "python" },
    { "objc",       "objective-c" },
    { "objj",       "objective-cpp" },
    { "cuda",       "cuda-cpp" },
    { "dosbatch",   "bat" },
    { "vbnet",      "vb" },
    { "haskell-literate", "haskell" },
    { "xslt",       "xsl" },
    { "t2t",        "restructuredtext" },
    { "rst",        "restructuredtext" },
    { "gettext-translation", "po" }
};


const char *
lsp_language_id (const char *moo_lang_id)
{
    guint i;

    /* moo_edit_get_lang_id() answers MOO_LANG_NONE, not NULL, for a document
       with no syntax; the protocol spells that "plaintext". */
    if (!moo_lang_id || !moo_lang_id[0] || strcmp (moo_lang_id, MOO_LANG_NONE) == 0)
        return "plaintext";

    for (i = 0; i < G_N_ELEMENTS (language_map); ++i)
        if (strcmp (language_map[i].moo_id, moo_lang_id) == 0)
            return language_map[i].lsp_id;

    return moo_lang_id;
}


/**********************************************************************/
/* Positions
 */

static int
utf16_units (const char *text)
{
    const char *p;
    int units = 0;

    for (p = text; p && *p; p = g_utf8_next_char (p))
        units += g_utf8_get_char (p) >= 0x10000 ? 2 : 1;

    return units;
}


void
lsp_iter_to_position (const GtkTextIter   *iter,
                      LspPositionEncoding  encoding,
                      int                 *line,
                      int                 *character)
{
    GtkTextIter start;
    char *text;

    g_return_if_fail (iter != NULL);

    if (line)
        *line = gtk_text_iter_get_line (iter);

    if (!character)
        return;

    if (encoding == LSP_POSITION_ENCODING_UTF32)
    {
        /* A code point is exactly what a GtkTextIter offset counts. */
        *character = gtk_text_iter_get_line_offset (iter);
        return;
    }

    start = *iter;
    gtk_text_iter_set_line_offset (&start, 0);
    text = gtk_text_iter_get_text (&start, iter);

    if (encoding == LSP_POSITION_ENCODING_UTF8)
        *character = text ? (int) strlen (text) : 0;
    else
        *character = utf16_units (text);

    g_free (text);
}


gboolean
lsp_position_to_iter (GtkTextBuffer       *buffer,
                      int                  line,
                      int                  character,
                      LspPositionEncoding  encoding,
                      GtkTextIter         *iter)
{
    GtkTextIter end;
    int units = 0;

    g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
    g_return_val_if_fail (iter != NULL, FALSE);

    if (line < 0)
        line = 0;

    if (line >= gtk_text_buffer_get_line_count (buffer))
    {
        gtk_text_buffer_get_end_iter (buffer, iter);
        return FALSE;
    }

    gtk_text_buffer_get_iter_at_line (buffer, iter, line);

    if (character <= 0)
        return TRUE;

    if (encoding == LSP_POSITION_ENCODING_UTF32)
    {
        end = *iter;

        if (!gtk_text_iter_ends_line (&end))
            gtk_text_iter_forward_to_line_end (&end);

        if (character >= gtk_text_iter_get_line_offset (&end))
        {
            *iter = end;
            return TRUE;
        }

        gtk_text_iter_set_line_offset (iter, character);
        return TRUE;
    }

    /*
     * Walking one character at a time is the only way to land on the right one
     * when the units are not characters. Lines are short, and a position is
     * converted once per request, not once per keystroke.
     */
    while (units < character && !gtk_text_iter_ends_line (iter))
    {
        gunichar c = gtk_text_iter_get_char (iter);

        if (encoding == LSP_POSITION_ENCODING_UTF8)
        {
            char buf[6];
            units += g_unichar_to_utf8 (c, buf);
        }
        else
        {
            units += c >= 0x10000 ? 2 : 1;
        }

        gtk_text_iter_forward_char (iter);
    }

    return TRUE;
}


/**********************************************************************/
/* The document
 */

static char *
get_buffer_text (LspDoc *ldoc)
{
    GtkTextIter start, end;

    gtk_text_buffer_get_bounds (ldoc->buffer, &start, &end);

    return gtk_text_buffer_get_text (ldoc->buffer, &start, &end, TRUE);
}


static gboolean
send_did_change (gpointer data)
{
    LspDoc *ldoc = (LspDoc*) data;
    char *text;

    ldoc->change_timeout = 0;

    if (!ldoc->opened)
        return G_SOURCE_REMOVE;

    text = get_buffer_text (ldoc);
    lsp_server_did_change (ldoc->server, ldoc->uri, ++ldoc->version, text);
    g_free (text);

    return G_SOURCE_REMOVE;
}


static void
buffer_changed (LspDoc *ldoc)
{
    int delay;

    if (!ldoc->opened)
        return;

    if (ldoc->change_timeout)
        g_source_remove (ldoc->change_timeout);

    delay = moo_prefs_get_int (MOO_LSP_PREFS_SYNC_DELAY);

    if (delay <= 0)
        delay = MOO_LSP_SYNC_DELAY_DEFAULT;

    ldoc->change_timeout = g_timeout_add (delay, send_did_change, ldoc);
}


static void
doc_saved (LspDoc *ldoc)
{
    char *text;

    if (!ldoc->opened)
        return;

    /* The server has to see the final text before it is told about the save. */
    lsp_doc_flush (ldoc);

    text = get_buffer_text (ldoc);
    lsp_server_did_save (ldoc->server, ldoc->uri, text);
    g_free (text);
}


void
lsp_doc_flush (LspDoc *ldoc)
{
    g_return_if_fail (ldoc != NULL);

    if (!ldoc->change_timeout)
        return;

    g_source_remove (ldoc->change_timeout);
    ldoc->change_timeout = 0;

    send_did_change (ldoc);
}


void
lsp_doc_open (LspDoc *ldoc)
{
    char *text;

    g_return_if_fail (ldoc != NULL);

    if (lsp_server_has_doc (ldoc->server, ldoc->uri))
        return;

    text = get_buffer_text (ldoc);
    ldoc->version = 1;
    lsp_server_did_open (ldoc->server, ldoc->uri, ldoc->language_id,
                         ldoc->version, text);
    g_free (text);

    ldoc->opened = TRUE;
}


void
lsp_doc_refresh_diagnostics (LspDoc *ldoc)
{
    g_return_if_fail (ldoc != NULL);

    if (moo_prefs_get_bool (MOO_LSP_PREFS_DIAGNOSTICS))
        lsp_diagnostics_apply (ldoc->doc, ldoc->diagnostics,
                               lsp_server_get_position_encoding (ldoc->server));
    else
        lsp_diagnostics_clear (ldoc->doc);
}


void
lsp_doc_set_diagnostics (LspDoc    *ldoc,
                         JsonArray *array)
{
    g_return_if_fail (ldoc != NULL);

    lsp_diagnostics_free (ldoc->diagnostics);
    ldoc->diagnostics = lsp_diagnostics_parse (array);

    lsp_doc_refresh_diagnostics (ldoc);
}


GSList *
lsp_doc_get_diagnostics (LspDoc *ldoc)
{
    g_return_val_if_fail (ldoc != NULL, NULL);
    return ldoc->diagnostics;
}


gboolean
lsp_doc_is_current (LspDoc *ldoc)
{
    GFile *file;
    char *uri;
    char *lang_id;
    gboolean same;

    g_return_val_if_fail (ldoc != NULL, FALSE);

    file = moo_edit_get_file (ldoc->doc);
    uri = file ? g_file_get_uri (file) : NULL;
    moo_file_free (file);

    if (!uri)
        return FALSE;

    lang_id = moo_edit_get_lang_id (ldoc->doc);

    same = strcmp (uri, ldoc->uri) == 0 &&
           strcmp (lsp_language_id (lang_id), ldoc->language_id) == 0;

    g_free (lang_id);
    g_free (uri);

    return same;
}


LspDoc *
lsp_doc_new (MooEdit   *doc,
             LspServer *server)
{
    LspDoc *ldoc;
    GFile *file;
    char *uri;
    char *lang_id;

    g_return_val_if_fail (MOO_IS_EDIT (doc), NULL);
    g_return_val_if_fail (server != NULL, NULL);

    file = moo_edit_get_file (doc);
    uri = file ? g_file_get_uri (file) : NULL;
    moo_file_free (file);

    if (!uri)
        return NULL;

    lang_id = moo_edit_get_lang_id (doc);

    ldoc = g_new0 (LspDoc, 1);
    ldoc->doc = doc;
    ldoc->server = server;
    ldoc->buffer = moo_edit_get_buffer (doc);
    ldoc->uri = uri;
    ldoc->language_id = g_strdup (lsp_language_id (lang_id));

    g_free (lang_id);

    g_signal_connect_swapped (ldoc->buffer, "changed",
                              G_CALLBACK (buffer_changed), ldoc);
    g_signal_connect_swapped (doc, "after-save",
                              G_CALLBACK (doc_saved), ldoc);

    lsp_doc_open (ldoc);

    return ldoc;
}


void
lsp_doc_free (LspDoc *ldoc)
{
    if (!ldoc)
        return;

    if (ldoc->change_timeout)
        g_source_remove (ldoc->change_timeout);

    if (ldoc->buffer)
        g_signal_handlers_disconnect_by_data (ldoc->buffer, ldoc);
    if (ldoc->doc)
        g_signal_handlers_disconnect_by_data (ldoc->doc, ldoc);

    if (ldoc->opened)
        lsp_server_did_close (ldoc->server, ldoc->uri);

    if (ldoc->doc)
        lsp_diagnostics_clear (ldoc->doc);

    lsp_diagnostics_free (ldoc->diagnostics);

    g_free (ldoc->uri);
    g_free (ldoc->language_id);
    g_free (ldoc);
}


MooEdit *
lsp_doc_get_doc (LspDoc *ldoc)
{
    g_return_val_if_fail (ldoc != NULL, NULL);
    return ldoc->doc;
}


LspServer *
lsp_doc_get_server (LspDoc *ldoc)
{
    g_return_val_if_fail (ldoc != NULL, NULL);
    return ldoc->server;
}


const char *
lsp_doc_get_uri (LspDoc *ldoc)
{
    g_return_val_if_fail (ldoc != NULL, NULL);
    return ldoc->uri;
}
