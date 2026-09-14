/*
 *   ctags-doc.c
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *   Copyright (C) 2008      by Christian Dywan <christian@twotoasts.de>
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

#define MOO_DO_NOT_MANGLE_GLIB_FUNCTIONS
#include "ctags-doc.h"
#include "ctags-view.h"
#include "vendor/ctags/readtags.h"
#include "mooutils/mooutils-misc.h"
#ifdef HAVE_UNISTD_H
#endif

MOO_DEFINE_BOXED_TYPE_R (MooCtagsEntry, _moo_ctags_entry)

struct _MooCtagsDocPluginPrivate
{
    GtkTreeStore *store;
    guint update_idle;
};

G_DEFINE_TYPE_WITH_CODE (MooCtagsDocPlugin, _moo_ctags_doc_plugin, MOO_TYPE_DOC_PLUGIN, G_ADD_PRIVATE(MooCtagsDocPlugin))

typedef struct {
    const char *name;
    const char *opts;
    void (*process_list) (GSList *entries, GtkTreeStore *store);
} MooCtagsLanguage;

static gboolean moo_ctags_doc_plugin_create         (MooCtagsDocPlugin  *plugin);
static void     moo_ctags_doc_plugin_destroy        (MooCtagsDocPlugin  *plugin);

static void     moo_ctags_doc_plugin_queue_update   (MooCtagsDocPlugin  *plugin);
static gboolean moo_ctags_doc_plugin_update         (MooCtagsDocPlugin  *plugin);

static GSList  *moo_ctags_parse_file                (const char         *filename,
                                                     const char         *opts);


static void
_moo_ctags_doc_plugin_class_init (MooCtagsDocPluginClass *klass)
{
    MooDocPluginClass *plugin_class = MOO_DOC_PLUGIN_CLASS (klass);

    plugin_class->create = (MooDocPluginCreateFunc) moo_ctags_doc_plugin_create;
    plugin_class->destroy = (MooDocPluginDestroyFunc) moo_ctags_doc_plugin_destroy;
}

static void
_moo_ctags_doc_plugin_init (MooCtagsDocPlugin *plugin)
{
    plugin->priv = (MooCtagsDocPluginPrivate*) _moo_ctags_doc_plugin_get_instance_private (plugin);
}


static void
moo_ctags_doc_plugin_queue_update (MooCtagsDocPlugin *plugin)
{
    if (!plugin->priv->update_idle)
        plugin->priv->update_idle =
            g_idle_add_full (G_PRIORITY_LOW,
                             (GSourceFunc) moo_ctags_doc_plugin_update,
                             plugin, NULL);
}

static void
ensure_model (MooCtagsDocPlugin *plugin)
{
    if (!plugin->priv->store)
        plugin->priv->store = gtk_tree_store_new (2, MOO_TYPE_CTAGS_ENTRY, G_TYPE_STRING);
}

static gboolean
moo_ctags_doc_plugin_create (MooCtagsDocPlugin *plugin)
{
    ensure_model (plugin);
    moo_ctags_doc_plugin_queue_update (plugin);

    g_signal_connect_swapped (MOO_DOC_PLUGIN (plugin)->doc, "after-save",
                              G_CALLBACK (moo_ctags_doc_plugin_queue_update),
                              plugin);
    g_signal_connect_swapped (MOO_DOC_PLUGIN (plugin)->doc, "filename-changed",
                              G_CALLBACK (moo_ctags_doc_plugin_queue_update),
                              plugin);

    return TRUE;
}

static void
moo_ctags_doc_plugin_destroy (MooCtagsDocPlugin *plugin)
{
    g_signal_handlers_disconnect_by_func (MOO_DOC_PLUGIN (plugin)->doc,
                                          (gpointer) moo_ctags_doc_plugin_queue_update,
                                          plugin);

    if (plugin->priv->update_idle)
        g_source_remove (plugin->priv->update_idle);
    plugin->priv->update_idle = 0;

    g_object_unref (plugin->priv->store);
}


GtkTreeModel *
_moo_ctags_doc_plugin_get_store (MooCtagsDocPlugin *plugin)
{
    g_return_val_if_fail (MOO_IS_CTAGS_DOC_PLUGIN (plugin), NULL);
    ensure_model (plugin);
    return GTK_TREE_MODEL (plugin->priv->store);
}


static void
get_iter_for_class (GtkTreeStore *store,
                    GtkTreeIter  *class_iter,
                    const char   *type,
                    const char   *name,
                    GHashTable   *classes)
{
    GtkTreeIter *stored = g_hash_table_lookup (classes, name);

    if (!stored)
    {
        GtkTreeIter iter;
        char *label;

        label = g_strdup_printf ("<b>%s</b> %s", type, name);

        gtk_tree_store_append (store, &iter, NULL);
        gtk_tree_store_set (store, &iter, MOO_CTAGS_VIEW_COLUMN_LABEL, label, -1);

        stored = gtk_tree_iter_copy (&iter);
        g_hash_table_insert (classes, g_strdup (name), stored);

        g_free (label);
    }

    *class_iter = *stored;
}

/* The labels of the fixed groups, in MooCtagsGroup order. */
static const char * const group_labels[MOO_CTAGS_N_GROUPS] = {
    "<b>Functions</b>",
    "<b>Macros</b>",
    "<b>Types</b>",
    "<b>Variables</b>",
    "<b>Other</b>"
};

/*
 * The group a tag of kind @kind belongs under.
 *
 * The kinds are ctags' own one-letter codes, shared between languages wherever
 * the concept is: "f" a function, "d" a #define or equivalent, "v" a variable,
 * "t" a typedef. Every language ctags knows adds kinds of its own, so anything
 * not listed here goes under Other instead of being dropped -- a tag stays
 * reachable even when this function has not caught up with it.
 *
 * The kinds that name a container ("c", "s", "m", "g", "e") do not reach here:
 * they get a top-level row of their own rather than a place in a group.
 */
MooCtagsGroup
_moo_ctags_group_for_kind (const char *kind)
{
    g_return_val_if_fail (kind != NULL, MOO_CTAGS_GROUP_OTHER);

    if (!strcmp (kind, "f"))
        return MOO_CTAGS_GROUP_FUNCS;
    else if (!strcmp (kind, "d"))
        return MOO_CTAGS_GROUP_MACROS;
    else if (!strcmp (kind, "v"))
        return MOO_CTAGS_GROUP_VARS;
    else if (!strcmp (kind, "t") || !strcmp (kind, "g"))
        return MOO_CTAGS_GROUP_TYPES;
    else
        return MOO_CTAGS_GROUP_OTHER;
}

/*
 * A tag that is a container -- a class, a struct, an enum -- or that belongs
 * to one.
 *
 * These do not go into the fixed groups: the container gets a top-level row
 * named after itself, and its members hang off that row. The row is usually
 * already there by the time the container's own tag turns up, because ctags
 * lists members before the thing they belong to as often as not, and each
 * member asks for the row of its container; get_iter_for_class() is what makes
 * that idempotent.
 */
static void
process_class_entry (GtkTreeStore  *store,
                     MooCtagsEntry *entry,
                     GHashTable    *classes)
{
    GtkTreeIter iter;
    GtkTreeIter parent_iter;
    const char *type = NULL;

    /* What the row is labelled; ctags distinguishes more kinds than the pane
       cares to show, so several of them collapse onto one word. */
    if (!strcmp (entry->kind, "c"))
        type = "class";
    else if (!strcmp (entry->kind, "g") || !strcmp (entry->kind, "e"))
        type = "enum";
    else if (!strcmp (entry->kind, "s") || !strcmp (entry->kind, "m"))
        type = "struct";
    else
        type = entry->kind;

    get_iter_for_class (store, &parent_iter,
                        type,
                        entry->klass ? entry->klass : entry->name,
                        classes);

    if (entry->klass || !strcmp (entry->kind, "m") || !strcmp (entry->kind, "e"))
    {
        gtk_tree_store_append (store, &iter, &parent_iter);
        gtk_tree_store_set (store, &iter, MOO_CTAGS_VIEW_COLUMN_ENTRY, entry, -1);
    }
    else
    {
        /* The container itself: the row exists but so far holds only the
           label, so this is what puts the tag on it and makes it clickable. */
        gtk_tree_store_set (store, &parent_iter, MOO_CTAGS_VIEW_COLUMN_ENTRY, entry, -1);
    }
}

/*
 * The tag list turned into the tree the pane shows.
 *
 * There are two sorts of top-level row. The five fixed groups are created
 * up front, so that they come out in the same order whatever order the tags
 * arrive in, and the ones that stayed empty are taken out at the end -- which
 * is why they are created even when there may be nothing to put in them.
 * Alongside them sits one row per container found in the file.
 *
 * This is the layout for every language without a handler of its own, so the
 * kinds it knows about are the ones the C-like languages have in common.
 */
static void
process_list_simple (GSList       *entries,
                     GtkTreeStore *store)
{
    GHashTable *classes;
    GtkTreeIter group_iter[MOO_CTAGS_N_GROUPS];
    gboolean group_used[MOO_CTAGS_N_GROUPS];
    guint i;

    classes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                     (GDestroyNotify) gtk_tree_iter_free);

    for (i = 0; i < MOO_CTAGS_N_GROUPS; ++i)
    {
        group_used[i] = FALSE;
        gtk_tree_store_append (store, &group_iter[i], NULL);
        gtk_tree_store_set (store, &group_iter[i],
                            MOO_CTAGS_VIEW_COLUMN_LABEL, group_labels[i],
                            -1);
    }

    while (entries)
    {
        MooCtagsEntry *entry;

        entry = entries->data;
        entries = entries->next;

        /* A tag with a class is a member of it; the bare kinds here are the
           containers themselves. Either way the tag belongs to a container
           row and not to a group. */
        if (entry->klass || !strcmp (entry->kind, "c") ||
            !strcmp (entry->kind, "s") || !strcmp (entry->kind, "m") ||
            !strcmp (entry->kind, "g") || !strcmp (entry->kind, "e"))
        {
            process_class_entry (store, entry, classes);
        }
        else
        {
            MooCtagsGroup group = _moo_ctags_group_for_kind (entry->kind);
            GtkTreeIter iter;

            gtk_tree_store_append (store, &iter, &group_iter[group]);
            gtk_tree_store_set (store, &iter, MOO_CTAGS_VIEW_COLUMN_ENTRY, entry, -1);
            group_used[group] = TRUE;
        }
    }

    /* Removing a row does not disturb the others: a GtkTreeStore keeps its
       iters valid, which is what makes it safe to have held on to all five. */
    for (i = 0; i < MOO_CTAGS_N_GROUPS; ++i)
        if (!group_used[i])
            gtk_tree_store_remove (store, &group_iter[i]);

    g_hash_table_destroy (classes);
}

static void
process_list_c (GSList       *entries,
                GtkTreeStore *store)
{
    process_list_simple (entries, store);
}

static void
process_list_python (GSList       *entries,
                     GtkTreeStore *store)
{
    process_list_simple (entries, store);
}

static MooCtagsLanguage *
_moo_ctags_language_find_for_name (const char *lang_name)
{
    static GHashTable *langs_hash;

    if (!lang_name)
        return NULL;

    if (!langs_hash)
    {
        /* default fields option is --fields=afksS */
        static MooCtagsLanguage langs[] = {
            { "c", "--language-force=c -I G_BEGIN_DECLS,G_END_DECLS", process_list_c },
            { "c++", " --language-force=c++ -I G_BEGIN_DECLS,G_END_DECLS", process_list_c },
            { "python", "--language-force=python", process_list_python },
            { "vala", "--language-force=\"C#\"", process_list_c },
        };

        langs_hash = g_hash_table_new (g_str_hash, g_str_equal);
        g_hash_table_insert (langs_hash, (char*) "c", &langs[0]);
        g_hash_table_insert (langs_hash, (char*) "cpp", &langs[1]);
        g_hash_table_insert (langs_hash, (char*) "chdr", &langs[1]);
        g_hash_table_insert (langs_hash, (char*) "python", &langs[2]);
        g_hash_table_insert (langs_hash, (char*) "vala", &langs[3]);
    }

    return g_hash_table_lookup (langs_hash, lang_name);
}


static void
process_entries (MooCtagsDocPlugin *plugin,
                 GSList            *list,
                 MooCtagsLanguage  *lang)
{
    g_return_if_fail (lang == NULL || lang->process_list != NULL);

    if (lang)
        lang->process_list (list, plugin->priv->store);
    else
        process_list_simple (list, plugin->priv->store);
}

static gboolean
moo_ctags_doc_plugin_update (MooCtagsDocPlugin *plugin)
{
    MooEdit *doc;
    char *filename;
    GFile *file;

    plugin->priv->update_idle = 0;

    gtk_tree_store_clear (plugin->priv->store);

    doc = MOO_DOC_PLUGIN (plugin)->doc;

    file = moo_edit_get_file (doc);
    filename = file ? g_file_get_path (file) : NULL;

    if (filename && !g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        g_free (filename);
        filename = NULL;
    }

    if (filename)
    {
        GSList *list = NULL;
        char *lang_id = moo_edit_get_lang_id (doc);
        MooCtagsLanguage *ctags_lang = _moo_ctags_language_find_for_name (lang_id);

        if (ctags_lang && (list = moo_ctags_parse_file (filename, ctags_lang->opts)))
            process_entries (plugin, list, ctags_lang);
        else if ((list = moo_ctags_parse_file (filename, NULL)))
            process_entries (plugin, list, NULL);

        g_slist_foreach (list, (GFunc) _moo_ctags_entry_unref_data, NULL);
        g_slist_free (list);
        g_free (lang_id);
    }

    g_free (filename);
    moo_file_free (file);
    return FALSE;
}


MooCtagsEntry *
_moo_ctags_entry_ref (MooCtagsEntry *entry)
{
    g_return_val_if_fail (entry != NULL, NULL);
    entry->ref_count++;
    return entry;
}


void
_moo_ctags_entry_unref (MooCtagsEntry *entry)
{
    if (entry && !--entry->ref_count)
    {
        g_free (entry->name);
        g_free (entry->klass);
        g_free (entry->signature);
        g_slice_free (MooCtagsEntry, entry);
    }
}


void
_moo_ctags_entry_unref_data (MooCtagsEntry *entry, G_GNUC_UNUSED gpointer data)
{
    _moo_ctags_entry_unref(entry);
}


static MooCtagsEntry *
moo_ctags_entry_new (const tagEntry *te)
{
    MooCtagsEntry *entry;
    guint i;

    g_return_val_if_fail (te != NULL, NULL);

    entry = g_slice_new (MooCtagsEntry);
    entry->ref_count = 1;

    entry->name = g_strdup (te->name);
    entry->line = (int) te->address.lineNumber - 1;
    entry->kind = g_intern_string (te->kind);
    entry->klass = NULL;
    entry->signature = NULL;
    entry->file_scope = te->fileScope != 0;

    for (i = 0; i < te->fields.count; ++i)
    {
        if (!strcmp (te->fields.list[i].key, "class") ||
            !strcmp (te->fields.list[i].key, "struct") ||
            !strcmp (te->fields.list[i].key, "enum"))
            entry->klass = g_strdup (te->fields.list[i].value);

        if (!strcmp (te->fields.list[i].key, "signature"))
            entry->signature = g_strdup (te->fields.list[i].value);
    }

    return entry;
}


static GSList *
_moo_ctags_read_tags (const char *filename)
{
    tagFile *file;
    tagFileInfo file_info;
    tagEntry tentry;
    GSList *list = NULL;

    g_return_val_if_fail (filename != NULL, NULL);

    file = tagsOpen (filename, &file_info);

    if (!file_info.status.opened)
    {
        char *file_utf8 = g_filename_display_name (filename);
        g_warning ("could not read file '%s': %s", file_utf8,
                   g_strerror (file_info.status.error_number));
        g_free (file_utf8);
        return NULL;
    }

    tagsSetSortType (file, TAG_UNSORTED);

    while (tagsNext (file, &tentry) == TagSuccess)
    {
        MooCtagsEntry *e = moo_ctags_entry_new (&tentry);
        if (e)
            list = g_slist_prepend (list, e);
    }

    tagsClose (file);

    return g_slist_reverse (list);
}


static char *
get_tmp_file (void)
{
    char *file;
    int fd;
    GError *error = NULL;

    fd = g_file_open_tmp ("moo-ctags-plugin-XXXXXX", &file, &error);

    if (fd == -1)
    {
        g_critical ("%s: could not open temporary file: %s",
                    G_STRFUNC, error->message);
        g_error_free (error);
        return NULL;
    }

    close (fd);
    return file;
}

static GSList *
moo_ctags_parse_file (const char *filename,
                      const char *opts)
{
    char *tags_file;
    GError *error = NULL;
    int exit_status;
    GSList *list = NULL;
    GString *cmd_line;
    char *quoted_tags_file, *quoted_filename;

    g_return_val_if_fail (filename != NULL, NULL);

    if (!(tags_file = get_tmp_file ()))
        return NULL;

    /* -u unsorted
     * --fields
     *     a access of class members
     *     f file-restricted scoping
     *     k kind of tag as a single letter
     *     s scope of definition
     *     S signature or prototype
     */
    cmd_line = g_string_new ("ctags -u --fields=afksS ");

    if (opts && opts[0])
    {
        g_string_append (cmd_line, opts);
        g_string_append_c (cmd_line, ' ');
    }

    g_string_append (cmd_line, "--excmd=number ");

    quoted_tags_file = g_shell_quote (tags_file);
    quoted_filename = g_shell_quote (filename);
    g_string_append_printf (cmd_line, "-f %s %s", quoted_tags_file, quoted_filename);

    if (!g_spawn_command_line_sync (cmd_line->str, NULL, NULL, &exit_status, &error))
    {
        g_warning ("%s: could not run ctags command: %s", G_STRFUNC, error->message);
        g_error_free (error);
        goto out;
    }

    if (exit_status != 0)
    {
        g_warning ("%s: ctags command returned an error", G_STRFUNC);
        goto out;
    }

    list = _moo_ctags_read_tags (tags_file);

out:
    g_free (quoted_filename);
    g_free (quoted_tags_file);
    g_string_free (cmd_line, TRUE);
    unlink (tags_file);
    g_free (tags_file);
    return list;
}
