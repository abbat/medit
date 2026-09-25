/*
 *   mooquickopen.cpp
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

#include "plugins/mooplugin-builtin.h"
#include "plugins/mooquickopen-ranker.h"
#include "mooedit/mooeditor.h"
#include "mooedit/mooeditwindow.h"
#include "mooedit/mooedit.h"
#include "mooedit/mooedit-accels.h"
#include "mooedit/mooedithistoryitem.h"
#include "mooutils/moofuzzy.h"
#include "mooutils/moofileindex.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooi18n.h"
#include "mooutils/moohistorymgr.h"
#include "moocpp/gstr.h"

#include <gtk/gtk.h>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#define QUICK_OPEN_ACTION_ID "QuickOpen"

#define QUICK_OPEN_MAX_VISIBLE 50
#define QUICK_OPEN_HISTORY_ITEMS 200
#define QUICK_OPEN_TYPING_DELAY_MS 150

enum {
    COLUMN_MARKUP,
    COLUMN_PATH,
    N_COLUMNS
};

struct QuickOpenDialog {
    GtkWidget    *window;
    GtkEntry     *entry;
    GtkTreeView  *view;
    GtkListStore *store;
    GtkLabel     *status;
    MooEditWindow *edit_window;
    std::string   root;
    std::string   active_path;
    int           target_line; /* 0-based; -1 if the query names no line */
    bool          result_opened;
    guint         changed_timeout_id; /* 0 if none pending */
    guint         anim_timeout_id; /* 0 if no "Searching..." animation running */
    int           anim_frame;
    guint         rank_generation; /* bumped on every dispatch; discards stale results */
};

struct QuickOpenIndexRequest {
    GtkWidget       *window; /* weak pointer; NULL if dialog already destroyed */
    GtkListStore    *store;
    GtkEntry        *entry;
    GtkLabel        *status;
    QuickOpenDialog *dlg;
    std::string      root;
    std::string      active_path;
};

static void quick_open_populate (GtkListStore *store, const char *query,
                                 GPtrArray *files, const char *root,
                                 const std::string &active_path, int *target_line);
static void quick_open_dispatch_rank (QuickOpenDialog *dlg, const char *query, GPtrArray *files);

static void
quick_open_index_ready (GPtrArray *files,
                        gpointer   user_data)
{
    QuickOpenIndexRequest *req = static_cast<QuickOpenIndexRequest *> (user_data);

    if (req->window)
    {
        g_object_remove_weak_pointer (G_OBJECT (req->window), (gpointer *) &req->window);
        quick_open_dispatch_rank (req->dlg, gtk_entry_get_text (req->entry), files);
    }

    delete req;
}

static std::string
markup_highlight_basename (const std::string &path, const char *query)
{
    gstr base = gstr::take (g_path_get_basename (path.c_str ()));
    MooFuzzyMatch match;

    if (!query[0] || !moo_fuzzy_match (query, base.get (), TRUE, &match) || !match.n_positions)
    {
        gstr escaped = gstr::take (g_markup_escape_text (base.get (), -1));
        return escaped.get ();
    }

    std::string markup;
    const char *p = base.get ();
    guint char_idx = 0;
    guint pos_idx = 0;

    while (*p)
    {
        const char *next = g_utf8_next_char (p);
        gstr piece = gstr::take (g_markup_escape_text (p, next - p));

        if (pos_idx < match.n_positions && match.positions[pos_idx] == char_idx)
        {
            markup += "<b>";
            markup += piece.get ();
            markup += "</b>";
            ++pos_idx;
        }
        else
        {
            markup += piece.get ();
        }

        p = next;
        ++char_idx;
    }

    return markup;
}

static void
quick_open_populate (GtkListStore       *store,
                     const char         *query,
                     GPtrArray          *files,
                     const char         *root,
                     const std::string  &active_path,
                     int                *target_line)
{
    MooEditor *editor = moo_editor_instance ();
    std::vector<QuickOpenCandidate> candidates;
    std::unordered_map<std::string, size_t> index;
    gint64 now = g_get_real_time () / G_USEC_PER_SEC;

    gstr search = quick_open_split_line (query, target_line);
    query = search.get ();

    if (query[0] && files)
    {
        for (guint i = 0; i < files->len; ++i)
        {
            const char *rel = static_cast<const char *> (g_ptr_array_index (files, i));
            gstr abs = gstr::take (g_build_filename (root, rel, NULL));
            add_candidate (candidates, index, abs.get (), false, false, 0.0);
        }
    }

    MooEditArray *docs = moo_editor_get_docs (editor);
    for (guint i = 0; i < docs->n_elms; ++i)
    {
        MooEdit *doc = docs->elms[i];
        gstr filename = gstr::take (moo_edit_get_filename (doc));
        if (!filename.empty ())
            add_candidate (candidates, index, filename.get (), true,
                          filename.get () == active_path, 0.0);
    }
    delete docs;

    MooHistoryMgr *history = _moo_editor_get_file_history (editor);
    if (history)
    {
        GSList *items = moo_history_mgr_list_items (history, QUICK_OPEN_HISTORY_ITEMS);
        for (GSList *l = items; l != NULL; l = l->next)
        {
            MooHistoryItem *item = static_cast<MooHistoryItem *> (l->data);
            const char *uri = moo_history_item_get_uri (item);
            gstr path = gstr::take (uri ? g_filename_from_uri (uri, NULL, NULL) : NULL);
            if (!path.empty ())
            {
                double f = _moo_edit_history_item_get_frecency (item, now);
                add_candidate (candidates, index, path.get (), false, false, f);
            }
        }
        g_slist_free (items);
    }

    gstr active_dir = gstr::take (active_path.empty () ? NULL : g_path_get_dirname (active_path.c_str ()));
    std::vector<QuickOpenResult> results =
        quick_open_rank (candidates, query, active_dir.empty () ? std::string () : active_dir.get (), now);

    gtk_list_store_clear (store);

    guint n_visible = 0;
    for (const QuickOpenResult &r : results)
    {
        if (n_visible >= QUICK_OPEN_MAX_VISIBLE)
            break;

        std::string markup = markup_highlight_basename (r.candidate->path, query);
        gstr dir = gstr::take (g_path_get_dirname (r.candidate->path.c_str ()));
        gstr dir_escaped = gstr::take (g_markup_escape_text (dir.get (), -1));
        std::string full_markup = markup + "  <small>" + dir_escaped.get () + "</small>";

        GtkTreeIter iter;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            COLUMN_MARKUP, full_markup.c_str (),
                            COLUMN_PATH, r.candidate->path.c_str (),
                            -1);
        ++n_visible;
    }
}

static gboolean
quick_open_anim_tick (gpointer data)
{
    QuickOpenDialog *dlg = static_cast<QuickOpenDialog *> (data);
    static const char *frames[] = { ".", "..", "..." };

    dlg->anim_frame = (dlg->anim_frame + 1) % (int) G_N_ELEMENTS (frames);
    gstr text = gstr::take (g_strdup_printf ("%s%s", _("Searching"), frames[dlg->anim_frame]));
    gtk_label_set_text (dlg->status, text.get ());

    return G_SOURCE_CONTINUE;
}

static void
quick_open_start_searching (QuickOpenDialog *dlg)
{
    if (dlg->anim_timeout_id)
        return;

    dlg->anim_frame = -1;
    quick_open_anim_tick (dlg);
    dlg->anim_timeout_id = g_timeout_add (300, quick_open_anim_tick, dlg);
}

static void
quick_open_stop_searching (QuickOpenDialog *dlg)
{
    if (dlg->anim_timeout_id)
    {
        g_source_remove (dlg->anim_timeout_id);
        dlg->anim_timeout_id = 0;
    }

    gtk_label_set_text (dlg->status, "");
}

/* Everything quick_open_rank() (and building its candidate list) needs, moved
   to a background thread by quick_open_dispatch_rank() -- see the comment on
   quick_open_entry_changed() for why a single pass over the file index is too
   slow to run on the main thread. Plain data only: no GTK object outlives the
   dispatch that filled this in, and files is ref'd so a cache refresh landing
   on the main thread mid-scan cannot free it out from under the thread. */
struct QuickOpenRankTaskData {
    GPtrArray                                       *files; /* ref'd; may be NULL */
    std::string                                       root;
    std::string                                       query; /* already ":N"-stripped */
    std::string                                       active_dir;
    gint64                                             now;
    std::vector<QuickOpenCandidate>                   extra; /* open docs + history */
    std::vector<std::pair<std::string, std::string>>  rows;  /* filled in-thread: markup, path */
};

static void
quick_open_rank_task_data_free (gpointer data)
{
    QuickOpenRankTaskData *td = static_cast<QuickOpenRankTaskData *> (data);

    if (td->files)
        g_ptr_array_unref (td->files);

    delete td;
}

static void
quick_open_rank_in_thread (GTask                      *task,
                           G_GNUC_UNUSED gpointer      source_object,
                           gpointer                    task_data,
                           G_GNUC_UNUSED GCancellable *cancellable)
{
    QuickOpenRankTaskData *td = static_cast<QuickOpenRankTaskData *> (task_data);
    std::vector<QuickOpenCandidate> candidates = std::move (td->extra);
    std::unordered_map<std::string, size_t> index;

    for (size_t i = 0; i < candidates.size (); ++i)
        index[candidates[i].path] = i;

    if (td->files)
    {
        for (guint i = 0; i < td->files->len; ++i)
        {
            const char *rel = static_cast<const char *> (g_ptr_array_index (td->files, i));
            gstr abs = gstr::take (g_build_filename (td->root.c_str (), rel, NULL));
            add_candidate (candidates, index, abs.get (), false, false, 0.0);
        }
    }

    std::vector<QuickOpenResult> results =
        quick_open_rank (candidates, td->query.c_str (), td->active_dir, td->now);

    guint n_visible = 0;
    for (const QuickOpenResult &r : results)
    {
        if (n_visible >= QUICK_OPEN_MAX_VISIBLE)
            break;

        std::string markup = markup_highlight_basename (r.candidate->path, td->query.c_str ());
        gstr dir = gstr::take (g_path_get_dirname (r.candidate->path.c_str ()));
        gstr dir_escaped = gstr::take (g_markup_escape_text (dir.get (), -1));
        markup += "  <small>";
        markup += dir_escaped.get ();
        markup += "</small>";

        td->rows.push_back (std::make_pair (markup, r.candidate->path));
        ++n_visible;
    }

    g_task_return_boolean (task, TRUE);
}

struct QuickOpenRankRequest {
    GtkWidget       *window; /* weak pointer; NULL if dialog already destroyed */
    QuickOpenDialog *dlg;
    guint            generation;
};

static void
quick_open_rank_done (G_GNUC_UNUSED GObject *source,
                      GAsyncResult          *result,
                      gpointer               user_data)
{
    QuickOpenRankRequest *req = static_cast<QuickOpenRankRequest *> (user_data);
    GTask *task = G_TASK (result);

    g_task_propagate_boolean (task, NULL);

    if (req->window)
    {
        g_object_remove_weak_pointer (G_OBJECT (req->window), (gpointer *) &req->window);

        /* A newer keystroke may have dispatched another rank in the time this
           one took to run; if so, its own completion is still pending and
           this is a stale result -- drop it rather than clobber the list with
           an answer to a query that is no longer on screen. */
        if (req->generation == req->dlg->rank_generation)
        {
            QuickOpenRankTaskData *td = static_cast<QuickOpenRankTaskData *> (g_task_get_task_data (task));

            gtk_list_store_clear (req->dlg->store);
            for (const auto &row : td->rows)
            {
                GtkTreeIter iter;
                gtk_list_store_append (req->dlg->store, &iter);
                gtk_list_store_set (req->dlg->store, &iter,
                                    COLUMN_MARKUP, row.first.c_str (),
                                    COLUMN_PATH, row.second.c_str (),
                                    -1);
            }

            quick_open_stop_searching (req->dlg);
        }
    }

    delete req;
}

/* Builds the candidate list and runs quick_open_rank() on a background
   thread, so the main thread stays free to keep the "Searching..." animation
   moving and the entry responsive. query is the raw entry text (still
   possibly carrying a ":N" suffix); files may be NULL. */
static void
quick_open_dispatch_rank (QuickOpenDialog *dlg,
                          const char      *query,
                          GPtrArray       *files)
{
    gstr clean_query = quick_open_split_line (query, &dlg->target_line);

    if (!clean_query.get ()[0])
    {
        /* Nothing left to fuzzy-match (e.g. the query was just ":42"); fall
           back to the cheap synchronous path rather than spinning up a
           thread for it. Route target_line through a throwaway so populate()
           re-parsing "" does not stomp what we just parsed above. */
        int dummy_line = dlg->target_line;
        ++dlg->rank_generation; /* supersede any rank still in flight */
        quick_open_populate (dlg->store, "", NULL, dlg->root.c_str (), dlg->active_path, &dummy_line);
        quick_open_stop_searching (dlg);
        return;
    }

    MooEditor *editor = moo_editor_instance ();
    QuickOpenRankTaskData *td = new QuickOpenRankTaskData ();
    std::unordered_map<std::string, size_t> index;
    gint64 now = g_get_real_time () / G_USEC_PER_SEC;

    td->files = files ? g_ptr_array_ref (files) : NULL;
    td->root = dlg->root;
    td->query = clean_query.get ();
    td->now = now;

    MooEditArray *docs = moo_editor_get_docs (editor);
    for (guint i = 0; i < docs->n_elms; ++i)
    {
        MooEdit *doc = docs->elms[i];
        gstr filename = gstr::take (moo_edit_get_filename (doc));
        if (!filename.empty ())
            add_candidate (td->extra, index, filename.get (), true,
                          filename.get () == dlg->active_path, 0.0);
    }
    delete docs;

    MooHistoryMgr *history = _moo_editor_get_file_history (editor);
    if (history)
    {
        GSList *items = moo_history_mgr_list_items (history, QUICK_OPEN_HISTORY_ITEMS);
        for (GSList *l = items; l != NULL; l = l->next)
        {
            MooHistoryItem *item = static_cast<MooHistoryItem *> (l->data);
            const char *uri = moo_history_item_get_uri (item);
            gstr path = gstr::take (uri ? g_filename_from_uri (uri, NULL, NULL) : NULL);
            if (!path.empty ())
            {
                double f = _moo_edit_history_item_get_frecency (item, now);
                add_candidate (td->extra, index, path.get (), false, false, f);
            }
        }
        g_slist_free (items);
    }

    gstr active_dir = gstr::take (dlg->active_path.empty () ? NULL : g_path_get_dirname (dlg->active_path.c_str ()));
    td->active_dir = active_dir.empty () ? std::string () : active_dir.get ();

    QuickOpenRankRequest *req = new QuickOpenRankRequest ();
    req->window = dlg->window;
    req->dlg = dlg;
    req->generation = ++dlg->rank_generation;
    g_object_add_weak_pointer (G_OBJECT (dlg->window), (gpointer *) &req->window);

    quick_open_start_searching (dlg);

    GTask *task = g_task_new (NULL, NULL, quick_open_rank_done, req);
    g_task_set_task_data (task, td, quick_open_rank_task_data_free);
    g_task_run_in_thread (task, quick_open_rank_in_thread);
    g_object_unref (task);
}

static void
quick_open_run_query (QuickOpenDialog *dlg)
{
    const char *query = gtk_entry_get_text (dlg->entry);

    if (!query[0])
    {
        ++dlg->rank_generation; /* supersede any rank still in flight */
        quick_open_populate (dlg->store, query, NULL, dlg->root.c_str (), dlg->active_path, &dlg->target_line);
        quick_open_stop_searching (dlg);
        return;
    }

    QuickOpenIndexRequest *req = new QuickOpenIndexRequest ();
    req->window = dlg->window;
    req->store = dlg->store;
    req->entry = dlg->entry;
    req->status = dlg->status;
    req->dlg = dlg;
    req->root = dlg->root;
    req->active_path = dlg->active_path;
    g_object_add_weak_pointer (G_OBJECT (dlg->window), (gpointer *) &req->window);

    quick_open_start_searching (dlg);
    GPtrArray *cached = _moo_file_index_get (dlg->root.c_str (), quick_open_index_ready, req);

    if (cached)
        quick_open_dispatch_rank (dlg, query, cached);
}

static void
quick_open_open_selected (QuickOpenDialog *dlg)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection (dlg->view);
    GtkTreeIter iter;
    GtkTreeModel *model;

    if (!gtk_tree_selection_get_selected (sel, &model, &iter))
        return;

    gstr path;
    char *raw_path = NULL;
    gtk_tree_model_get (model, &iter, COLUMN_PATH, &raw_path, -1);
    path.steal (raw_path);

    if (path.empty ())
        return;

    MooEditor *editor = moo_edit_window_get_editor (dlg->edit_window);
    moo_editor_open_path (editor, path.get (), NULL, dlg->target_line, dlg->edit_window);
    dlg->result_opened = true;

    gtk_widget_destroy (dlg->window);
}

static void
quick_open_move_selection (QuickOpenDialog *dlg,
                           int              delta)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection (dlg->view);
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreePath *path;
    int index = -1;

    if (gtk_tree_selection_get_selected (sel, &model, &iter))
    {
        path = gtk_tree_model_get_path (model, &iter);
        index = gtk_tree_path_get_indices (path)[0];
        gtk_tree_path_free (path);
    }

    index += delta;
    if (index < 0)
        index = 0;

    path = gtk_tree_path_new_from_indices (index, -1);
    gtk_tree_view_set_cursor (dlg->view, path, NULL, FALSE);
    gtk_tree_path_free (path);
}

static gboolean
quick_open_entry_key_press (GtkWidget   *widget,
                            GdkEventKey *event,
                            QuickOpenDialog *dlg)
{
    (void) widget;

    switch (event->keyval)
    {
        case GDK_KEY_Escape:
            gtk_widget_destroy (dlg->window);
            return TRUE;

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            quick_open_open_selected (dlg);
            return TRUE;

        case GDK_KEY_Up:
            quick_open_move_selection (dlg, -1);
            return TRUE;

        case GDK_KEY_Down:
            quick_open_move_selection (dlg, 1);
            return TRUE;

        case GDK_KEY_Page_Up:
            quick_open_move_selection (dlg, -10);
            return TRUE;

        case GDK_KEY_Page_Down:
            quick_open_move_selection (dlg, 10);
            return TRUE;
    }

    return FALSE;
}

static gboolean
quick_open_changed_timeout (gpointer data)
{
    QuickOpenDialog *dlg = static_cast<QuickOpenDialog *> (data);

    dlg->changed_timeout_id = 0;
    quick_open_run_query (dlg);

    return G_SOURCE_REMOVE;
}

/* Ranking the full file index is a synchronous, O(files) scan, and on a large
   enough tree it takes over 1 second (measured with synthetic, deeply-nested
   trees at 200000 files). Running it on every keystroke turns typing a
   two- or three-character query into several back-to-back multi-second UI
   freezes; wait for a short pause in typing instead of ranking after each
   character. */
static void
quick_open_entry_changed (GtkEditable     *editable,
                          QuickOpenDialog *dlg)
{
    (void) editable;

    if (dlg->changed_timeout_id)
        g_source_remove (dlg->changed_timeout_id);

    dlg->changed_timeout_id = g_timeout_add (QUICK_OPEN_TYPING_DELAY_MS,
                                             quick_open_changed_timeout, dlg);
}

static void
quick_open_row_activated (GtkTreeView       *view,
                          GtkTreePath       *path,
                          GtkTreeViewColumn *column,
                          QuickOpenDialog   *dlg)
{
    (void) view;
    (void) path;
    (void) column;
    quick_open_open_selected (dlg);
}

static void
quick_open_activate (MooEditWindow *window)
{
    MooEditor *editor = moo_edit_window_get_editor (window);
    MooEdit *active_doc = moo_editor_get_active_doc (editor);

    QuickOpenDialog dlg;
    dlg.edit_window = window;
    dlg.target_line = -1;
    dlg.result_opened = false;
    dlg.changed_timeout_id = 0;
    dlg.anim_timeout_id = 0;
    dlg.anim_frame = 0;
    dlg.rank_generation = 0;

    gstr active_filename = gstr::take (active_doc ? moo_edit_get_filename (active_doc) : NULL);
    dlg.active_path = active_filename.empty () ? std::string () : active_filename.get ();

    gstr dir = gstr::take (dlg.active_path.empty () ? g_get_current_dir ()
                                                    : g_path_get_dirname (dlg.active_path.c_str ()));

    /* Not just ".git": any marker a project layout tends to have, and the
       outermost one going up rather than the nearest -- so a submodule's own
       ".git" two levels into a monorepo does not shadow the monorepo as a
       whole. Without this, root is dir verbatim (_moo_find_project_root with
       no markers never climbs), so a file open two levels into a repo -- or
       no file open at all, cwd wherever the desktop launcher put it --
       indexes that one directory instead of the project. */
    char *markers[] = {
        (char *) ".git", (char *) "Cargo.toml", (char *) "package.json",
        (char *) "go.work", (char *) "pnpm-workspace.yaml", (char *) "lerna.json",
        (char *) "nx.json", (char *) "turbo.json", (char *) ".projectile",
        (char *) "pyproject.toml", (char *) "setup.py", (char *) "Makefile",
        (char *) "CMakeLists.txt", (char *) "BUILD.bazel", NULL
    };
    dlg.root = gstr::take (_moo_find_project_root (dir.get (), markers, TRUE)).get ();

    dlg.window = gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dlg.window), _("Quick Open"));
    gtk_window_set_transient_for (GTK_WINDOW (dlg.window), GTK_WINDOW (window));
    gtk_window_set_modal (GTK_WINDOW (dlg.window), TRUE);
    gtk_window_set_position (GTK_WINDOW (dlg.window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_decorated (GTK_WINDOW (dlg.window), TRUE);
    gtk_window_set_resizable (GTK_WINDOW (dlg.window), TRUE);
    gtk_window_set_default_size (GTK_WINDOW (dlg.window), 500, 400);

    GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
    gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dlg.window))), vbox);

    dlg.entry = GTK_ENTRY (gtk_entry_new ());
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (dlg.entry), FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
#if GTK_CHECK_VERSION(3,0,0)
    gtk_widget_set_vexpand (scroll, TRUE);
#endif
    gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

    dlg.store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    dlg.view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (dlg.store)));
    gtk_tree_view_set_headers_visible (dlg.view, FALSE);
    g_object_unref (dlg.store);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (
        "", renderer, "markup", COLUMN_MARKUP, NULL);
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_append_column (dlg.view, column);
    gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (dlg.view));

    dlg.status = GTK_LABEL (gtk_label_new (""));
    gtk_misc_set_alignment (GTK_MISC (dlg.status), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (dlg.status), FALSE, FALSE, 0);

    g_signal_connect (dlg.entry, "key-press-event", G_CALLBACK (quick_open_entry_key_press), &dlg);
    g_signal_connect (dlg.entry, "changed", G_CALLBACK (quick_open_entry_changed), &dlg);
    g_signal_connect (dlg.view, "row-activated", G_CALLBACK (quick_open_row_activated), &dlg);
    g_signal_connect (dlg.window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    quick_open_run_query (&dlg);

    gtk_widget_show_all (dlg.window);
    gtk_widget_grab_focus (GTK_WIDGET (dlg.entry));

    gtk_main ();

    if (dlg.changed_timeout_id)
        g_source_remove (dlg.changed_timeout_id);
    if (dlg.anim_timeout_id)
        g_source_remove (dlg.anim_timeout_id);
}

gboolean
_moo_quick_open_plugin_init (void)
{
    MooWindowClass *edit_window_class = MOO_WINDOW_CLASS (g_type_class_ref (MOO_TYPE_EDIT_WINDOW));

    moo_window_class_new_action (edit_window_class, QUICK_OPEN_ACTION_ID, NULL,
                                 "display-name", _("Quick Open"),
                                 "label", _("_Quick Open..."),
                                 "tooltip", _("Open a file by typing part of its name"),
                                 "default-accel", MOO_EDIT_ACCEL_QUICK_OPEN,
                                 "closure-callback", quick_open_activate,
                                 NULL);

    g_type_class_unref (edit_window_class);
    return TRUE;
}
