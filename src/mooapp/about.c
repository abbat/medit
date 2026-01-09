/*
 *   mooapp/about.c
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

#include "about.h"

#include "credits.h"
#include "mooapp-info.h"
#include "mooutils/mooi18n.h"

/*!
 * \brief Callback function for credits button click event
 * \param widget The button widget that triggered the event
 * \param data The dialog widget
 */
static void
on_credits_button_clicked (GtkWidget *widget, gpointer data)
{
  (void) widget;
  show_credits_dialog (GTK_WIDGET (data));
}

/*!
 * \brief Callback function for license button click event
 * \param widget The button widget that triggered the event
 * \param data The dialog widget
 */
static void
on_license_button_clicked (GtkWidget *widget, gpointer data)
{
  (void) widget;
  (void) data;
  gtk_show_uri (NULL, "https://github.com/abbat/medit/blob/main/COPYING", GDK_CURRENT_TIME, NULL);
}

/*!
 * \brief Callback function for close button click event
 * \param widget The button widget that triggered the event
 * \param data The dialog widget
 */
static void
on_close_button_clicked (GtkWidget *widget, gpointer data)
{
  (void) widget;
  gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_CLOSE);
}

/*!
 * \brief Creates and adds the application logo image to the dialog
 * \param box The box container to add the logo to
 */
static void
create_logo_image (GtkBox *box)
{
  GtkWidget *widget;
  const char *resource_name = "/pixmap/medit.png";

#if GTK_CHECK_VERSION(3, 0, 0)
  widget = gtk_image_new_from_resource (resource_name);
#else
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_resource (resource_name, NULL);
  widget = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);
#endif

  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates and adds the application name label with version
 * \param box The box container to add the label to
 */
static void
create_application_name_label (GtkBox *box)
{
  char *markup;
  GtkLabel *label;
  GtkWidget *widget;

  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);

  gtk_label_set_selectable (label, TRUE);

  markup = g_markup_printf_escaped ("<span size=\"xx-large\"><b>%s %s</b></span>", MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates and adds the application description label
 * \param box The box container to add the label to
 */
static void
create_application_description_label (GtkBox *box)
{
  GtkLabel *label;
  GtkWidget *widget;

  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);

  gtk_label_set_text (label, MOO_APP_DESCRIPTION);
  gtk_label_set_selectable (label, TRUE);

  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates and adds the application copyright label
 * \param box The box container to add the label to
 */
static void
create_application_copyright_label (GtkBox *box)
{
  char *markup;
  GtkLabel *label;
  GtkWidget *widget;

  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);

  gtk_label_set_selectable (label, TRUE);

  markup = g_markup_printf_escaped ("<small>\302\251 %s</small>", MOO_COPYRIGHT);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates and adds the application website URL label
 * \param box The box container to add the label to
 */
static void
create_application_url_label (GtkBox *box)
{
  char *markup;
  GtkLabel *label;
  GtkWidget *widget;

  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);

  gtk_label_set_selectable (label, TRUE);

  markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", MOO_APP_WEBSITE, MOO_APP_WEBSITE_LABEL);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates the content area of the about dialog
 * \param dialog The dialog to create the content area for
 */
static void
create_content_area (GtkDialog *dialog)
{
  GtkBox *box;
  GtkBox *vbox;
  GtkWidget *widget;

  vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));

  widget = gtk_vbox_new (FALSE, 8);
  box = GTK_BOX (widget);

  gtk_container_set_border_width (GTK_CONTAINER (widget), 12);

  create_logo_image (box);
  create_application_name_label (box);
  create_application_description_label (box);
  create_application_copyright_label (box);
  create_application_url_label (box);

  gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates and adds the credits button to the dialog
 * \param hbox The action box to add the button to
 */
static void
create_credits_button (GtkDialog *dialog, GtkBox *hbox)
{
  GtkWidget *image;
  GtkButton *button;
  GtkWidget *widget;
  const char *mnemonic = _ ("C_redits");

#if !GTK_CHECK_VERSION(3, 0, 0)
  GtkBox *bbox;
  GtkWidget *wbox;
  GtkWidget *label;
  GtkWidget *alignment;
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
  widget = gtk_button_new_with_mnemonic (mnemonic);
  button = GTK_BUTTON (widget);
  image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_BUTTON);

  gtk_button_set_image (button, image);
  gtk_button_set_always_show_image (button, TRUE);
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  widget = gtk_button_new ();
  button = GTK_BUTTON (widget);

  gtk_button_set_focus_on_click (button, FALSE);

  image = gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON);
  label = gtk_label_new_with_mnemonic (mnemonic);
  alignment = gtk_alignment_new (0.5, 0.5, 0, 0);

  wbox = gtk_hbox_new (FALSE, 2);
  bbox = GTK_BOX (wbox);

  gtk_box_pack_start (bbox, image, FALSE, FALSE, 0);
  gtk_box_pack_start (bbox, label, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (button), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET (bbox));

  gtk_widget_show (wbox);
  gtk_widget_show (image);
  gtk_widget_show (label);
  gtk_widget_show (alignment);
#endif

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);

  gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (button, "clicked", G_CALLBACK (on_credits_button_clicked), dialog);
}

/*!
 * \brief Creates and adds the license button to the dialog
 * \param hbox The action box to add the button to
 */
static void
create_license_button (GtkDialog *dialog, GtkBox *hbox)
{
  GtkButton *button;
  GtkWidget *widget;
  const char *mnemonic = _ ("_License");

  widget = gtk_button_new_with_mnemonic (mnemonic);
  button = GTK_BUTTON (widget);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  gtk_button_set_focus_on_click (button, FALSE);
#endif

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
  gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (button, "clicked", G_CALLBACK (on_license_button_clicked), dialog);
}

/*!
 * \brief Creates and adds the close button to the dialog
 * \param dialog The dialog to close when button is clicked
 * \param hbox The action box to add the button to
 */
static void
create_close_button (GtkDialog *dialog, GtkBox *hbox)
{
  GtkButton *button;
  GtkWidget *widget;

#if GTK_CHECK_VERSION(3, 0, 0)
  GtkWidget *image;
  const char *mnemonic = _ ("_Close");

  widget = gtk_button_new_with_mnemonic (mnemonic);
  button = GTK_BUTTON (widget);
  image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_BUTTON);

  gtk_button_set_image (button, image);
  gtk_button_set_always_show_image (button, TRUE);
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  button = GTK_BUTTON (widget);

  gtk_button_set_focus_on_click (button, FALSE);
#endif

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
  gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (button, "clicked", G_CALLBACK (on_close_button_clicked), dialog);
}

/*!
 * \brief Creates the action area of the about dialog
 * \param dialog The dialog to create the action area for
 */
static void
create_action_area (GtkDialog *dialog)
{
  GtkBox *hbox;

  hbox = GTK_BOX (gtk_dialog_get_action_area (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);

  create_credits_button (dialog, hbox);
  create_license_button (dialog, hbox);
  create_close_button (dialog, hbox);
}

/*!
 * \brief Creates a new about dialog with all widgets initialized
 * \param parent Parent widget for the dialog
 * \return A newly created GtkDialog widget
 */
static GtkDialog *
about_dialog_new (GtkWidget *parent)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  GtkWindow *window;

  widget = gtk_dialog_new ();
  dialog = GTK_DIALOG (widget);
  window = GTK_WINDOW (widget);

  gtk_window_set_title (window, _ ("About"));
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_destroy_with_parent (window, TRUE);

#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator (dialog, FALSE);
#endif

  create_content_area (dialog);
  create_action_area (dialog);

  if (parent)
    gtk_window_set_transient_for (window, GTK_WINDOW (gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW)));

  gtk_widget_show (widget);

  return dialog;
}

/*!
 * \brief Shows the about dialog
 * \param parent Parent widget for the dialog
 */
void
show_about (GtkWidget *parent)
{
  // FIXME: on ESC key press on gtk-3 raise
  // gtk_widget_event: assertion 'WIDGET_REALIZED_FOR_EVENT (widget, event)'
  GtkDialog *dialog = about_dialog_new (parent);
  gtk_dialog_run (dialog);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}
