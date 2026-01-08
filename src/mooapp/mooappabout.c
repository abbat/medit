/*
 *   mooapp/mooappabout.c
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

#include "mooappabout.h"
#include "credits.h"
#include "mooapp.h"
#include "mooapp-credits.h"
#include "mooapp-info.h"
#include "moohtml.h"
#include "moolinklabel.h"
#include "mooutils/moostock.h"
#include "mooutils/mooutils-misc.h"
#include "mooutils/mooglade.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moodialogs.h"
#include "mooutils/moohelp.h"
#ifdef MOO_ENABLE_HELP
#include "moo-help-sections.h"
#endif
#include "mooapp/mooappabout-dialog-gxml.h"
#include "mooapp/mooappabout-license-gxml.h"
#include "mooapp/mooappabout-credits-gxml.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <libxml/xmlversion.h>

static gpointer about_dialog;
static gpointer credits_dialog;

#undef MOO_USE_HTML
#define MOO_USE_HTML 1

#ifndef MOO_USE_HTML
#define MooHtml GtkTextView
#undef MOO_TYPE_HTML
#define MOO_TYPE_HTML GTK_TYPE_TEXT_VIEW
#endif

static void
set_translator_credits (CreditsDialogXml *gxml)
{
    const char *credits, *credits_markup;

    /* Translators: this goes into About box, under Translated by tab,
       do not ignore it, markup isn't always used. It should be something
       like "Some Guy <someguy@domain.net>", with lines separated by \n */
    credits = _("translator-credits");
    /* Translators: this goes into About box, under Translated by tab,
       this must be valid html markup, e.g.
       "Some Guy <a href=\"mailto:someguy@domain.net\">&lt;someguy@domain.net&gt;</a>"
       Lines must be separated by <br>, like "First guy<br>Second Guy" */
    credits_markup = _("translator-credits-markup");

    if (!strcmp (credits, "translator-credits"))
    {
        gtk_widget_hide (GTK_WIDGET (gxml->tab_translated_by));
        return;
    }

#ifdef MOO_USE_HTML
    if (strcmp (credits_markup, "translator-credits-markup") != 0)
        _moo_html_load_memory (GTK_TEXT_VIEW (gxml->view_translated_by),
                               credits_markup, -1, NULL, NULL);
    else
#endif
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gxml->view_translated_by));
        gtk_text_buffer_insert_at_cursor (buffer, credits, -1);
    }
}

static void
show_credits (void)
{
    GtkTextBuffer *buffer;
    CreditsDialogXml *gxml;
    //CreditsDialog *credits;

    //credits = credits_dialog_new (GTK_WINDOW (about_dialog));
    //return;

    if (credits_dialog)
    {
        if (about_dialog)
            moo_window_set_parent (GTK_WIDGET (credits_dialog), GTK_WIDGET (about_dialog));

        gtk_window_present (GTK_WINDOW (credits_dialog));

        return;
    }

#ifdef MOO_USE_HTML
    moo_glade_xml_register_type (MOO_TYPE_HTML);
    gxml = credits_dialog_xml_new ();
#else
    gxml = credits_dialog_xml_new_empty ();
    moo_glade_xml_map_class (gxml->xml, "MooHtml", MOO_TYPE_HTML);
    credits_dialog_xml_build (gxml);
#endif

    credits_dialog = gxml->CreditsDialog;
    g_return_if_fail (credits_dialog != NULL);
    g_object_add_weak_pointer (G_OBJECT (credits_dialog), &credits_dialog);
    g_signal_connect (credits_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

#ifdef MOO_USE_HTML
    _moo_html_load_memory (GTK_TEXT_VIEW (gxml->view_written_by),
                           "Yevgen Muntyan <a href=\"mailto://" MOO_EMAIL "\">&lt;" MOO_EMAIL "&gt;</a>",
                           -1, NULL, NULL);
#else
    buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (gxml->view_written_by));
    gtk_text_buffer_insert_at_cursor (buffer, "Yevgen Muntyan <" MOO_EMAIL ">", -1);
#endif

    set_translator_credits (gxml);

    buffer = gtk_text_view_get_buffer (gxml->view_thanks);
    gtk_text_buffer_insert_at_cursor (buffer, MOO_APP_CREDITS, -1);

    if (about_dialog)
        moo_window_set_parent (GTK_WIDGET (credits_dialog), GTK_WIDGET (about_dialog));

    gtk_window_present (GTK_WINDOW (credits_dialog));
}


static void
license_clicked (void)
{
#ifdef MOO_ENABLE_HELP
    moo_help_open_id (HELP_SECTION_APP_LICENSE, NULL);
#endif
}


static gboolean
about_dialog_key_press (GtkWidget   *dialog,
                        GdkEventKey *event)
{
    if (event->keyval == GDK_KEY_Escape)
    {
        gtk_widget_hide (dialog);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_s && (event->state & GDK_CONTROL_MASK))
    {
        char *info = moo_app_get_system_info (moo_app_instance ());
        if (info)
            gtk_clipboard_set_text (gtk_widget_get_clipboard (dialog, GDK_SELECTION_CLIPBOARD), info, -1);
        g_free (info);
    }

    return FALSE;
}


static GtkWidget *
create_about_dialog (void)
{
    AboutDialogXml *gxml;
    GtkWidget *dialog;
    char *markup;
    GtkStockItem dummy;

    moo_glade_xml_register_type (MOO_TYPE_LINK_LABEL);
    gxml = about_dialog_xml_new ();

    dialog = GTK_WIDGET (gxml->AboutDialog);
    g_signal_connect (dialog, "key-press-event", G_CALLBACK (about_dialog_key_press), NULL);

    g_object_add_weak_pointer (G_OBJECT (dialog), &about_dialog);
    g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

    markup = g_markup_printf_escaped ("<span size=\"xx-large\"><b>%s %s</b></span>",
                                      MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);
    gtk_label_set_markup (gxml->name, markup);
    g_free (markup);

    gtk_label_set_text (gxml->description, MOO_APP_DESCRIPTION);

    markup = g_markup_printf_escaped ("<small>\302\251 %s</small>", MOO_COPYRIGHT);
    gtk_label_set_markup (gxml->copyright, markup);
    g_free (markup);

    _moo_link_label_set_url (gxml->url, MOO_APP_WEBSITE);
    _moo_link_label_set_text (gxml->url, MOO_APP_WEBSITE_LABEL);

    if (gtk_stock_lookup (MOO_APP_LOGO, &dummy))
        gtk_image_set_from_stock (gxml->logo, MOO_APP_LOGO,
                                  GTK_ICON_SIZE_DIALOG);
    else
        gtk_image_set_from_icon_name (gxml->logo, MOO_APP_LOGO,
                                      GTK_ICON_SIZE_DIALOG);

    g_signal_connect (gxml->credits_button, "clicked", G_CALLBACK (show_credits), NULL);
    g_signal_connect (gxml->license_button, "clicked", G_CALLBACK (license_clicked), NULL);
    g_signal_connect_swapped (gxml->close_button, "clicked", G_CALLBACK (gtk_widget_hide), dialog);

    return dialog;
}


void
moo_app_about_dialog (GtkWidget *parent)
{
    if (!about_dialog)
        about_dialog = create_about_dialog ();

    if (parent)
        parent = gtk_widget_get_toplevel (parent);

    if (parent && GTK_IS_WINDOW (parent))
        moo_window_set_parent (GTK_WIDGET (about_dialog), parent);

    gtk_window_present (GTK_WINDOW (about_dialog));
}
