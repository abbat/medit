/*
 *   mooapp/license.c
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

#include "license.h"

#include "mooutils/mooi18n.h"

/*!
 * \brief Structure containing all widgets from the license dialog
 */
struct _LicenseDialog
{
  GtkDialog *LicenseDialog;           /*!< \brief Main dialog widget */
  GtkVBox *vbox;                      /*!< \brief Dialog's main vbox */
  GtkScrolledWindow *scrolled_window; /*!< \brief Scrolled window for license text */
  GtkTextView *textview;              /*!< \brief Text view for license text */
  GtkHButtonBox *action_box;          /*!< \brief Button box for dialog actions */
  GtkButton *button_close;            /*!< \brief Close button */
};

/*!
 * \brief Creates the scrolled window with text view for license text
 * \param dialog A LicenseDialog structure
 */
static void
create_scrolled_window (LicenseDialog *dialog)
{
  GtkWidget *widget;

  widget = gtk_scrolled_window_new (NULL, NULL);
  dialog->scrolled_window = GTK_SCROLLED_WINDOW (widget);
  gtk_scrolled_window_set_policy (dialog->scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (dialog->scrolled_window, GTK_SHADOW_IN);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
#endif

  widget = gtk_text_view_new ();
  dialog->textview = GTK_TEXT_VIEW (widget);
  gtk_text_view_set_editable (dialog->textview, FALSE);
  gtk_text_view_set_cursor_visible (dialog->textview, FALSE);
  gtk_text_view_set_left_margin (dialog->textview, 3);
  gtk_text_view_set_right_margin (dialog->textview, 3);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_text_view_set_top_margin (dialog->textview, 3);
  gtk_text_view_set_bottom_margin (dialog->textview, 3);
#endif

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->scrolled_window));

  gtk_container_add (GTK_CONTAINER (dialog->scrolled_window), widget);
}

/*!
 * \brief Creates the action area with the close button
 * \param dialog A LicenseDialog structure
 */
static void
create_action_box (LicenseDialog *dialog)
{
  GtkWidget *widget;

  widget = gtk_hbutton_box_new ();
  dialog->action_box = GTK_HBUTTON_BOX (widget);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_END);

  /* Create close button */
  widget = gtk_button_new_from_stock ("gtk-close");
  dialog->button_close = GTK_BUTTON (widget);

  gtk_widget_set_can_focus (widget, TRUE);
  gtk_widget_set_can_default (widget, TRUE);
  gtk_widget_grab_focus (widget);
  gtk_widget_grab_default (widget);

  /* Add button to dialog */
  gtk_dialog_add_action_widget (dialog->LicenseDialog, widget, GTK_RESPONSE_CLOSE);

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->action_box));
}

/*!
 * \brief Creates a new LicenseDialog structure with all widgets initialized
 * \param parent The parent widget for the dialog
 * \return A newly allocated LicenseDialog structure
 */
LicenseDialog *
license_dialog_new (GtkWidget *parent)
{
  GtkWindow *window;
  LicenseDialog *dialog;

  dialog = g_new0 (LicenseDialog, 1);

  /* Create main dialog */
  dialog->LicenseDialog = GTK_DIALOG (gtk_dialog_new ());
  window = GTK_WINDOW (dialog->LicenseDialog);

  gtk_window_set_title (window, _ ("License"));
  gtk_window_set_default_size (window, 420, 320);
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_destroy_with_parent (window, TRUE);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);

#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator (dialog->LicenseDialog, FALSE);
#endif

  /* Get the existing vbox from the dialog */
#if GTK_CHECK_VERSION(3, 0, 0)
  dialog->vbox = GTK_VBOX (gtk_dialog_get_content_area (dialog->LicenseDialog));
#else
  dialog->vbox = GTK_VBOX (dialog->LicenseDialog->vbox);
#endif

  /* Create and add scrolled window */
  create_scrolled_window (dialog);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), GTK_WIDGET (dialog->scrolled_window), TRUE, TRUE, 0);

  /* Create and add action area */
  create_action_box (dialog);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), GTK_WIDGET (dialog->action_box), FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (dialog->vbox));
  gtk_widget_show_all (GTK_WIDGET (dialog->action_box));

  if (parent)
    {
      gtk_window_set_transient_for (window, GTK_WINDOW (gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW)));
    }

  gtk_widget_show (GTK_WIDGET (window));

  return dialog;
}

/*!
 * \brief Frees the LicenseDialog structure and all its widgets
 * \param dialog A LicenseDialog structure
 */
void
license_dialog_free (LicenseDialog *dialog)
{
  if (dialog)
    {
      if (dialog->LicenseDialog)
        gtk_widget_destroy (GTK_WIDGET (dialog->LicenseDialog));

      g_free (dialog);
    }
}

/*!
 * \brief Sets the license text in the dialog
 * \param dialog A LicenseDialog structure
 * \param text The license text to display
 */
void
license_dialog_set_text (LicenseDialog *dialog, const gchar *text)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (dialog != NULL);
  g_return_if_fail (dialog->textview != NULL);

  buffer = gtk_text_view_get_buffer (dialog->textview);
  gtk_text_buffer_set_text (buffer, text ? text : "", -1);
}
