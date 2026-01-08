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

#include "mooutils/mooi18n.h"

/*!
 * \brief Structure containing all widgets from the about dialog
 */
struct _AboutDialog
{
  GtkDialog *AboutDialog;      /*!< \brief Main dialog widget */
  GtkVBox *dialog_vbox;        /*!< \brief Dialog's main vbox */
  GtkVBox *vbox;               /*!< \brief Inner vbox with content */
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
};

/*!
 * \brief Creates the vbox with all content elements
 * \param dialog An AboutDialog structure
 */
static void
create_vbox_content (AboutDialog *dialog)
{
  GtkWidget *widget;

  /* Create main vbox with border and spacing */
  widget = gtk_vbox_new (FALSE, 8);
  dialog->vbox = GTK_VBOX (widget);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 12);
  gtk_widget_show (widget);

  /* Create logo image */
  widget = gtk_image_new_from_stock ("gtk-about", GTK_ICON_SIZE_DIALOG);
  dialog->logo = GTK_IMAGE (widget);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), widget, FALSE, FALSE, 0);

  /* Create application name label */
  widget = gtk_label_new (NULL);
  dialog->name = GTK_LABEL (widget);
  gtk_label_set_markup (dialog->name, "<span size=\"xx-large\"><b>The App</b></span>");
  gtk_label_set_selectable (dialog->name, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), widget, FALSE, FALSE, 0);

  /* Create description label */
  widget = gtk_label_new ("The App is an app");
  dialog->description = GTK_LABEL (widget);
  gtk_label_set_selectable (dialog->description, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), widget, FALSE, FALSE, 0);

  /* Create copyright label */
  widget = gtk_label_new (NULL);
  dialog->copyright = GTK_LABEL (widget);
  gtk_label_set_markup (dialog->copyright, "<small>\302\251 2004-2006 The Author</small>");
  gtk_label_set_selectable (dialog->copyright, TRUE);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), widget, FALSE, FALSE, 0);

  /* Create URL alignment */
  widget = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
  dialog->url_alignment = GTK_ALIGNMENT (widget);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), widget, FALSE, FALSE, 0);

  /* Create URL label */
  widget = gtk_label_new (NULL);
  dialog->url = GTK_LABEL (widget);
  gtk_label_set_markup (dialog->url, "<span foreground=\"#0000FF\">http://somesite.org</span>");
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (dialog->url_alignment), widget);
}

/*!
 * \brief Creates the action buttons
 * \param dialog An AboutDialog structure
 */
static void
create_action_buttons (AboutDialog *dialog)
{
  GtkWidget *widget;
  GtkWidget *alignment;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;

  /* Get the action area from the dialog */
#if GTK_CHECK_VERSION(3, 0, 0)
  dialog->action_box = GTK_HBUTTON_BOX (gtk_dialog_get_action_area (dialog->AboutDialog));
#else
  dialog->action_box = GTK_HBUTTON_BOX (dialog->AboutDialog->action_area);
#endif
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->action_box), GTK_BUTTONBOX_END);

  /* Create credits button */
  widget = gtk_button_new ();
  dialog->credits_button = GTK_BUTTON (widget);
  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  gtk_button_set_focus_on_click (GTK_BUTTON (widget), FALSE);
#endif

  /* Create alignment for credits button */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_widget_show (alignment);
  gtk_container_add (GTK_CONTAINER (widget), alignment);

  /* Create hbox for credits button */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (alignment), hbox);

  /* Create image for credits button */
  image = gtk_image_new_from_stock ("gtk-about", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  /* Create label for credits button */
  label = gtk_label_new_with_mnemonic ("C_redits");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  /* Add credits button to dialog */
  gtk_dialog_add_action_widget (dialog->AboutDialog, widget, 0);
  gtk_widget_show (widget);

  /* Create license button */
  widget = gtk_button_new_with_mnemonic ("_License");
  dialog->license_button = GTK_BUTTON (widget);
  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  gtk_button_set_focus_on_click (GTK_BUTTON (widget), FALSE);
#endif

  /* Add license button to dialog */
  gtk_dialog_add_action_widget (dialog->AboutDialog, widget, 0);
  gtk_widget_show (widget);

  /* Create close button */
  widget = gtk_button_new_from_stock ("gtk-close");
  dialog->close_button = GTK_BUTTON (widget);
  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_focus_on_click (widget, FALSE);
#else
  gtk_button_set_focus_on_click (GTK_BUTTON (widget), FALSE);
#endif

  /* Add close button to dialog */
  gtk_dialog_add_action_widget (dialog->AboutDialog, widget, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates a new AboutDialog structure with all widgets initialized
 * \param parent Parent widget for the dialog
 * \return A newly allocated AboutDialog structure
 */
AboutDialog *
about_dialog_new (GtkWidget *parent)
{
  GtkWindow *window;
  AboutDialog *dialog;

  dialog = g_new0 (AboutDialog, 1);

  /* Create main dialog */
  dialog->AboutDialog = GTK_DIALOG (gtk_dialog_new ());
  window = GTK_WINDOW (dialog->AboutDialog);

  gtk_window_set_title (window, _ ("About"));
  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_destroy_with_parent (window, TRUE);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);

#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator (dialog->AboutDialog, FALSE);
#endif

  /* Get the existing vbox from the dialog */
#if GTK_CHECK_VERSION(3, 0, 0)
  dialog->dialog_vbox = GTK_VBOX (gtk_dialog_get_content_area (dialog->AboutDialog));
#else
  dialog->dialog_vbox = GTK_VBOX (dialog->AboutDialog->vbox);
#endif

  /* Create and add vbox with content */
  create_vbox_content (dialog);
  gtk_box_pack_start (GTK_BOX (dialog->dialog_vbox), GTK_WIDGET (dialog->vbox), TRUE, TRUE, 0);

  /* Create action buttons */
  create_action_buttons (dialog);

  gtk_widget_show_all (GTK_WIDGET (dialog->dialog_vbox));

  if (parent)
    {
      gtk_window_set_transient_for (window, GTK_WINDOW (gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW)));
    }

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
