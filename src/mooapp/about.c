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

#include "mooapp-info.h"
#include "mooutils/mooi18n.h"

/*!
 * \brief Structure containing all widgets from the about dialog
 */
typedef struct
{
  GtkDialog *AboutDialog;      /*!< \brief Main dialog widget */
  GtkVBox *vbox;               /*!< \brief Dialog's main vbox */
  GtkImage *logo;              /*!< \brief Application logo image */
  GtkLabel *name;              /*!< \brief Application name label */
  GtkLabel *description;       /*!< \brief Application description label */
  GtkLabel *copyright;         /*!< \brief Copyright label */
  GtkAlignment *url_alignment; /*!< \brief Alignment for URL label */
  GtkLabel *url;               /*!< \brief Website URL label */
  GtkHButtonBox *action_box;   /*!< \brief Button box for dialog actions */
  GtkButton *credits_button;   /*!< \brief Credits button */
  GtkButton *license_button;   /*!< \brief License button */
  GtkButton *close_button;     /*!< \brief Close button */
} AboutDialog;

/*!
 * \brief Creates a new AboutDialog structure with all widgets initialized
 * \param parent Parent widget for the dialog
 * \return A newly allocated AboutDialog structure
 */
AboutDialog *
about_dialog_new (GtkWidget *parent)
{
  char *markup;
  GtkWindow *window;
  GtkWidget *widget;
  AboutDialog *dialog;
  GtkBox *vbox;
  GtkBox *box;
  GtkLabel *label;
  GtkWidget *image;
  GtkBox *abox;
  GtkButton *button;

#if !GTK_CHECK_VERSION(3, 0, 0)
  GdkPixbuf *pixbuf;
  GtkWidget *alignment;
#endif

  dialog = g_new0 (AboutDialog, 1);

  widget = gtk_dialog_new ();
  window = GTK_WINDOW (widget);
  dialog->AboutDialog = GTK_DIALOG (widget);

  gtk_window_set_title (window, _ ("About"));
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_destroy_with_parent (window, TRUE);

#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator (dialog->AboutDialog, FALSE);
#endif

  vbox = GTK_BOX (gtk_dialog_get_content_area (dialog->AboutDialog));
  abox = GTK_BOX (gtk_dialog_get_action_area (dialog->AboutDialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (abox), GTK_BUTTONBOX_END);

  /*
   * content area
   */

  widget = gtk_vbox_new (FALSE, 8);
  box = GTK_BOX (widget);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
  gtk_widget_show (widget);
  gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

  /* logo */
#if GTK_CHECK_VERSION(3, 0, 0)
  widget = gtk_image_new_from_resource ("/pixmap/medit.png");
#else
  pixbuf = gdk_pixbuf_new_from_resource ("/pixmap/medit.png", NULL);
  widget = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);
#endif

  gtk_widget_show (widget);
  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

  /* application name */
  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);
  gtk_label_set_selectable (label, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

  markup = g_markup_printf_escaped ("<span size=\"xx-large\"><b>%s %s</b></span>", MOO_APP_FULL_NAME, MOO_DISPLAY_VERSION);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  /* application description */
  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);
  gtk_label_set_text (label, MOO_APP_DESCRIPTION);
  gtk_label_set_selectable (label, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

  /* application copyright */
  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);
  gtk_label_set_selectable (label, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

  markup = g_markup_printf_escaped ("<small>\302\251 %s</small>", MOO_COPYRIGHT);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  /* application url */
  widget = gtk_label_new (NULL);
  label = GTK_LABEL (widget);
  gtk_label_set_selectable (label, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

  markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", MOO_APP_WEBSITE, MOO_APP_WEBSITE_LABEL);
  gtk_label_set_markup (label, markup);
  g_free (markup);

  /*
   * action area
   */

  /* credits button */
  widget = gtk_button_new_with_mnemonic ("C_redits");
  button = GTK_BUTTON (widget);
  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);

#if GTK_CHECK_VERSION(3, 0, 0)
  image = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (button, image);
  gtk_button_set_always_show_image (button, TRUE);

  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  // FIXME: Attempting to add a widget with type GtkAlignment to a GtkButton,
  // but as a GtkBin subclass a GtkButton can only contain one widget at a time;
  // it already contains a widget of type GtkLabel
  box = GTK_BOX (gtk_hbox_new (FALSE, 2));
  image = gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON);
  alignment = gtk_alignment_new (0.5, 0.5, 0, 0);

  gtk_container_add (GTK_CONTAINER (button), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET(box));
  gtk_box_pack_start (box, image, FALSE, FALSE, 0);
  gtk_box_pack_start (box, label, FALSE, FALSE, 0);

  gtk_button_set_focus_on_click (button, FALSE);
#endif

  gtk_widget_show (widget);
  gtk_box_pack_start (abox, widget, FALSE, FALSE, 0);

  /* Create and add content_box with content */
  // create_content_box_content (dialog);

  /* Create action buttons */
  // create_action_buttons (dialog);

  // gtk_widget_show_all (GTK_WIDGET (dialog->vbox));

  if (parent)
    gtk_window_set_transient_for (window, GTK_WINDOW (gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW)));

  gtk_widget_show (GTK_WIDGET (window));

  return dialog;
}

/*!
 * \brief Frees the AboutDialog structure and all its widgets
 * \param dialog An AboutDialog structure
 */
void
about_dialog_free (AboutDialog *dialog)
{
  if (dialog)
    {
      if (dialog->AboutDialog)
        gtk_widget_destroy (GTK_WIDGET (dialog->AboutDialog));

      g_free (dialog);
    }
}

void
show_about (GtkWidget *parent)
{
  AboutDialog *dialog = about_dialog_new (parent);
  gtk_dialog_run (dialog->AboutDialog);
  about_dialog_free (dialog);
}
