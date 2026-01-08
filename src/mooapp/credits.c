/*
 *   mooapp/credits.c
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

#include "credits.h"

#include "mooutils/mooi18n.h"

/*!
 * \brief Structure containing all widgets from the credits dialog
 */
struct _CreditsDialog
{
  GtkDialog *CreditsDialog;             /*!< \brief Main dialog widget */
  GtkVBox *vbox;                        /*!< \brief Dialog's main vbox */
  GtkNotebook *notebook;                /*!< \brief Notebook widget with tabs */
  GtkScrolledWindow *tab_thanks;        /*!< \brief Scrolled window for "Thanks" tab */
  GtkTextView *view_thanks;             /*!< \brief Text view for "Thanks" tab */
  GtkLabel *label_thanks;               /*!< \brief Label for "Thanks" tab */
  GtkScrolledWindow *tab_written_by;    /*!< \brief Scrolled window for "Written by" tab */
  GtkTextView *view_written_by;         /*!< \brief Text view for "Written by" tab */
  GtkLabel *label_written_by;           /*!< \brief Label for "Written by" tab */
  GtkScrolledWindow *tab_translated_by; /*!< \brief Scrolled window for "Translated by" tab */
  GtkTextView *view_translated_by;      /*!< \brief Text view for "Translated by" tab */
  GtkLabel *label_translated_by;        /*!< \brief Label for "Translated by" tab */
  GtkHButtonBox *action_box;            /*!< \brief Button box for dialog actions */
  GtkButton *button_close;              /*!< \brief Close button */
};

/*!
 * \brief Creates the "Thanks" tab with a scrolled window and text view
 * \return The created scrolled window widget
 */
static void
create_tab_thanks (CreditsDialog *dialog)
{
  GtkWidget *widget;

  widget = gtk_scrolled_window_new (NULL, NULL);
  dialog->tab_thanks = GTK_SCROLLED_WINDOW (widget);
  gtk_scrolled_window_set_policy (dialog->tab_thanks, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (dialog->tab_thanks, GTK_SHADOW_IN);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
#endif

  widget = gtk_text_view_new ();
  dialog->view_thanks = GTK_TEXT_VIEW (widget);
  gtk_text_view_set_editable (dialog->view_thanks, FALSE);
  gtk_text_view_set_left_margin (dialog->view_thanks, 3);
  gtk_text_view_set_right_margin (dialog->view_thanks, 3);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_text_view_set_top_margin (dialog->view_thanks, 3);
  gtk_text_view_set_bottom_margin (dialog->view_thanks, 3);
#endif

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->tab_thanks));

  gtk_container_add (GTK_CONTAINER (dialog->tab_thanks), widget);
}

/*!
 * \brief Creates the "Written by" tab with a scrolled window and text view
 * \return The created scrolled window widget
 */
static void
create_tab_written_by (CreditsDialog *dialog)
{
  GtkWidget *widget;

  widget = gtk_scrolled_window_new (NULL, NULL);
  dialog->tab_written_by = GTK_SCROLLED_WINDOW (widget);
  gtk_scrolled_window_set_policy (dialog->tab_written_by, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (dialog->tab_written_by, GTK_SHADOW_IN);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
#endif

  widget = gtk_text_view_new ();
  dialog->view_written_by = GTK_TEXT_VIEW (widget);
  gtk_text_view_set_editable (dialog->view_written_by, FALSE);
  gtk_text_view_set_left_margin (dialog->view_written_by, 3);
  gtk_text_view_set_right_margin (dialog->view_written_by, 3);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_text_view_set_top_margin (dialog->view_written_by, 3);
  gtk_text_view_set_bottom_margin (dialog->view_written_by, 3);
#endif

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->tab_written_by));

  gtk_container_add (GTK_CONTAINER (dialog->tab_written_by), widget);
}

/*!
 * \brief Creates the "Translated by" tab with a scrolled window and text view
 * \return The created scrolled window widget
 */
static void
create_tab_translated_by (CreditsDialog *dialog)
{
  GtkWidget *widget;

  widget = gtk_scrolled_window_new (NULL, NULL);
  dialog->tab_translated_by = GTK_SCROLLED_WINDOW (widget);
  gtk_scrolled_window_set_policy (dialog->tab_translated_by, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (dialog->tab_translated_by, GTK_SHADOW_IN);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_widget_set_vexpand (widget, TRUE);
#endif

  widget = gtk_text_view_new ();
  dialog->view_translated_by = GTK_TEXT_VIEW (widget);
  gtk_text_view_set_editable (dialog->view_translated_by, FALSE);
  gtk_text_view_set_left_margin (dialog->view_translated_by, 3);
  gtk_text_view_set_right_margin (dialog->view_translated_by, 3);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_text_view_set_top_margin (dialog->view_translated_by, 3);
  gtk_text_view_set_bottom_margin (dialog->view_translated_by, 3);
#endif

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->tab_translated_by));

  gtk_container_add (GTK_CONTAINER (dialog->tab_translated_by), widget);
}

/*!
 * \brief Creates the notebook with all tabs
 * \param dialog A CreditsDialog structure
 * \return The created notebook widget
 */
static void
create_notebook (CreditsDialog *dialog)
{
  GtkWidget *tab;
  GtkWidget *label;

  dialog->notebook = GTK_NOTEBOOK (gtk_notebook_new ());

  /* Create "Thanks" tab */
  create_tab_thanks (dialog);
  tab = GTK_WIDGET (dialog->tab_thanks);
  label = gtk_label_new (_ ("Thanks"));
  dialog->label_thanks = GTK_LABEL (label);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_container_child_set (GTK_CONTAINER (dialog->notebook), tab, "tab-fill", FALSE, NULL);
#else
  gtk_notebook_set_tab_label_packing (dialog->notebook, tab, FALSE, FALSE, GTK_PACK_START);
#endif

  gtk_widget_show (label);
  gtk_notebook_append_page (dialog->notebook, tab, NULL);
  gtk_notebook_set_tab_label (dialog->notebook, tab, label);

  /* Create "Written by" tab */
  create_tab_written_by (dialog);
  tab = GTK_WIDGET (dialog->tab_written_by);
  label = gtk_label_new (_ ("Written by"));
  dialog->label_written_by = GTK_LABEL (label);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_container_child_set (GTK_CONTAINER (dialog->notebook), tab, "tab-fill", FALSE, NULL);
#else
  gtk_notebook_set_tab_label_packing (dialog->notebook, tab, FALSE, FALSE, GTK_PACK_START);
#endif

  gtk_widget_show (label);
  gtk_notebook_append_page (dialog->notebook, tab, NULL);
  gtk_notebook_set_tab_label (dialog->notebook, tab, label);

  /* Create "Translated by" tab */
  create_tab_translated_by (dialog);
  tab = GTK_WIDGET (dialog->tab_translated_by);
  label = gtk_label_new (_ ("Translated by"));
  dialog->label_translated_by = GTK_LABEL (label);

#if GTK_CHECK_VERSION(3, 0, 0)
  gtk_container_child_set (GTK_CONTAINER (dialog->notebook), tab, "tab-fill", FALSE, NULL);
#else
  gtk_notebook_set_tab_label_packing (dialog->notebook, tab, FALSE, FALSE, GTK_PACK_START);
#endif

  gtk_widget_show (label);
  gtk_notebook_append_page (dialog->notebook, tab, NULL);
  gtk_notebook_set_tab_label (dialog->notebook, tab, label);

  gtk_widget_show (GTK_WIDGET (dialog->notebook));
}

/*!
 * \brief Creates the action area with the close button
 * \param dialog A CreditsDialog structure
 * \return The created button box widget
 */
static void
create_action_box (CreditsDialog *dialog)
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
  gtk_dialog_add_action_widget (dialog->CreditsDialog, widget, GTK_RESPONSE_CLOSE);

  gtk_widget_show (widget);
  gtk_widget_show (GTK_WIDGET (dialog->action_box));
}

/*!
 * \brief Creates a new CreditsDialog structure with all widgets initialized
 * \return A newly allocated CreditsDialog structure
 */
CreditsDialog *
credits_dialog_new (GtkWidget *parent)
{
  GtkWindow *window;
  CreditsDialog *dialog;

  dialog = g_new0 (CreditsDialog, 1);

  /* Create main dialog */
  dialog->CreditsDialog = GTK_DIALOG (gtk_dialog_new ());
  window = GTK_WINDOW (dialog->CreditsDialog);

  gtk_window_set_title (window, _ ("Credits"));
  gtk_window_set_default_size (window, 360, 260);
  gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_destroy_with_parent (window, TRUE);
  gtk_window_set_type_hint (window, GDK_WINDOW_TYPE_HINT_DIALOG);

#if !GTK_CHECK_VERSION(3, 0, 0)
  gtk_dialog_set_has_separator (dialog->CreditsDialog, FALSE);
#endif

  /* Get the existing vbox from the dialog */
  /* TODO: check vbox created automatically */
#if GTK_CHECK_VERSION(3, 0, 0)
  dialog->vbox = GTK_VBOX (gtk_dialog_get_content_area (dialog->CreditsDialog));
#else
  dialog->vbox = GTK_VBOX (dialog->CreditsDialog->vbox);
#endif

  /* Create and add notebook */
  create_notebook (dialog);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), GTK_WIDGET (dialog->notebook), TRUE, TRUE, 0);

  /* Create and add action area */
  create_action_box (dialog);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), GTK_WIDGET (dialog->action_box), FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (dialog->vbox));
  gtk_widget_show_all (GTK_WIDGET (dialog->action_box));

  if (parent) {
    // FIXME:
    gtk_window_set_transient_for (window, GTK_WINDOW (gtk_widget_get_ancestor (parent, GTK_TYPE_WINDOW)));
  }

  gtk_widget_show (GTK_WIDGET (window));

  return dialog;
}

/*!
 * \brief Frees the CreditsDialog structure and all its widgets
 * \param dialog A CreditsDialog structure
 */
void
credits_dialog_free (CreditsDialog *dialog)
{
  if (dialog)
    {
      if (dialog->CreditsDialog)
        gtk_widget_destroy (GTK_WIDGET (dialog->CreditsDialog));

      g_free (dialog);
    }
}
