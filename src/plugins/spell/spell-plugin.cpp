/*
 *   plugins/spell/spell-plugin.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
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
 * Spell checking through Enchant, which is loaded at run time: see
 * spell-dict.cpp.
 *
 * Only what is on the screen is checked, a moment after the last thing that
 * could have changed it (typing, scrolling, the cursor moving), and the
 * misspelled words are marked with a tag. The context menu is built from the
 * document ui xml with document actions rather than from ::populate-popup, so
 * its spelling entries are always there and are shown or hidden, and relabelled,
 * when the button goes down.
 */

#include "plugins/spell/spell-plugin.h"
#include "plugins/spell/spell-dict.h"

#include "mooedit/mooplugin-macro.h"
#include "mooedit/mooeditaction-factory.h"
#include "mooedit/mootext-private.h"
#include "mooutils/mooprefs.h"
#include "mooutils/mooi18n.h"

#define SPELL_CHECK_DELAY       300     /* ms */
#define SPELL_MAX_SUGGESTIONS   5

#define SPELL_VIEW_HOOKED_QUARK "moo-spell-view-hooked"
#define SPELL_VIEW_ADJ_QUARK    "moo-spell-view-adjustment"

typedef struct {
    MooPlugin parent;
    guint     ui_merge_id;
    guint     doc_ui_merge_id;
} SpellPlugin;

typedef struct {
    MooDocPlugin parent;

    GtkTextTag *tag;
    guint       timeout;
    gboolean    edited;         /* the text changed since the last check */
    GHashTable *ignored;        /* folded words let go in this document */

    /* what the context menu was opened on, if on a misspelled word */
    char           *word;
    MooSpellScript  script;
    int             word_start;
    int             word_end;
    char          **suggestions;
} SpellDocPlugin;

MOO_PLUGIN_DEFINE_INFO (spell,
                        N_("Spell Checking"), N_("Underlines misspelled words"),
                        "Anton Batenev <antonbatenev@yandex.ru>",
                        MOO_VERSION)

MOO_DOC_PLUGIN_DEFINE (Spell, spell)

#define LOOKUP_DOC_PLUGIN(doc) \
    ((SpellDocPlugin*) moo_doc_plugin_lookup (MOO_SPELL_PLUGIN_ID, (doc)))


/**********************************************************************/
/* Dictionaries
 *
 * One per alphabet, shared by every document, so that a word added in one is
 * known in all.
 */

static struct {
    SpellDict  *dict[2];        /* by MooSpellScript */
    gboolean    loaded;
    GHashTable *ignored;        /* folded words let go for the session */
} spell;

static const char *
pref_string (const char *key)
{
    const char *s = moo_prefs_get_string (key);
    return s ? s : "";
}

static void
dicts_load (void)
{
    if (spell.loaded)
        return;

    spell.loaded = TRUE;

    /* The tests must not depend on what is installed. */
    if (g_getenv ("MOO_SPELL_STUB"))
    {
        spell.dict[MOO_SPELL_LATIN] = moo_spell_dict_new_stub (MOO_SPELL_LATIN);
        spell.dict[MOO_SPELL_CYRILLIC] = moo_spell_dict_new_stub (MOO_SPELL_CYRILLIC);
        return;
    }

    char **langs = g_strsplit_set (pref_string (MOO_SPELL_PREFS_LANGUAGES), ", ;", -1);

    for (char **l = langs; *l; ++l)
    {
        if (!**l)
            continue;

        MooSpellScript script = moo_spell_lang_script (*l);

        if (!spell.dict[script])
            spell.dict[script] = moo_spell_dict_open (*l);
    }

    g_strfreev (langs);
}

static void
dicts_unload (void)
{
    for (guint i = 0; i < G_N_ELEMENTS (spell.dict); ++i)
    {
        moo_spell_dict_destroy (spell.dict[i]);
        spell.dict[i] = NULL;
    }

    spell.loaded = FALSE;
}

/* Enchant knows the apostrophe, not the typographic one. */
static char *
lookup_form (const char *s,
             gsize       len)
{
    GString *out = g_string_sized_new (len);

    for (const char *p = s, *end = s + len; p < end; p = g_utf8_next_char (p))
    {
        gunichar c = g_utf8_get_char (p);
        g_string_append_unichar (out, c == 0x2019 ? '\'' : c);
    }

    return g_string_free (out, FALSE);
}

static gboolean
word_is_error (SpellDocPlugin      *sp,
               const MooSpellWord  *w)
{
    SpellDict *dict = spell.dict[w->script];

    if (!dict)
        return FALSE;

    char *form = lookup_form (w->start, w->end - w->start);
    gsize len = strlen (form);
    gboolean error = !dict->check (dict, form, len);

    if (error)
    {
        char *folded = g_utf8_strdown (form, len);

        if (g_hash_table_contains (spell.ignored, folded) ||
            g_hash_table_contains (sp->ignored, folded))
            error = FALSE;

        g_free (folded);
    }

    g_free (form);
    return error;
}

/* Where the words of a piece of text are, as offsets in the buffer. */
typedef struct {
    SpellDocPlugin *sp;
    GtkTextBuffer  *buffer;
    gboolean        code;       /* only comments and strings */
    const char     *text;
    gsize           len;
    gsize           pos;
    const char     *last;       /* where offset "off" is in text */
    int             off;
    int             start;      /* of the word found, in characters */
    int             n;
    MooSpellWord    word;
} Scan;

/* from is the offset in the buffer of the character at "at" in the text. */
static void
scan_init (Scan           *scan,
           SpellDocPlugin *sp,
           GtkTextBuffer  *buffer,
           gboolean        code,
           const char     *text,
           gsize           at,
           int             from)
{
    scan->sp = sp;
    scan->buffer = buffer;
    scan->code = code;
    scan->text = text;
    scan->len = strlen (text);
    scan->pos = at;
    scan->last = text + at;
    scan->off = from;
}

static gboolean
scan_next_error (Scan *scan)
{
    while (moo_spell_next_word (scan->text, scan->len, &scan->pos, &scan->word))
    {
        scan->off += g_utf8_pointer_to_offset (scan->last, scan->word.start);
        scan->n = g_utf8_pointer_to_offset (scan->word.start, scan->word.end);
        scan->start = scan->off;
        scan->off += scan->n;
        scan->last = scan->word.end;

        if (scan->code)
        {
            GtkTextIter iter;

            gtk_text_buffer_get_iter_at_offset (scan->buffer, &iter, scan->start);

            if (!_moo_text_buffer_iter_in_prose (MOO_TEXT_BUFFER (scan->buffer), &iter))
                continue;
        }

        if (word_is_error (scan->sp, &scan->word))
            return TRUE;
    }

    return FALSE;
}


/**********************************************************************/
/* What is checked
 */

/* Whether the extension of the file is one that is listed for checking. */
static gboolean
doc_has_listed_extension (MooEdit *doc)
{
    char *filename = moo_edit_get_filename (doc);
    const char *dot = filename ? strrchr (filename, '.') : NULL;
    gboolean found = FALSE;

    if (dot)
    {
        char **exts = g_strsplit_set (pref_string (MOO_SPELL_PREFS_EXTENSIONS), ", ;", -1);

        for (char **e = exts; *e; ++e)
        {
            const char *ext = *e + strspn (*e, ".*");

            if (*ext && g_ascii_strcasecmp (ext, dot + 1) == 0)
                found = TRUE;
        }

        g_strfreev (exts);
    }

    g_free (filename);
    return found;
}

enum { SPELL_OFF, SPELL_TEXT, SPELL_CODE };

/* A listed file is text through and through; any other has its comments and
   strings checked, if that is asked for. */
static int
doc_mode (MooEdit *doc)
{
    if (doc_has_listed_extension (doc))
        return SPELL_TEXT;

    return moo_prefs_get_bool (MOO_SPELL_PREFS_CHECK_CODE) ? SPELL_CODE : SPELL_OFF;
}


/**********************************************************************/
/* Marking
 */

static void
hook_view (MooEditView *view);

static gboolean
get_visible_range (GtkTextView *view,
                   GtkTextIter *start,
                   GtkTextIter *end)
{
    GdkRectangle rect;

    gtk_text_view_get_visible_rect (view, &rect);

    if (rect.height <= 0)
        return FALSE;

    gtk_text_view_get_line_at_y (view, start, rect.y, NULL);
    gtk_text_view_get_line_at_y (view, end, rect.y + rect.height, NULL);
    gtk_text_iter_set_line_offset (start, 0);

    if (!gtk_text_iter_ends_line (end))
        gtk_text_iter_forward_to_line_end (end);

    return TRUE;
}

/* cursor is where the word being typed is, or -1 for none. */
static void
check_range (SpellDocPlugin    *sp,
             GtkTextBuffer     *buffer,
             const GtkTextIter *start,
             const GtkTextIter *end,
             int                mode,
             int                cursor)
{
    gtk_text_buffer_remove_tag (buffer, sp->tag, start, end);

    char *text = gtk_text_buffer_get_text (buffer, start, end, FALSE);
    Scan scan;

    scan_init (&scan, sp, buffer, mode == SPELL_CODE, text, 0, gtk_text_iter_get_offset (start));

    while (scan_next_error (&scan))
    {
        if (cursor < scan.start || cursor > scan.start + scan.n)
        {
            GtkTextIter a, b;

            gtk_text_buffer_get_iter_at_offset (buffer, &a, scan.start);
            gtk_text_buffer_get_iter_at_offset (buffer, &b, scan.start + scan.n);
            gtk_text_buffer_apply_tag (buffer, sp->tag, &a, &b);
        }
    }

    g_free (text);
}

static void
check_doc (SpellDocPlugin *sp,
           MooEdit        *doc)
{
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    gboolean typing = sp->edited;
    GtkTextIter a, b;

    sp->edited = FALSE;

    dicts_load ();

    int mode = doc_mode (doc);

    if (mode == SPELL_OFF || (!spell.dict[0] && !spell.dict[1]))
    {
        gtk_text_buffer_get_bounds (buffer, &a, &b);
        gtk_text_buffer_remove_tag (buffer, sp->tag, &a, &b);
        return;
    }

    /* The word being typed is not yet a mistake; one merely clicked on is. */
    int cursor = -1;

    if (typing)
    {
        gtk_text_buffer_get_iter_at_mark (buffer, &a, gtk_text_buffer_get_insert (buffer));
        cursor = gtk_text_iter_get_offset (&a);
    }

    MooEditViewArray *views = moo_edit_get_views (doc);

    for (guint i = 0; i < views->size (); ++i)
    {
        hook_view (views->elms[i]);

        if (get_visible_range (GTK_TEXT_VIEW (views->elms[i]), &a, &b))
            check_range (sp, buffer, &a, &b, mode, cursor);
    }

    delete views;
}

static gboolean
check_timeout (SpellDocPlugin *sp)
{
    sp->timeout = 0;
    check_doc (sp, moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (sp)));
    return FALSE;
}

/* Not restarted by what comes in meanwhile: a scroll that goes on would
   otherwise never get its words marked. */
static void
queue_check (SpellDocPlugin *sp)
{
    if (!sp->timeout)
        sp->timeout = g_timeout_add (SPELL_CHECK_DELAY, (GSourceFunc) check_timeout, sp);
}

static void
queue_all (void)
{
    MooEditArray *docs = moo_editor_get_docs (moo_editor_instance ());

    for (guint i = 0; i < docs->size (); ++i)
    {
        SpellDocPlugin *sp = LOOKUP_DOC_PLUGIN (docs->elms[i]);

        if (sp)
            queue_check (sp);
    }

    delete docs;
}

void
_moo_spell_apply_prefs (void)
{
    /* the languages may have changed */
    dicts_unload ();
    queue_all ();
}


/**********************************************************************/
/* The context menu
 */

static void
set_action (MooEdit    *doc,
            const char *id,
            gboolean    visible,
            const char *label)
{
    GtkAction *action = moo_edit_get_action_by_id (doc, id);

    if (!action)
        return;

    if (label)
    {
        /* a mnemonic is not what a suggestion means by an underscore */
        char **parts = g_strsplit (label, "_", -1);
        char *escaped = g_strjoinv ("__", parts);

        g_object_set (action, "label", escaped, (const char*) NULL);

        g_free (escaped);
        g_strfreev (parts);
    }

    g_object_set (action, "visible", visible, (const char*) NULL);
}

static void
forget_word (SpellDocPlugin *sp)
{
    g_free (sp->word);
    sp->word = NULL;
    g_strfreev (sp->suggestions);
    sp->suggestions = NULL;
}

/* The line between the suggestions and the rest is in the menu only while there
   are suggestions: a separator is not an action, so it cannot be hidden. */
static guint separator_merge_id;

static void
show_separator (gboolean show)
{
    MooUiXml *xml = moo_editor_get_doc_ui_xml (moo_editor_instance ());

    if (!xml || show == (separator_merge_id != 0))
        return;

    if (show)
    {
        separator_merge_id = moo_ui_xml_new_merge_id (xml);
        moo_ui_xml_insert_markup_before (xml, separator_merge_id, "Editor/Popup/PopupStart",
                                         "SpellAdd", "<separator/>");
    }
    else
    {
        moo_ui_xml_remove_ui (xml, separator_merge_id);
        separator_merge_id = 0;
    }
}

/* The word at the iterator, when it is underlined, and the menu to go with it. */
static void
update_menu (SpellDocPlugin    *sp,
             MooEdit           *doc,
             const GtkTextIter *at)
{
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextIter s = *at, e;

    forget_word (sp);

    if (gtk_text_iter_has_tag (&s, sp->tag) ||
        (gtk_text_iter_backward_char (&s) && gtk_text_iter_has_tag (&s, sp->tag)))
    {
        e = s;

        if (!gtk_text_iter_begins_tag (&s, sp->tag))
            gtk_text_iter_backward_to_tag_toggle (&s, sp->tag);
        gtk_text_iter_forward_to_tag_toggle (&e, sp->tag);

        char *text = gtk_text_buffer_get_text (buffer, &s, &e, FALSE);
        gsize pos = 0;
        MooSpellWord w;

        if (moo_spell_next_word (text, strlen (text), &pos, &w) && spell.dict[w.script])
        {
            SpellDict *dict = spell.dict[w.script];

            sp->word = lookup_form (w.start, w.end - w.start);
            sp->script = w.script;
            sp->word_start = gtk_text_iter_get_offset (&s);
            sp->word_end = gtk_text_iter_get_offset (&e);
            sp->suggestions = dict->suggest (dict, sp->word, strlen (sp->word),
                                             SPELL_MAX_SUGGESTIONS);
        }

        g_free (text);
    }

    guint n = sp->suggestions ? g_strv_length (sp->suggestions) : 0;

    for (guint i = 0; i < SPELL_MAX_SUGGESTIONS; ++i)
    {
        char *id = g_strdup_printf ("SpellSuggest%u", i);
        set_action (doc, id, i < n, i < n ? sp->suggestions[i] : NULL);
        g_free (id);
    }

    show_separator (n > 0);
    set_action (doc, "SpellAdd", sp->word != NULL, NULL);
    set_action (doc, "SpellIgnoreAll", sp->word != NULL, NULL);
    set_action (doc, "SpellIgnoreDoc", sp->word != NULL, NULL);
}

static void
apply_suggestion (MooEdit *doc,
                  guint    i)
{
    SpellDocPlugin *sp = LOOKUP_DOC_PLUGIN (doc);

    if (!sp || !sp->word || !sp->suggestions || i >= g_strv_length (sp->suggestions))
        return;

    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextIter s, e;

    gtk_text_buffer_get_iter_at_offset (buffer, &s, sp->word_start);
    gtk_text_buffer_get_iter_at_offset (buffer, &e, sp->word_end);

    gtk_text_buffer_begin_user_action (buffer);
    gtk_text_buffer_delete (buffer, &s, &e);
    gtk_text_buffer_insert (buffer, &s, sp->suggestions[i], -1);
    gtk_text_buffer_end_user_action (buffer);
}

#define SUGGEST_CB(n) \
    static void suggest_##n##_cb (MooEdit *doc) { apply_suggestion (doc, n); }

SUGGEST_CB (0)
SUGGEST_CB (1)
SUGGEST_CB (2)
SUGGEST_CB (3)
SUGGEST_CB (4)

static void
add_cb (MooEdit *doc)
{
    SpellDocPlugin *sp = LOOKUP_DOC_PLUGIN (doc);

    if (!sp || !sp->word)
        return;

    SpellDict *dict = spell.dict[sp->script];

    if (dict)
        dict->add (dict, sp->word, strlen (sp->word));

    queue_all ();
}

static void
ignore_word (MooEdit    *doc,
             GHashTable *(*set) (SpellDocPlugin*))
{
    SpellDocPlugin *sp = LOOKUP_DOC_PLUGIN (doc);

    if (!sp || !sp->word)
        return;

    g_hash_table_add (set (sp), g_utf8_strdown (sp->word, -1));
    queue_all ();
}

static GHashTable *
session_set (SpellDocPlugin *)
{
    return spell.ignored;
}

static GHashTable *
document_set (SpellDocPlugin *sp)
{
    return sp->ignored;
}

static void
ignore_all_cb (MooEdit *doc)
{
    ignore_word (doc, session_set);
}

static void
ignore_doc_cb (MooEdit *doc)
{
    ignore_word (doc, document_set);
}

static gboolean
view_button_press (MooEditView    *view,
                   GdkEventButton *event)
{
    if (event->button != 3)
        return FALSE;

    MooEdit *doc = moo_edit_view_get_doc (view);
    SpellDocPlugin *sp = doc ? LOOKUP_DOC_PLUGIN (doc) : NULL;
    GtkTextView *text_view = GTK_TEXT_VIEW (view);

    if (!sp)
        return FALSE;

    GtkTextIter at;
    int x, y;

    /* the margin has its own window, and coordinates that mean something else */
    if (event->window != gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT))
    {
        gtk_text_buffer_get_start_iter (moo_edit_get_buffer (doc), &at);
        forget_word (sp);
        update_menu (sp, doc, &at);
        return FALSE;
    }

    gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                           (int) event->x, (int) event->y, &x, &y);
    gtk_text_view_get_iter_at_location (text_view, &at, x, y);
    update_menu (sp, doc, &at);
    return FALSE;
}

/* the menu key, which puts the menu where the cursor is */
static gboolean
view_popup_menu (MooEditView *view)
{
    MooEdit *doc = moo_edit_view_get_doc (view);
    SpellDocPlugin *sp = doc ? LOOKUP_DOC_PLUGIN (doc) : NULL;

    if (sp)
    {
        GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
        GtkTextIter at;

        gtk_text_buffer_get_iter_at_mark (buffer, &at, gtk_text_buffer_get_insert (buffer));
        update_menu (sp, doc, &at);
    }

    return FALSE;
}

static void
queue_view_doc (MooEditView *view)
{
    MooEdit *doc = moo_edit_view_get_doc (view);
    SpellDocPlugin *sp = doc ? LOOKUP_DOC_PLUGIN (doc) : NULL;

    if (sp)
        queue_check (sp);
}

static void
view_scrolled (G_GNUC_UNUSED GtkAdjustment *adjustment,
               MooEditView                 *view)
{
    queue_view_doc (view);
}

static void
view_resized (MooEditView *view)
{
    queue_view_doc (view);
}

/*
 * Views are hooked when a check comes round to them, since a split view is
 * made long after its document is. The scroll bar is looked at every time, as
 * the view gets its adjustments only when it is put into a scrolled window.
 */
static void
hook_view (MooEditView *view)
{
    if (!g_object_get_data (G_OBJECT (view), SPELL_VIEW_HOOKED_QUARK))
    {
        g_object_set_data (G_OBJECT (view), SPELL_VIEW_HOOKED_QUARK, GINT_TO_POINTER (TRUE));

        g_signal_connect (view, "button-press-event", G_CALLBACK (view_button_press), NULL);
        g_signal_connect (view, "popup-menu", G_CALLBACK (view_popup_menu), NULL);
        g_signal_connect (view, "size-allocate", G_CALLBACK (view_resized), NULL);
    }

    GtkAdjustment *adj = gtk_text_view_get_vadjustment (GTK_TEXT_VIEW (view));

    if (adj && g_object_get_data (G_OBJECT (view), SPELL_VIEW_ADJ_QUARK) != adj)
    {
        g_object_set_data (G_OBJECT (view), SPELL_VIEW_ADJ_QUARK, adj);
        g_signal_connect_object (adj, "value-changed", G_CALLBACK (view_scrolled), view, (GConnectFlags) 0);
    }
}


/**********************************************************************/
/* The document
 */

static void
edited (SpellDocPlugin *sp)
{
    sp->edited = TRUE;
    queue_check (sp);
}

static void
doc_moved (SpellDocPlugin *sp)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (sp));
    MooEditViewArray *views = moo_edit_get_views (doc);

    /* the cursor moving is also what a click in a view not hooked yet is */
    for (guint i = 0; i < views->size (); ++i)
        hook_view (views->elms[i]);

    delete views;
    queue_check (sp);
}

static gboolean
spell_doc_plugin_create (SpellDocPlugin *sp)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (sp));
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);

    sp->tag = gtk_text_buffer_create_tag (buffer, NULL,
                                          "underline", PANGO_UNDERLINE_ERROR,
                                          (const char*) NULL);
    sp->ignored = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    g_signal_connect_swapped (buffer, "changed", G_CALLBACK (edited), sp);
    /* comments and strings are known only once highlighted */
    g_signal_connect_swapped (buffer, "highlight-updated", G_CALLBACK (queue_check), sp);
    g_signal_connect_swapped (buffer, "notify::cursor-position", G_CALLBACK (doc_moved), sp);
    g_signal_connect_swapped (doc, "filename-changed", G_CALLBACK (queue_check), sp);
    g_signal_connect_swapped (doc, "notify::lang", G_CALLBACK (queue_check), sp);

    queue_check (sp);
    return TRUE;
}

static void
spell_doc_plugin_destroy (SpellDocPlugin *sp)
{
    MooEdit *doc = moo_doc_plugin_get_doc (MOO_DOC_PLUGIN (sp));
    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);

    if (sp->timeout)
        g_source_remove (sp->timeout);
    sp->timeout = 0;

    g_signal_handlers_disconnect_by_data (doc, sp);
    g_signal_handlers_disconnect_by_data (buffer, sp);

    gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (buffer), sp->tag);
    sp->tag = NULL;

    forget_word (sp);
    g_hash_table_unref (sp->ignored);
    sp->ignored = NULL;
}


/**********************************************************************/
/* Next error
 */

static void
next_error_cb (MooEditWindow *window)
{
    MooEdit *doc = moo_edit_window_get_active_doc (window);
    MooEditView *view = moo_edit_window_get_active_view (window);
    SpellDocPlugin *sp = doc ? LOOKUP_DOC_PLUGIN (doc) : NULL;

    if (!sp || !view || doc_mode (doc) == SPELL_OFF)
        return;

    dicts_load ();

    GtkTextBuffer *buffer = moo_edit_get_buffer (doc);
    GtkTextIter a, b;

    gtk_text_buffer_get_selection_bounds (buffer, &a, &b);
    a = b;      /* the far end of the selection, or the cursor */

    int from = gtk_text_iter_get_offset (&a);

    gtk_text_buffer_get_bounds (buffer, &a, &b);
    char *text = gtk_text_buffer_get_text (buffer, &a, &b, FALSE);
    int found_start = -1, found_end = -1;

    /* from the cursor to the end, and then from the start up to it */
    for (int pass = 0; pass < 2 && found_start < 0; ++pass)
    {
        Scan scan;

        scan_init (&scan, sp, buffer, doc_mode (doc) == SPELL_CODE, text,
                   pass ? 0 : g_utf8_offset_to_pointer (text, from) - text,
                   pass ? 0 : from);

        if (scan_next_error (&scan) && (pass == 0 || scan.start < from))
        {
            found_start = scan.start;
            found_end = scan.start + scan.n;
        }
    }

    g_free (text);

    if (found_start < 0)
        return;

    gtk_text_buffer_get_iter_at_offset (buffer, &a, found_start);
    gtk_text_buffer_get_iter_at_offset (buffer, &b, found_end);
    gtk_text_buffer_select_range (buffer, &b, &a);
    gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view), gtk_text_buffer_get_insert (buffer));
}


/**********************************************************************/
/* The plugin
 */

static void
new_doc_action (MooEditClass *klass,
                const char   *id,
                const char   *name,
                GCallback     callback)
{
    moo_edit_class_new_action (klass, id,
                               "display-name", name,
                               "label", name,
                               "tooltip", _("Spelling"),
                               "closure-callback", callback,
                               (char*) 0);
}

static gboolean
spell_plugin_init (SpellPlugin *plugin)
{
    MooEditor *editor = moo_editor_instance ();
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditClass *edit_klass = (MooEditClass*) g_type_class_ref (MOO_TYPE_EDIT);
    MooUiXml *xml = moo_editor_get_ui_xml (editor);
    MooUiXml *doc_xml = moo_editor_get_doc_ui_xml (editor);
    static const char *const suggest_ids[] = {
        "SpellSuggest0", "SpellSuggest1", "SpellSuggest2", "SpellSuggest3", "SpellSuggest4"
    };
    static const GCallback suggest_cbs[] = {
        G_CALLBACK (suggest_0_cb), G_CALLBACK (suggest_1_cb), G_CALLBACK (suggest_2_cb),
        G_CALLBACK (suggest_3_cb), G_CALLBACK (suggest_4_cb)
    };

    g_return_val_if_fail (klass != NULL && edit_klass != NULL, FALSE);

    moo_prefs_new_key_string (MOO_SPELL_PREFS_LANGUAGES, "en_US,ru_RU");
    moo_prefs_new_key_string (MOO_SPELL_PREFS_EXTENSIONS, "txt,md,rst,tex");
    moo_prefs_new_key_bool (MOO_SPELL_PREFS_CHECK_CODE, FALSE);

    spell.ignored = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    if (!moo_window_class_find_group (klass, MOO_SPELL_PLUGIN_ID))
        moo_window_class_new_group (klass, MOO_SPELL_PLUGIN_ID, _("Spell Checking"));

    moo_window_class_new_action (klass, "SpellNextError", MOO_SPELL_PLUGIN_ID,
                                 "display-name", _("Next Spelling Error"),
                                 "label", _("Next _Spelling Error"),
                                 "tooltip", _("Go to the next misspelled word"),
                                 "stock-id", GTK_STOCK_SPELL_CHECK,
                                 "default-accel", "F7",
                                 "closure-callback", next_error_cb,
                                 nullptr);

    for (guint i = 0; i < G_N_ELEMENTS (suggest_ids); ++i)
        new_doc_action (edit_klass, suggest_ids[i], _("Suggestion"), suggest_cbs[i]);
    new_doc_action (edit_klass, "SpellAdd", _("Add to Dictionary"), G_CALLBACK (add_cb));
    new_doc_action (edit_klass, "SpellIgnoreAll", _("Ignore All"), G_CALLBACK (ignore_all_cb));
    new_doc_action (edit_klass, "SpellIgnoreDoc", _("Ignore in This Document"), G_CALLBACK (ignore_doc_cb));

    if (xml)
    {
        plugin->ui_merge_id = moo_ui_xml_new_merge_id (xml);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id, "Editor/Menubar/Tools",
                             "SpellNextError", "SpellNextError", -1);
    }

    if (doc_xml)
    {
        plugin->doc_ui_merge_id = moo_ui_xml_new_merge_id (doc_xml);

        for (guint i = 0; i < G_N_ELEMENTS (suggest_ids); ++i)
            moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id, "Editor/Popup/PopupStart",
                                 suggest_ids[i], suggest_ids[i], -1);
        moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id, "Editor/Popup/PopupStart",
                             "SpellAdd", "SpellAdd", -1);
        moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id, "Editor/Popup/PopupStart",
                             "SpellIgnoreAll", "SpellIgnoreAll", -1);
        moo_ui_xml_add_item (doc_xml, plugin->doc_ui_merge_id, "Editor/Popup/PopupStart",
                             "SpellIgnoreDoc", "SpellIgnoreDoc", -1);
    }

    g_type_class_unref (edit_klass);
    g_type_class_unref (klass);
    return TRUE;
}

static void
spell_plugin_deinit (SpellPlugin *plugin)
{
    MooEditor *editor = moo_editor_instance ();
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditClass *edit_klass = (MooEditClass*) g_type_class_ref (MOO_TYPE_EDIT);
    MooUiXml *xml = moo_editor_get_ui_xml (editor);
    MooUiXml *doc_xml = moo_editor_get_doc_ui_xml (editor);
    static const char *const doc_ids[] = {
        "SpellSuggest0", "SpellSuggest1", "SpellSuggest2", "SpellSuggest3", "SpellSuggest4",
        "SpellAdd", "SpellIgnoreAll", "SpellIgnoreDoc"
    };

    moo_window_class_remove_action (klass, "SpellNextError");

    for (guint i = 0; i < G_N_ELEMENTS (doc_ids); ++i)
        moo_edit_class_remove_action (edit_klass, doc_ids[i]);

    if (plugin->ui_merge_id && xml)
        moo_ui_xml_remove_ui (xml, plugin->ui_merge_id);
    plugin->ui_merge_id = 0;

    show_separator (FALSE);

    if (plugin->doc_ui_merge_id && doc_xml)
        moo_ui_xml_remove_ui (doc_xml, plugin->doc_ui_merge_id);
    plugin->doc_ui_merge_id = 0;

    g_type_class_unref (edit_klass);
    g_type_class_unref (klass);

    dicts_unload ();
    moo_spell_dict_shutdown ();

    if (spell.ignored)
        g_hash_table_unref (spell.ignored);
    spell.ignored = NULL;
}

MOO_PLUGIN_DEFINE (Spell, spell,
                   NULL, NULL, NULL, NULL,
                   _moo_spell_prefs_page,
                   (GType) 0, spell_doc_plugin_get_type ())

gboolean
moo_spell_plugin_init (void)
{
    /* Off until asked for, like the language server client. */
    MooPluginParams params = { FALSE, TRUE };

    return moo_plugin_register (MOO_SPELL_PLUGIN_ID,
                                spell_plugin_get_type (),
                                &spell_plugin_info,
                                &params);
}
