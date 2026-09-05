/*
 *   terminal-plugin.cpp
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *   Copyright (C) 2014 by Yannick Duchêne
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
 * The terminal pane, a port of the python plugin medit carried until 1.2.92.
 * vte dropped GTK+2 in 0.30, so this builds for the GTK+3 build only and the
 * whole file is compiled out when cmake does not find vte-2.91.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "plugins/terminal/terminal-plugin.h"
#include "plugins/terminal/terminal-colors.h"

#include "mooedit/mooplugin-macro.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooedit.h"
#include "mooedit/mooedit-accels.h"
#include "mooedit/mooeditview.h"
#include "mooutils/mooaccel.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moopane.h"
#include "mooutils/mooprefs.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-misc.h"

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <string.h>

/* What the python plugin asked for. vte allocates rows lazily. */
#define TERMINAL_SCROLLBACK_LINES 1000000

/* A shell that dies sooner than this counts as a failure to start. */
#define TERMINAL_QUICK_EXIT_USEC (G_TIME_SPAN_SECOND / 2)
#define TERMINAL_MAX_QUICK_EXITS 3

#define COLOR_SCHEME_DATA_KEY "moo-terminal-color-scheme"

typedef struct {
    MooPlugin parent;
    guint ui_merge_id;
} TerminalPlugin;

typedef struct {
    MooWinPlugin parent;

    MooEditWindow *window;
    VteTerminal *terminal;
    MooPane *pane;

    char *shell;
    gboolean shell_has_pushd;

    gint64 spawn_time;
    guint quick_exits;
    gboolean started;
    gboolean running;
    gboolean closing;
} TerminalWindowPlugin;

#define WindowStuff TerminalWindowPlugin

/*
 * Every live pane, so that a preference change can reach all of them and so
 * that the spawn callback can tell whether its pane is still around.
 */
static GSList *terminal_panes;

static void     start_shell     (WindowStuff    *stuff);
static void     apply_prefs     (WindowStuff    *stuff);


/**********************************************************************/
/* The shell
 */

static char *
get_shell (void)
{
    const char *pref = moo_prefs_get_string (MOO_TERMINAL_PREFS_SHELL);
    char *shell;

    if (pref && pref[0])
        return g_strdup (pref);

    shell = vte_get_user_shell ();

    if (shell && shell[0])
        return shell;

    g_free (shell);

    pref = g_getenv ("SHELL");

    return g_strdup (pref && pref[0] ? pref : "/bin/sh");
}


/*
 * There is no counterpart function to test whether or not a shell supports
 * the "cd" command: they all support it, even MS-DOS's command.com did.
 * If you happen to know another shell which supports "pushd", add its name.
 */
static gboolean
shell_supports_pushd (const char *shell)
{
    char *base = g_path_get_basename (shell);
    gboolean supports = strcmp (base, "bash") == 0;
    g_free (base);
    return supports;
}


static char *
get_document_dir (WindowStuff *stuff)
{
    MooEdit *doc = moo_edit_window_get_active_doc (stuff->window);
    char *filename, *dir;

    if (!doc)
        return NULL;

    filename = moo_edit_get_filename (doc);

    if (!filename)
        return NULL;

    dir = g_path_get_dirname (filename);
    g_free (filename);

    return dir;
}


static void
feed_message (WindowStuff *stuff,
              const char  *message)
{
    char *text = g_strdup_printf ("\r\n%s\r\n", message);
    vte_terminal_feed (stuff->terminal, text, -1);
    g_free (text);
}


static void
spawn_finished (G_GNUC_UNUSED VteTerminal *terminal,
                G_GNUC_UNUSED GPid pid,
                GError      *error,
                gpointer     user_data)
{
    WindowStuff *stuff = (WindowStuff*) user_data;

    /* The pane may have been destroyed while the spawn was in flight. The list
       holds every live pane, so a missing entry means stuff is gone. */
    if (!g_slist_find (terminal_panes, stuff))
        return;

    if (!error)
    {
        stuff->running = TRUE;
        return;
    }

    /* No child means no child-exited, so nothing will try to restart it; say
       what happened where the user is looking. */
    g_warning ("could not run the shell '%s': %s", stuff->shell, error->message);
    feed_message (stuff, error->message);
}


static void
start_shell (WindowStuff *stuff)
{
    char *argv[2];
    char *working_dir;

    g_return_if_fail (stuff->terminal != NULL);

    vte_terminal_reset (stuff->terminal, TRUE, TRUE);

    g_free (stuff->shell);
    stuff->shell = get_shell ();
    stuff->shell_has_pushd = shell_supports_pushd (stuff->shell);

    argv[0] = stuff->shell;
    argv[1] = NULL;

    /* The python plugin inherited medit's own directory, which for a desktop
       launch is the home directory. Start where the user is working instead. */
    working_dir = get_document_dir (stuff);

    stuff->spawn_time = g_get_monotonic_time ();
    stuff->running = FALSE;

    vte_terminal_spawn_async (stuff->terminal, VTE_PTY_DEFAULT,
                              working_dir, argv, NULL,
                              G_SPAWN_SEARCH_PATH,
                              NULL, NULL, NULL,
                              -1, NULL,
                              spawn_finished, stuff);

    g_free (working_dir);
}


static void
child_exited (G_GNUC_UNUSED VteTerminal *terminal,
              G_GNUC_UNUSED int status,
              WindowStuff   *stuff)
{
    gint64 lifetime = g_get_monotonic_time () - stuff->spawn_time;

    stuff->running = FALSE;

    if (stuff->closing)
        return;

    /*
     * The python plugin restarted the shell unconditionally, which turns a bad
     * Plugins/Terminal/shell into an endless fork loop. Give up after a few
     * shells in a row that died as soon as they started.
     */
    if (lifetime < TERMINAL_QUICK_EXIT_USEC)
        stuff->quick_exits += 1;
    else
        stuff->quick_exits = 0;

    if (stuff->quick_exits >= TERMINAL_MAX_QUICK_EXITS)
    {
        /* start_shell() would reset the terminal and wipe this out. Applying
           the preferences starts a shell again, which is what the message
           asks the user to do. */
        feed_message (stuff, _("The shell keeps exiting immediately, not restarting it. "
                               "Check the shell in the Terminal preferences."));
        return;
    }

    start_shell (stuff);
}


/*
 * The shell starts when the pane is first shown, not when the plugin is
 * created: a window plugin is attached before the window has a document, so at
 * creation time there is no directory to start in, and a user who never opens
 * the pane has no reason to pay for a shell at all.
 */
static void
terminal_map (G_GNUC_UNUSED GtkWidget *widget,
              WindowStuff *stuff)
{
    apply_prefs (stuff);

    if (stuff->started)
        return;

    stuff->started = TRUE;
    start_shell (stuff);
}


static void
window_title_changed (VteTerminal *terminal,
                      WindowStuff *stuff)
{
    const char *title = vte_terminal_get_window_title (terminal);
    moo_pane_set_frame_text (stuff->pane, title && title[0] ? title : _("Terminal"));
}


/**********************************************************************/
/* Preferences
 */

static void
apply_prefs (WindowStuff *stuff)
{
    const char *font_name;
    const char *scheme_name;

    g_return_if_fail (stuff->terminal != NULL);

    font_name = moo_prefs_get_string (MOO_TERMINAL_PREFS_FONT);

    if (font_name && font_name[0])
    {
        PangoFontDescription *font = pango_font_description_from_string (font_name);
        vte_terminal_set_font (stuff->terminal, font);
        pango_font_description_free (font);
    }
    else
    {
        /* NULL is vte's own default, the system monospace font */
        vte_terminal_set_font (stuff->terminal, NULL);
    }

    scheme_name = moo_prefs_get_string (MOO_TERMINAL_PREFS_COLOR_SCHEME);
    _moo_terminal_color_scheme_apply (_moo_terminal_color_scheme_lookup (scheme_name),
                                      stuff->terminal);
}


/*
 * With no font configured vte uses the system monospace font. The preferences
 * page needs its name, so that turning the default off starts from the font
 * the terminal actually shows rather than from whatever GtkFontButton holds.
 */
char *
_moo_terminal_get_default_font (void)
{
    GSList *l;

    for (l = terminal_panes; l != NULL; l = l->next)
    {
        WindowStuff *stuff = (WindowStuff*) l->data;
        const PangoFontDescription *font;

        if (!stuff->terminal)
            continue;

        font = vte_terminal_get_font (stuff->terminal);

        if (font)
            return pango_font_description_to_string (font);
    }

    return g_strdup ("Monospace 10");
}


static gboolean
monospace_only (const PangoFontFamily *family,
                G_GNUC_UNUSED const PangoFontFace *face,
                G_GNUC_UNUSED gpointer data)
{
    return pango_font_family_is_monospace (const_cast<PangoFontFamily*> (family));
}


/* What the python plugin wanted from GtkFontSelection and could not get. */
void
_moo_terminal_font_chooser_filter (GtkWidget *chooser)
{
    g_return_if_fail (GTK_IS_FONT_CHOOSER (chooser));
    gtk_font_chooser_set_filter_func (GTK_FONT_CHOOSER (chooser),
                                      monospace_only, NULL, NULL);
}


void
_moo_terminal_apply_prefs (void)
{
    GSList *l;

    for (l = terminal_panes; l != NULL; l = l->next)
    {
        WindowStuff *stuff = (WindowStuff*) l->data;

        apply_prefs (stuff);

        /* A pane whose shell died and was not restarted, because it kept
           failing or because it could not be run at all, comes back here: the
           user has just been told to fix the shell in these preferences. */
        if (stuff->started && !stuff->running)
        {
            stuff->quick_exits = 0;
            start_shell (stuff);
        }
    }
}


/**********************************************************************/
/* Context menu
 */

static void
copy_clipboard (WindowStuff *stuff)
{
    vte_terminal_copy_clipboard_format (stuff->terminal, VTE_FORMAT_TEXT);
}


static void
paste_clipboard (WindowStuff *stuff)
{
    vte_terminal_paste_clipboard (stuff->terminal);
}


static void
goto_document_dir (WindowStuff *stuff,
                   gboolean     pushd)
{
    char *dir, *quoted, *command;

    /*
     * TODO: be able to check if the user was editing a command. Would be nice
     * to not append to a command being edited and to only send this command
     * when there is nothing entered at the current shell prompt.
     */
    dir = get_document_dir (stuff);

    if (!dir)
        return;

    quoted = g_shell_quote (dir);
    command = g_strdup_printf ("%s %s\n", pushd ? "pushd" : "cd", quoted);

    vte_terminal_feed_child (stuff->terminal, command, -1);

    g_free (command);
    g_free (quoted);
    g_free (dir);
}


static void
cd_item_activated (G_GNUC_UNUSED GtkWidget *item,
                   WindowStuff *stuff)
{
    goto_document_dir (stuff, FALSE);
}


static void
pushd_item_activated (G_GNUC_UNUSED GtkWidget *item,
                      WindowStuff *stuff)
{
    goto_document_dir (stuff, TRUE);
}


static void
copy_item_activated (G_GNUC_UNUSED GtkWidget *item,
                     WindowStuff *stuff)
{
    copy_clipboard (stuff);
}


static void
paste_item_activated (G_GNUC_UNUSED GtkWidget *item,
                      WindowStuff *stuff)
{
    paste_clipboard (stuff);
}


static void
font_item_activated (G_GNUC_UNUSED GtkWidget *item,
                     WindowStuff *stuff)
{
    GtkWidget *dialog;
    const char *font_name;
    const PangoFontDescription *current;

    /* gtk carries this string and its translations; the python plugin borrowed
       them the same way, from gtk20 rather than gtk30 */
    dialog = gtk_font_chooser_dialog_new (D_("Pick a Font", "gtk30"),
                                          GTK_WINDOW (stuff->window));
    _moo_terminal_font_chooser_filter (dialog);

    font_name = moo_prefs_get_string (MOO_TERMINAL_PREFS_FONT);
    current = vte_terminal_get_font (stuff->terminal);

    if (font_name && font_name[0])
    {
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font_name);
    }
    else if (current)
    {
        char *string = pango_font_description_to_string (current);
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), string);
        g_free (string);
    }

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        char *chosen = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

        if (chosen)
        {
            moo_prefs_set_string (MOO_TERMINAL_PREFS_FONT, chosen);
            _moo_terminal_apply_prefs ();
            g_free (chosen);
        }
    }

    gtk_widget_destroy (dialog);
}


static void
color_scheme_item_toggled (GtkCheckMenuItem *item,
                           G_GNUC_UNUSED gpointer data)
{
    const MooTerminalColorScheme *scheme;

    if (!gtk_check_menu_item_get_active (item))
        return;

    scheme = (const MooTerminalColorScheme*)
        g_object_get_data (G_OBJECT (item), COLOR_SCHEME_DATA_KEY);
    g_return_if_fail (scheme != NULL);

    moo_prefs_set_string (MOO_TERMINAL_PREFS_COLOR_SCHEME, scheme->name);
    _moo_terminal_apply_prefs ();
}


static GtkWidget *
create_color_scheme_menu (void)
{
    GtkWidget *menu = gtk_menu_new ();
    const MooTerminalColorScheme *schemes;
    const MooTerminalColorScheme *current;
    GSList *group = NULL;
    guint n_schemes, i;

    schemes = _moo_terminal_color_schemes (&n_schemes);
    current = _moo_terminal_color_scheme_lookup (
        moo_prefs_get_string (MOO_TERMINAL_PREFS_COLOR_SCHEME));

    for (i = 0; i < n_schemes; ++i)
    {
        GtkWidget *item = gtk_radio_menu_item_new_with_label (group, _(schemes[i].display_name));

        group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
        g_object_set_data (G_OBJECT (item), COLOR_SCHEME_DATA_KEY,
                           (gpointer) &schemes[i]);

        /* set the state before connecting, so that building the menu does not
           write the scheme back into the prefs */
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                        &schemes[i] == current);
        g_signal_connect (item, "toggled",
                          G_CALLBACK (color_scheme_item_toggled), NULL);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    return menu;
}


static GtkWidget *
create_popup_menu (WindowStuff *stuff)
{
    GtkWidget *menu, *item, *submenu;

    menu = gtk_menu_new ();

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
    gtk_widget_set_sensitive (item, vte_terminal_get_has_selection (stuff->terminal));
    g_signal_connect (item, "activate", G_CALLBACK (copy_item_activated), stuff);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PASTE, NULL);
    g_signal_connect (item, "activate", G_CALLBACK (paste_item_activated), stuff);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    item = gtk_menu_item_new_with_label (_("\342\200\234cd\342\200\235 to current file directory"));
    gtk_widget_set_sensitive (item, moo_edit_window_get_active_doc (stuff->window) != NULL);
    g_signal_connect (item, "activate", G_CALLBACK (cd_item_activated), stuff);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    if (stuff->shell_has_pushd)
    {
        item = gtk_menu_item_new_with_label (_("\342\200\234pushd\342\200\235 to current file directory"));
        gtk_widget_set_sensitive (item, moo_edit_window_get_active_doc (stuff->window) != NULL);
        g_signal_connect (item, "activate", G_CALLBACK (pushd_item_activated), stuff);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    submenu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_SELECT_FONT, NULL);
    g_signal_connect (item, "activate", G_CALLBACK (font_item_activated), stuff);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_SELECT_COLOR, NULL);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), create_color_scheme_menu ());
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

    g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);
    gtk_widget_show_all (menu);

    return menu;
}


static gboolean
terminal_button_press (G_GNUC_UNUSED GtkWidget *widget,
                       GdkEventButton *event,
                       WindowStuff    *stuff)
{
    if (event->type != GDK_BUTTON_PRESS || event->button != 3)
        return FALSE;

    gtk_widget_grab_focus (GTK_WIDGET (stuff->terminal));
    gtk_menu_popup_at_pointer (GTK_MENU (create_popup_menu (stuff)), (GdkEvent*) event);

    return TRUE;
}


static gboolean
terminal_popup_menu (G_GNUC_UNUSED GtkWidget *widget,
                     WindowStuff *stuff)
{
    gtk_menu_popup_at_widget (GTK_MENU (create_popup_menu (stuff)),
                              GTK_WIDGET (stuff->terminal),
                              GDK_GRAVITY_CENTER, GDK_GRAVITY_CENTER, NULL);
    return TRUE;
}


/*
 * MooWindow hands a key to the focused widget before the accelerators, so while
 * the terminal has the focus every editor shortcut reaches the shell instead.
 * That is what a terminal is for, but it also means the pane's own accelerator
 * never fires and there is no way back to the document without the mouse, so
 * take that one key here and let it toggle the focus.
 */
static gboolean
terminal_accel_pressed (WindowStuff *stuff,
                        GdkEventKey *event)
{
    GtkAction *action;
    const char *accel_path;
    const char *accel;
    guint key;
    GdkModifierType mods;

    action = moo_window_get_action (MOO_WINDOW (stuff->window), "ShowTerminal");

    if (!action)
        return FALSE;

    accel_path = gtk_action_get_accel_path (action);
    accel = accel_path ? _moo_get_accel (accel_path) : NULL;

    if (!accel || !accel[0] || !_moo_accel_parse (accel, &key, &mods))
        return FALSE;

    return moo_accel_check_event (GTK_WIDGET (stuff->terminal), event, key, mods);
}


/*
 * Ctrl-C and Ctrl-V belong to the shell, so copy and paste move one modifier
 * up, the way every terminal emulator does it.
 */
static gboolean
terminal_key_press (G_GNUC_UNUSED GtkWidget *widget,
                    GdkEventKey *event,
                    WindowStuff *stuff)
{
    GdkModifierType mods;

    if (terminal_accel_pressed (stuff, event))
    {
        MooEditView *view = moo_edit_window_get_active_view (stuff->window);

        if (view)
            gtk_widget_grab_focus (GTK_WIDGET (view));

        return TRUE;
    }

    mods = (GdkModifierType) (event->state & gtk_accelerator_get_default_mod_mask ());

    if (mods != (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
        return FALSE;

    switch (event->keyval)
    {
        case GDK_KEY_C:
        case GDK_KEY_c:
            copy_clipboard (stuff);
            return TRUE;

        case GDK_KEY_V:
        case GDK_KEY_v:
            paste_clipboard (stuff);
            return TRUE;

        default:
            return FALSE;
    }
}


/**********************************************************************/
/* The plugin
 */

static void
show_terminal_cb (MooEditWindow *window)
{
    WindowStuff *stuff = (WindowStuff*) moo_win_plugin_lookup (MOO_TERMINAL_PLUGIN_ID, window);

    if (!stuff || !stuff->pane)
        return;

    moo_edit_window_show_pane (window, MOO_TERMINAL_PLUGIN_ID);
    gtk_widget_grab_focus (GTK_WIDGET (stuff->terminal));
}


static gboolean
terminal_window_plugin_create (WindowStuff *stuff)
{
    GtkWidget *swin;
    MooPaneLabel *label;

    stuff->window = MOO_WIN_PLUGIN (stuff)->window;
    stuff->terminal = VTE_TERMINAL (vte_terminal_new ());

    vte_terminal_set_scrollback_lines (stuff->terminal, TERMINAL_SCROLLBACK_LINES);

    /*
     * VteTerminal asks for eighty columns by twenty-four rows, which no pane at
     * the bottom of a window can grant; without this the pane opens huge and
     * cannot be dragged smaller. The python plugin did the same two calls.
     */
    vte_terminal_set_size (stuff->terminal,
                           vte_terminal_get_column_count (stuff->terminal), 10);
    gtk_widget_set_size_request (GTK_WIDGET (stuff->terminal), 10, 10);

    swin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (stuff->terminal));
    gtk_widget_show_all (swin);

    g_signal_connect (stuff->terminal, "map",
                      G_CALLBACK (terminal_map), stuff);
    g_signal_connect (stuff->terminal, "child-exited",
                      G_CALLBACK (child_exited), stuff);
    g_signal_connect (stuff->terminal, "window-title-changed",
                      G_CALLBACK (window_title_changed), stuff);
    g_signal_connect (stuff->terminal, "button-press-event",
                      G_CALLBACK (terminal_button_press), stuff);
    g_signal_connect (stuff->terminal, "popup-menu",
                      G_CALLBACK (terminal_popup_menu), stuff);
    g_signal_connect (stuff->terminal, "key-press-event",
                      G_CALLBACK (terminal_key_press), stuff);

    label = moo_pane_label_new (MOO_STOCK_TERMINAL, NULL,
                                _("Terminal"), _("Terminal"));
    stuff->pane = moo_edit_window_add_pane (stuff->window, MOO_TERMINAL_PLUGIN_ID,
                                            swin, label, MOO_PANE_POS_BOTTOM);
    moo_pane_label_free (label);

    /* before the first spawn: the spawn callback looks stuff up in this list */
    terminal_panes = g_slist_prepend (terminal_panes, stuff);

    return TRUE;
}


static void
terminal_window_plugin_destroy (WindowStuff *stuff)
{
    stuff->closing = TRUE;
    terminal_panes = g_slist_remove (terminal_panes, stuff);

    if (stuff->terminal)
        g_signal_handlers_disconnect_by_data (stuff->terminal, stuff);

    stuff->terminal = NULL;
    stuff->pane = NULL;

    moo_edit_window_remove_pane (stuff->window, MOO_TERMINAL_PLUGIN_ID);

    g_free (stuff->shell);
    stuff->shell = NULL;
}


static gboolean
terminal_plugin_init (TerminalPlugin *plugin)
{
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditor *editor = moo_editor_instance ();
    MooUiXml *xml = moo_editor_get_ui_xml (editor);

    g_return_val_if_fail (klass != NULL, FALSE);

    moo_prefs_new_key_string (MOO_TERMINAL_PREFS_COLOR_SCHEME, NULL);
    moo_prefs_new_key_string (MOO_TERMINAL_PREFS_SHELL, NULL);
    moo_prefs_new_key_string (MOO_TERMINAL_PREFS_FONT, NULL);

    moo_window_class_new_action (klass, "ShowTerminal", NULL,
                                 "display-name", _("Terminal"),
                                 "label", _("Terminal"),
                                 "tooltip", _("Show the terminal pane"),
                                 "default-accel", MOO_EDIT_ACCEL_TERMINAL,
                                 "stock-id", MOO_STOCK_TERMINAL,
                                 "closure-callback", show_terminal_cb,
                                 nullptr);

    if (xml)
    {
        plugin->ui_merge_id = moo_ui_xml_new_merge_id (xml);
        moo_ui_xml_add_item (xml, plugin->ui_merge_id,
                             "Editor/Menubar/Tools",
                             "ShowTerminal", "ShowTerminal", -1);
    }

    g_type_class_unref (klass);
    return TRUE;
}


static void
terminal_plugin_deinit (TerminalPlugin *plugin)
{
    MooWindowClass *klass = (MooWindowClass*) g_type_class_ref (MOO_TYPE_EDIT_WINDOW);
    MooEditor *editor = moo_editor_instance ();
    MooUiXml *xml = moo_editor_get_ui_xml (editor);

    moo_window_class_remove_action (klass, "ShowTerminal");

    if (plugin->ui_merge_id)
        moo_ui_xml_remove_ui (xml, plugin->ui_merge_id);
    plugin->ui_merge_id = 0;

    g_type_class_unref (klass);
}


MOO_PLUGIN_DEFINE_INFO (terminal,
                        N_("Terminal"), N_("Terminal pane"),
                        "Yevgen Muntyan <emuntyan@users.sourceforge.net>",
                        MOO_VERSION)
MOO_WIN_PLUGIN_DEFINE (Terminal, terminal)
MOO_PLUGIN_DEFINE (Terminal, terminal,
                   NULL, NULL, NULL, NULL,
                   _moo_terminal_prefs_page,
                   terminal_window_plugin_get_type (), 0)


gboolean
moo_terminal_plugin_init (void)
{
    return moo_plugin_register (MOO_TERMINAL_PLUGIN_ID,
                                terminal_plugin_get_type (),
                                &terminal_plugin_info,
                                NULL);
}
