/*
 *   plugins/ctags/ctags-plugin.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *                 2008      by Christian Dywan <christian@twotoasts.de>
 *                 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
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

#include "ctags-plugin.h"

#include "ctags-doc.h"
#include "ctags-view.h"
#include "mooedit/mooplugin-macro.h"
#include "mooutils/mooi18n.h"

/*!< \brief Plugin identifier string */
const char *CTAGS_PLUGIN_ID = "Ctags";

/*!
 * \brief Ctags plugin structure
 */
typedef struct
{
  MooPlugin parent; /*!< \brief Parent plugin structure */
} CtagsPlugin;

/*!
 * \brief Ctags window plugin structure
 */
typedef struct
{
  MooWinPlugin parent;   /*!< \brief Parent window plugin structure */
  MooCtagsView *view;    /*!< \brief Ctags view widget */
  guint update_idle;     /*!< \brief ID for idle update callback */
} CtagsWindowPlugin;

/*!
 * \brief Define plugin information for Ctags plugin
 *
 * This macro creates a static MooPluginInfo structure containing
 * metadata about the Ctags plugin including name, description,
 * author information, and version.
 */
MOO_PLUGIN_DEFINE_INFO (ctags, "Ctags", "Shows functions in the open document", "Yevgen Muntyan <emuntyan@users.sourceforge.net>\n"
                                                                                "Christian Dywan <christian@twotoasts.de>",
                        MOO_VERSION)

/*!
 * \brief Define window plugin type for Ctags
 *
 * This macro generates the necessary code to register the CtagsWindowPlugin
 * type with the GObject type system, including class and instance initialization
 * functions, and the create/destroy callbacks for the window plugin.
 */
MOO_WIN_PLUGIN_DEFINE (Ctags, ctags)

/*!
 * \brief Define the main Ctags plugin type
 *
 * This macro generates the complete plugin definition including:
 * - Plugin class and instance initialization
 * - Window plugin attachment (ctags_window_plugin_get_type)
 * - Document plugin attachment (MOO_TYPE_CTAGS_DOC_PLUGIN)
 * - No custom attach/detach functions (NULL parameters)
 * - No preferences page (NULL parameter)
 */
MOO_PLUGIN_DEFINE (Ctags, ctags, NULL, NULL, NULL, NULL, NULL, ctags_window_plugin_get_type (), MOO_TYPE_CTAGS_DOC_PLUGIN)

/*!
 * \brief Update the ctags view with current document's tags
 * \param plugin The window plugin instance
 * \return FALSE to indicate the idle source should be removed
 */
static gboolean
window_plugin_update (CtagsWindowPlugin *plugin)
{
  MooEdit *doc;
  GtkTreeModel *model;
  MooEditWindow *window;
  MooCtagsDocPlugin *dp;

  plugin->update_idle = 0;

  window = MOO_WIN_PLUGIN (plugin)->window;
  doc = moo_edit_window_get_active_doc (window);

  if (!doc)
    {
      gtk_tree_view_set_model (GTK_TREE_VIEW (plugin->view), NULL);
      return FALSE;
    }

  dp = moo_doc_plugin_lookup (CTAGS_PLUGIN_ID, doc);
  g_return_val_if_fail (MOO_IS_CTAGS_DOC_PLUGIN (dp), FALSE);

  model = _moo_ctags_doc_plugin_get_store (dp);
  gtk_tree_view_set_model (GTK_TREE_VIEW (plugin->view), model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (plugin->view));

  return FALSE;
}

/*!
 * \brief Handle active document change event
 * \param plugin The window plugin instance
 */
static void
active_doc_changed (CtagsWindowPlugin *plugin)
{
  if (!plugin->update_idle)
    plugin->update_idle = g_idle_add_full (G_PRIORITY_LOW,
                                           (GSourceFunc) window_plugin_update,
                                           plugin, NULL);
}

/*!
 * \brief Handle entry activation in the ctags view
 * \param plugin The window plugin instance
 * \param entry The activated ctags entry
 */
static void
entry_activated (CtagsWindowPlugin *plugin, MooCtagsEntry *entry)
{
  MooEditView *view;
  MooEditWindow *window;

  window = MOO_WIN_PLUGIN (plugin)->window;
  view = moo_edit_window_get_active_view (window);

  if (view && entry->line >= 0)
    {
      gtk_widget_grab_focus (GTK_WIDGET (view));
      moo_text_view_move_cursor (MOO_TEXT_VIEW (view), entry->line, -1, FALSE, FALSE);
    }
}

/*!
 * \brief Create the ctags window plugin UI
 * \param plugin The window plugin instance
 * \return TRUE on success, FALSE on failure
 */
static gboolean
ctags_window_plugin_create (CtagsWindowPlugin *plugin)
{
  GtkWidget *swin;
  GtkWidget *view;
  MooPaneLabel *label;
  MooEditWindow *window = MOO_WIN_PLUGIN (plugin)->window;

  view = _moo_ctags_view_new ();
  g_return_val_if_fail (view != NULL, FALSE);

  plugin->view = MOO_CTAGS_VIEW (view);
  g_signal_connect_swapped (view, "activate-entry",
                            G_CALLBACK (entry_activated),
                            plugin);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (plugin->view));
  gtk_widget_show_all (swin);

  label = moo_pane_label_new (GTK_STOCK_INDEX, NULL,
                              /* label of Ctags plugin pane */
                              C_ ("window-pane", "Functions"),
                              C_ ("window-pane", "Functions"));
  moo_edit_window_add_pane (window, CTAGS_PLUGIN_ID,
                            swin, label, MOO_PANE_POS_RIGHT);
  moo_pane_label_free (label);

  g_signal_connect_swapped (window, "notify::active-doc",
                            G_CALLBACK (active_doc_changed),
                            plugin);
  active_doc_changed (plugin);

  return TRUE;
}

/*!
 * \brief Destroy the ctags window plugin UI
 * \param plugin The window plugin instance
 */
static void
ctags_window_plugin_destroy (CtagsWindowPlugin *plugin)
{
  MooEditWindow *window = MOO_WIN_PLUGIN (plugin)->window;

  moo_edit_window_remove_pane (window, CTAGS_PLUGIN_ID);

  g_signal_handlers_disconnect_by_func (window,
                                        (gpointer) active_doc_changed,
                                        plugin);

  if (plugin->update_idle)
    g_source_remove (plugin->update_idle);

  plugin->update_idle = 0;
}

/*!
 * \brief Initialize the ctags plugin
 * \param plugin The plugin instance (unused)
 * \return TRUE on success
 */
static gboolean
ctags_plugin_init (G_GNUC_UNUSED CtagsPlugin *plugin)
{
  return TRUE;
}

/*!
 * \brief Deinitialize the ctags plugin
 * \param plugin The plugin instance (unused)
 */
static void
ctags_plugin_deinit (G_GNUC_UNUSED CtagsPlugin *plugin)
{
}

/*!
 * \brief Register the ctags plugin with the plugin system
 * \return TRUE on success, FALSE on failure
 */
gboolean
moo_ctags_plugin_init (void)
{
  MooPluginParams params = { FALSE, TRUE };

  return moo_plugin_register (CTAGS_PLUGIN_ID,
                              ctags_plugin_get_type (),
                              &ctags_plugin_info,
                              &params);
}
