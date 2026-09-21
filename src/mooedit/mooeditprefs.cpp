/*
 *   mooeditprefs.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#include "mooedit/mooeditprefs.h"
#include "mooedit/mooedit-impl.h"
#include "mooedit/mooeditview-impl.h"
#include "mooedit/mooedit-fileops.h"
#include "mooedit/mootextview-private.h"
#include "mooedit/mootextbuffer.h"
#include "mooedit/moolangmgr.h"
#include "mooutils/mooencodings.h"
#include "mooutils/mooi18n.h"

#define DEFAULT_FONT "Monospace"

static void _moo_edit_init_prefs (void);


static guint settings[MOO_EDIT_LAST_SETTING];
guint *_moo_edit_settings = settings;


const char *
_moo_get_default_encodings (void)
{
    /* Translators: if translated, it should be a comma-separated list
       of encodings to try when opening files. Encodings names should be
       those understood by iconv, or "LOCALE" which means user's locale
       charset. For instance, the default value is "UTF-8,LOCALE,ISO_8859-15,ISO_8859-1".
       You want to add common preferred non-UTF8 encodings used in your locale.
       Do not remove ISO_8859-15 and ISO_8859-1, instead leave them at the end,
       these are common source files encodings. */
    const char *to_translate = N_("encodings_list");
    const char *encodings;

    encodings = _(to_translate);

    if (!strcmp (encodings, to_translate))
        encodings = "UTF-8," MOO_EDIT_ENCODING_LOCALE ",ISO_8859-1,ISO_8859-15";

    return encodings;
}

const char *
_moo_edit_get_default_encoding (void)
{
    return moo_prefs_get_string (moo_edit_setting (MOO_EDIT_PREFS_ENCODING_SAVE));
}


void
_moo_edit_init_config (void)
{
    static gboolean done = FALSE;

    if (done)
        return;
    done = TRUE;

    _moo_edit_init_prefs ();

    _moo_edit_settings[MOO_EDIT_SETTING_LANG] =
        moo_edit_config_install_setting (g_param_spec_string ("lang", "lang", "lang",
                                                              NULL,
                                                              (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_INDENT] =
        moo_edit_config_install_setting (g_param_spec_string ("indent", "indent", "indent",
                                                              NULL,
                                                              (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_STRIP] =
        moo_edit_config_install_setting (g_param_spec_boolean ("strip", "strip", "strip",
                                                               FALSE,
                                                               (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_ADD_NEWLINE] =
        moo_edit_config_install_setting (g_param_spec_boolean ("add-newline", "add-newline", "add-newline",
                                                               FALSE,
                                                               (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_WRAP_MODE] =
        moo_edit_config_install_setting (g_param_spec_enum ("wrap-mode", "wrap-mode", "wrap-mode",
                                                            GTK_TYPE_WRAP_MODE, GTK_WRAP_NONE,
                                                            (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_SHOW_LINE_NUMBERS] =
        moo_edit_config_install_setting (g_param_spec_boolean ("show-line-numbers", "show-line-numbers", "show-line-numbers",
                                                               FALSE,
                                                               (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_TAB_WIDTH] =
        moo_edit_config_install_setting (g_param_spec_uint ("tab-width", "tab-width", "tab-width",
                                                            1, G_MAXUINT, 8,
                                                            (GParamFlags) G_PARAM_READWRITE));
    _moo_edit_settings[MOO_EDIT_SETTING_WORD_CHARS] =
        moo_edit_config_install_setting (g_param_spec_string ("word-chars", "word-chars", "word-chars",
                                                              NULL, (GParamFlags) G_PARAM_READWRITE));
}


#define NEW_KEY_BOOL(s,v)    moo_prefs_new_key_bool (MOO_EDIT_PREFS_PREFIX "/" s, v)
#define NEW_KEY_INT(s,v)     moo_prefs_new_key_int (MOO_EDIT_PREFS_PREFIX "/" s, v)
#define NEW_KEY_STRING(s,v)  moo_prefs_new_key_string (MOO_EDIT_PREFS_PREFIX "/" s, v)

static void
_moo_edit_init_prefs (void)
{
    static gboolean done = FALSE;

    if (done)
        return;
    else
        done = TRUE;

    NEW_KEY_BOOL (MOO_EDIT_PREFS_SPACES_NO_TABS, FALSE);
    NEW_KEY_INT (MOO_EDIT_PREFS_INDENT_WIDTH, 8);
    NEW_KEY_INT (MOO_EDIT_PREFS_TAB_WIDTH, 8);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_AUTO_INDENT, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_TAB_INDENTS, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_BACKSPACE_INDENTS, TRUE);

    NEW_KEY_BOOL (MOO_EDIT_PREFS_SAVE_SESSION, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_AUTO_SAVE, FALSE);
    NEW_KEY_INT (MOO_EDIT_PREFS_AUTO_SAVE_INTERVAL, 5);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_MAKE_BACKUPS, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_STRIP, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_ADD_NEWLINE, FALSE);

    NEW_KEY_BOOL (MOO_EDIT_PREFS_USE_TABS, TRUE);

    NEW_KEY_STRING (MOO_EDIT_PREFS_TITLE_FORMAT, "%a - %f%s");
    NEW_KEY_STRING (MOO_EDIT_PREFS_TITLE_FORMAT_NO_DOC, "%a");

    NEW_KEY_BOOL (MOO_EDIT_PREFS_DIALOGS_OPEN_FOLLOWS_DOC, FALSE);

    NEW_KEY_BOOL (MOO_EDIT_PREFS_AUTO_SYNC, FALSE);

    NEW_KEY_STRING (MOO_EDIT_PREFS_COLOR_SCHEME, "medit");

    NEW_KEY_BOOL (MOO_EDIT_PREFS_SMART_HOME_END, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_WRAP_ENABLE, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_WRAP_WORDS, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_ENABLE_HIGHLIGHTING, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_HIGHLIGHT_MATCHING, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_HIGHLIGHT_MISMATCHING, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_HIGHLIGHT_CURRENT_LINE, TRUE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_DRAW_RIGHT_MARGIN, FALSE);
    NEW_KEY_INT (MOO_EDIT_PREFS_RIGHT_MARGIN_OFFSET, 80);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_SHOW_LINE_NUMBERS, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_SHOW_TABS, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_SHOW_SPACES, FALSE);
    NEW_KEY_BOOL (MOO_EDIT_PREFS_SHOW_TRAILING_SPACES, FALSE);
    NEW_KEY_STRING (MOO_EDIT_PREFS_FONT, DEFAULT_FONT);
    NEW_KEY_INT (MOO_EDIT_PREFS_QUICK_SEARCH_FLAGS, MOO_TEXT_SEARCH_CASELESS);
    NEW_KEY_STRING (MOO_EDIT_PREFS_LINE_NUMBERS_FONT, NULL);

    NEW_KEY_STRING (MOO_EDIT_PREFS_ENCODINGS, _moo_get_default_encodings ());
    NEW_KEY_STRING (MOO_EDIT_PREFS_ENCODING_SAVE, MOO_ENCODING_UTF8);
}


#define get_string(key) moo_prefs_get_string (MOO_EDIT_PREFS_PREFIX "/" key)
#define get_bool(key) moo_prefs_get_bool (MOO_EDIT_PREFS_PREFIX "/" key)
#define get_int(key) moo_prefs_get_int (MOO_EDIT_PREFS_PREFIX "/" key)

void
_moo_edit_update_global_config (void)
{
    gboolean use_tabs, strip, show_line_numbers, add_newline;
    int indent_width, tab_width;
    GtkWrapMode wrap_mode;

    use_tabs = !get_bool (MOO_EDIT_PREFS_SPACES_NO_TABS);
    indent_width = get_int (MOO_EDIT_PREFS_INDENT_WIDTH);
    tab_width = get_int (MOO_EDIT_PREFS_TAB_WIDTH);
    strip = get_bool (MOO_EDIT_PREFS_STRIP);
    add_newline = get_bool (MOO_EDIT_PREFS_ADD_NEWLINE);
    show_line_numbers = get_bool (MOO_EDIT_PREFS_SHOW_LINE_NUMBERS);

    if (get_bool (MOO_EDIT_PREFS_WRAP_ENABLE))
    {
        if (get_bool (MOO_EDIT_PREFS_WRAP_WORDS))
            wrap_mode = GTK_WRAP_WORD;
        else
            wrap_mode = GTK_WRAP_CHAR;
    }
    else
    {
        wrap_mode = GTK_WRAP_NONE;
    }

    moo_edit_config_set_global (MOO_EDIT_CONFIG_SOURCE_AUTO,
                                "indent-use-tabs", use_tabs,
                                "indent-width", indent_width,
                                "tab-width", tab_width,
                                "strip", strip,
                                "add-newline", add_newline,
                                "show-line-numbers", show_line_numbers,
                                "wrap-mode", wrap_mode,
                                nullptr);
}


/* Zoom is one for all the views and lasts until the editor quits: the font in
   the preferences stays as the user set it, and the zoom is added to it. */
#define MIN_FONT_POINTS 4
#define MAX_FONT_POINTS 96

static int zoom_points;

/* A font of the preferences may have no size ("Monospace"): then the view takes
   the one of the interface font, and so does the zoom. */
static int
font_size (PangoFontDescription *desc)
{
    int size = pango_font_description_get_size (desc);

    if (!size)
    {
        char *ui_font = NULL;
        PangoFontDescription *ui_desc;

        g_object_get (gtk_settings_get_default (), "gtk-font-name", &ui_font, nullptr);
        ui_desc = pango_font_description_from_string (ui_font ? ui_font : "");
        size = pango_font_description_get_size (ui_desc);
        pango_font_description_free (ui_desc);
        g_free (ui_font);
    }

    return size ? size : 10 * PANGO_SCALE;
}

static char *
zoomed_font (const char *name, int zoom)
{
    PangoFontDescription *desc = pango_font_description_from_string (name ? name : DEFAULT_FONT);
    int size = font_size (desc) + zoom * PANGO_SCALE;
    char *result;

    if (pango_font_description_get_size_is_absolute (desc))
        pango_font_description_set_absolute_size (desc, size);
    else
        pango_font_description_set_size (desc, size);

    result = pango_font_description_to_string (desc);
    pango_font_description_free (desc);
    return result;
}

void
_moo_edit_view_apply_font (MooEditView *view)
{
    char *font = zoom_points ? zoomed_font (get_string (MOO_EDIT_PREFS_FONT), zoom_points) : NULL;

    moo_text_view_set_font_from_string (MOO_TEXT_VIEW (view),
                                        font ? font : get_string (MOO_EDIT_PREFS_FONT));
    g_free (font);
}

/* Zero puts the size back. A step that would leave the font unreadable or absurd
   is not taken, so that the step back is not lost in the steps beyond. */
void
_moo_edit_zoom (int delta)
{
    if (delta)
    {
        const char *name = get_string (MOO_EDIT_PREFS_FONT);
        PangoFontDescription *desc = pango_font_description_from_string (name ? name : DEFAULT_FONT);
        int size = font_size (desc) + (zoom_points + delta) * PANGO_SCALE;

        pango_font_description_free (desc);

        if (size < MIN_FONT_POINTS * PANGO_SCALE || size > MAX_FONT_POINTS * PANGO_SCALE)
            return;

        zoom_points += delta;
    }
    else
    {
        zoom_points = 0;
    }

    _moo_edit_apply_font_all ();
}


void
_moo_edit_view_apply_prefs (MooEditView *view)
{
    MooLangMgr *mgr;
    MooTextStyleScheme *scheme;
    MooDrawWsFlags ws_flags = MOO_DRAW_WS_NONE;

    g_return_if_fail (MOO_IS_EDIT_VIEW (view));

    g_object_freeze_notify (G_OBJECT (view));

    mgr = moo_lang_mgr_default ();
    scheme = moo_lang_mgr_get_active_scheme (mgr);

    if (get_bool (MOO_EDIT_PREFS_SHOW_TABS))
        ws_flags |= MOO_DRAW_WS_TABS;
    if (get_bool (MOO_EDIT_PREFS_SHOW_SPACES))
        ws_flags |= MOO_DRAW_WS_SPACES;
    if (get_bool (MOO_EDIT_PREFS_SHOW_TRAILING_SPACES))
        ws_flags |= MOO_DRAW_WS_TRAILING;

    g_object_set (view,
                  "smart-home-end", get_bool (MOO_EDIT_PREFS_SMART_HOME_END),
                  "enable-highlight", get_bool (MOO_EDIT_PREFS_ENABLE_HIGHLIGHTING),
                  "highlight-matching-brackets", get_bool (MOO_EDIT_PREFS_HIGHLIGHT_MATCHING),
                  "highlight-mismatching-brackets", get_bool (MOO_EDIT_PREFS_HIGHLIGHT_MISMATCHING),
                  "highlight-current-line", get_bool (MOO_EDIT_PREFS_HIGHLIGHT_CURRENT_LINE),
                  "draw-right-margin", get_bool (MOO_EDIT_PREFS_DRAW_RIGHT_MARGIN),
                  "right-margin-offset", get_int (MOO_EDIT_PREFS_RIGHT_MARGIN_OFFSET),
                  "quick-search-flags", get_int (MOO_EDIT_PREFS_QUICK_SEARCH_FLAGS),
                  "auto-indent", get_bool (MOO_EDIT_PREFS_AUTO_INDENT),
                  "tab-indents", get_bool (MOO_EDIT_PREFS_TAB_INDENTS),
                  "backspace-indents", get_bool (MOO_EDIT_PREFS_BACKSPACE_INDENTS),
                  nullptr);

    g_object_set (view, "draw-whitespace", ws_flags, nullptr);

    _moo_edit_view_apply_font (view);
    _moo_text_view_set_line_numbers_font (MOO_TEXT_VIEW (view),
                                          get_string (MOO_EDIT_PREFS_LINE_NUMBERS_FONT));

    if (scheme)
        moo_text_view_set_style_scheme (MOO_TEXT_VIEW (view), scheme);

    g_object_thaw_notify (G_OBJECT (view));
}


const char *
moo_edit_setting (const char *setting_name)
{
#define STR_STACK_SIZE 4
    static GString *stack[STR_STACK_SIZE];
    static guint p;

    g_return_val_if_fail (setting_name != NULL, NULL);

    if (!stack[0])
    {
        for (p = 0; p < STR_STACK_SIZE; ++p)
            stack[p] = g_string_new ("");
        p = STR_STACK_SIZE - 1;
    }

    if (p == STR_STACK_SIZE - 1)
        p = 0;
    else
        p++;

    g_string_printf (stack[p], MOO_EDIT_PREFS_PREFIX "/%s", setting_name);
    return stack[p]->str;
#undef STR_STACK_SIZE
}
