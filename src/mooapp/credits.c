/*
 *   mooapp/credits.c
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

#include "credits.h"

#include "mooapp-credits.h"
#include "mooutils/mooi18n.h"

/*!
 * \brief Callback function for the close button click event
 * \param widget The button widget that triggered the event (unused)
 * \param data The dialog window to close
 */
static void
on_close_button_clicked (GtkWidget *widget, gpointer data)
{
  (void) widget;
  gtk_dialog_response (GTK_DIALOG (data), GTK_RESPONSE_CLOSE);
}

/*!
 * \brief Creates a new tab in a notebook with a text view
 * \param notebook The notebook widget to add the tab to
 * \param caption The text to display on the tab label
 * \return The created text view widget
 */
static GtkTextView *
notebook_create_tab (GtkNotebook *notebook, const char *caption)
{
  GtkWidget *label;
  GtkWidget *widget;
  GtkTextView *view;
  GtkScrolledWindow *tab;

  widget = gtk_scrolled_window_new (NULL, NULL);
  tab = GTK_SCROLLED_WINDOW (widget);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
#endif

  gtk_scrolled_window_set_policy (tab, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (tab, GTK_SHADOW_IN);
  gtk_notebook_append_page (notebook, widget, NULL);
  gtk_widget_show (widget);

  label = gtk_label_new (caption);
  gtk_notebook_set_tab_label (notebook, widget, label);
  gtk_widget_show (label);

  widget = gtk_text_view_new ();
  view = GTK_TEXT_VIEW (widget);

  gtk_text_view_set_editable (view, FALSE);
  gtk_text_view_set_left_margin (view, 3);
  gtk_text_view_set_right_margin (view, 3);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_text_view_set_top_margin (view, 3);
  gtk_text_view_set_bottom_margin (view, 3);
#endif

  gtk_container_add (GTK_CONTAINER (tab), widget);
  gtk_widget_show (widget);

  return view;
}

/*!
 * \brief Sets the content of the "Thanks" tab
 * \param view The text view to set the content for
 */
static void
set_thanks_content (GtkTextView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_set_text (buffer, MOO_APP_CREDITS, -1);
}

/*!
 * \brief Sets the content of the "Written by" tab with author information
 * \param view The text view to set the content for
 */
static void
set_written_by_content (GtkTextView *view)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
  gtk_text_buffer_set_text (buffer, "Yevgen Muntyan <" MOO_EMAIL ">", -1);
}

/*!
 * \brief Sets the content of the "Translated by" tab with translator credits
 * \param view The text view to set the content for
 */
static void
set_translated_by_content (GtkTextView *view)
{
  const char *msgid = "translator-credits";
  const char *credits = _ (msgid);
  if (strcmp (credits, msgid) != 0)
    {
      GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
      gtk_text_buffer_set_text (buffer, credits, -1);
    }
}

/*!
 * \brief Creates a notebook with tabs for credits information
 * \param vbox The box container to add the notebook to
 */
static void
create_notebook (GtkBox *vbox)
{
  GtkWidget *widget;
  GtkTextView *view;
  GtkNotebook *notebook;

  widget = gtk_notebook_new ();
  notebook = GTK_NOTEBOOK (widget);

  view = notebook_create_tab (notebook, _ ("Thanks"));
  set_thanks_content (view);

  view = notebook_create_tab (notebook, _ ("Written by"));
  set_written_by_content (view);

  view = notebook_create_tab (notebook, _ ("Translated by"));
  set_translated_by_content (view);

  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
  gtk_widget_show (widget);
}

/*!
 * \brief Creates the content area of the credits dialog
 * \param dialog The dialog to create the content area for
 */
static void
create_content_area (GtkDialog *dialog)
{
  GtkBox *vbox = GTK_BOX (gtk_dialog_get_content_area (dialog));

  create_notebook (vbox);
}

/*!
 * \brief Creates the close button for the credits dialog
 * \param dialog The dialog to create the button for
 * \param hbox The box container to add the button to
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
 * \brief Creates the action area of the credits dialog
 * \param dialog The dialog to create the action area for
 */
static void
create_action_area (GtkDialog *dialog)
{
  GtkBox *hbox;

  hbox = GTK_BOX (gtk_dialog_get_action_area (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);

  create_close_button (dialog, hbox);
}

/*!
 * \brief Creates a new credits dialog
 * \param parent The parent widget (can be NULL)
 * \return The newly created credits dialog
 */
GtkDialog *
credits_dialog_new (GtkWidget *parent)
{
  GtkWidget *widget;
  GtkDialog *dialog;
  GtkWindow *window;

  widget = gtk_dialog_new ();
  dialog = GTK_DIALOG (widget);
  window = GTK_WINDOW (widget);

  gtk_window_set_title (window, _ ("Credits"));
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_default_size (window, 360, 260);
  gtk_window_set_resizable (window, TRUE);
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
 * \brief Shows the credits dialog and waits for it to be closed
 * \param parent The parent widget (can be NULL)
 */
void
show_credits_dialog (GtkWidget *parent)
{
  GtkDialog *dialog = credits_dialog_new (parent);
  gtk_dialog_run (dialog);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}
