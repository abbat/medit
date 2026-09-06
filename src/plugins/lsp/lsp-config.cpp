/*
 *   plugins/lsp/lsp-config.cpp
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

#include "plugins/lsp/lsp-config.h"

#include "mooutils/moobuilder.h"
#include "mooutils/moomarkup.h"
#include "mooutils/mooutils-fs.h"
#include "mooutils/mooutils-misc.h"

#include <string.h>


void
lsp_config_free (LspServerConfig *config)
{
    if (!config)
        return;

    g_free (config->id);
    g_free (config->filter);
    g_free (config->command);
    g_strfreev (config->argv);
    g_strfreev (config->root_markers);
    g_free (config->init_options);
    g_strfreev (config->env);
    g_free (config);
}


void
lsp_config_list_free (GSList *list)
{
    g_slist_free_full (list, (GDestroyNotify) lsp_config_free);
}


char *
lsp_config_find_program (LspServerConfig *config)
{
    g_return_val_if_fail (config != NULL, NULL);

    if (!config->argv || !config->argv[0])
        return NULL;

    if (g_path_is_absolute (config->argv[0]))
        return g_file_test (config->argv[0], G_FILE_TEST_IS_EXECUTABLE)
                   ? g_strdup (config->argv[0]) : NULL;

    return g_find_program_in_path (config->argv[0]);
}


char *
lsp_config_user_file (void)
{
    return moo_get_user_data_file (MOO_LSP_CONFIG_FILE);
}


char *
lsp_config_system_file (void)
{
    char **files;
    char *user_file;
    char *result = NULL;
    guint i;

    /* moo_get_data_files() puts the user data directory first; skip it here,
       this function is about the file that medit itself installed. */
    user_file = lsp_config_user_file ();
    files = moo_get_data_files (MOO_LSP_CONFIG_FILE);

    for (i = 0; files && files[i] && !result; ++i)
    {
        if (user_file && strcmp (files[i], user_file) == 0)
            continue;
        if (g_file_test (files[i], G_FILE_TEST_EXISTS))
            result = g_strdup (files[i]);
    }

    g_strfreev (files);
    g_free (user_file);

    return result;
}


/*
 * The file medit was built with, which is the same one it installs. Having it
 * in the binary as well means the defaults do not depend on medit being
 * installed, and that the copy handed to the user is never an empty stub.
 */
static char *
builtin_config (gsize *len)
{
    return moo_resource_get_text (LSP_CONFIG_RESOURCE, len);
}


char *
lsp_config_ensure_user_file (GError **error)
{
    char *user_file = lsp_config_user_file ();
    char *system_file;
    char *contents = NULL;
    gsize len = 0;

    if (!user_file)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "could not work out where the user data directory is");
        return NULL;
    }

    if (g_file_test (user_file, G_FILE_TEST_EXISTS))
        return user_file;

    /*
     * The installed file first, so that a distribution editing its defaults
     * has the last word, and the built-in copy when medit is being run from a
     * build directory or was installed without its data files.
     */
    system_file = lsp_config_system_file ();

    if (!system_file || !g_file_get_contents (system_file, &contents, &len, NULL))
        contents = builtin_config (&len);

    g_free (system_file);

    if (!contents)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "the built-in configuration is missing");
        g_free (user_file);
        return NULL;
    }

    /* moo_save_config_file() creates the user data directory as needed. */
    if (!moo_save_config_file (user_file, contents, len, error))
    {
        g_free (contents);
        g_free (user_file);
        return NULL;
    }

    g_free (contents);

    return user_file;
}


/*
 * The content of a child element, or NULL when there is no such child. The
 * text is not CDATA: MooMarkup turns a CDATA section into a comment node,
 * where moo_markup_get_content() cannot see it. Ordinary escaped text works,
 * and JSON needs escaping for < > & only.
 */
static char *
child_content (MooMarkupNode *node,
               const char    *name)
{
    MooMarkupNode *child = moo_markup_get_element (node, name);
    const char *content;

    if (!child)
        return NULL;

    content = moo_markup_get_content (child);

    if (!content)
        return NULL;

    return g_strstrip (g_strdup (content));
}


static char **
split_list (const char *string)
{
    char **parts;
    char **result;
    guint i, n = 0;

    if (!string || !string[0])
        return NULL;

    parts = g_strsplit_set (string, ";,", -1);
    result = g_new0 (char*, g_strv_length (parts) + 1);

    for (i = 0; parts[i]; ++i)
    {
        char *item = g_strstrip (g_strdup (parts[i]));

        if (item[0])
            result[n++] = item;
        else
            g_free (item);
    }

    g_strfreev (parts);

    if (n == 0)
    {
        g_strfreev (result);
        return NULL;
    }

    return result;
}


static char **
collect_env (MooMarkupNode *node)
{
    MooMarkupNode *child;
    GPtrArray *array = g_ptr_array_new ();

    for (child = node->children; child != NULL; child = child->next)
    {
        const char *content;

        if (!MOO_MARKUP_IS_ELEMENT (child) || strcmp (child->name, "env") != 0)
            continue;

        content = moo_markup_get_content (child);

        if (content && content[0])
            g_ptr_array_add (array, g_strstrip (g_strdup (content)));
    }

    if (array->len == 0)
    {
        g_ptr_array_free (array, TRUE);
        return NULL;
    }

    g_ptr_array_add (array, NULL);

    return (char**) g_ptr_array_free (array, FALSE);
}


static LspServerConfig *
parse_server (MooMarkupNode *node)
{
    LspServerConfig *config;
    const char *id = moo_markup_get_prop (node, "id");
    char *command;
    char **argv = NULL;
    GError *error = NULL;

    if (!id || !id[0])
    {
        g_warning ("%s: a <server> element without an id, ignoring it", G_STRFUNC);
        return NULL;
    }

    command = child_content (node, "command");

    if (!command || !command[0])
    {
        g_warning ("%s: server '%s' has no <command>, ignoring it", G_STRFUNC, id);
        g_free (command);
        return NULL;
    }

    if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
        g_warning ("%s: server '%s' has an unparsable <command>: %s",
                   G_STRFUNC, id, error->message);
        g_error_free (error);
        g_free (command);
        return NULL;
    }

    config = g_new0 (LspServerConfig, 1);
    config->id = g_strdup (id);
    config->enabled = moo_markup_bool_prop (node, "enabled", TRUE);
    config->command = command;
    config->argv = argv;
    config->filter = child_content (node, "filter");
    config->init_options = child_content (node, "initialization-options");
    config->env = collect_env (node);

    {
        char *root = child_content (node, "root");
        config->root_markers = split_list (root);
        g_free (root);
    }

    if (!config->filter || !config->filter[0])
    {
        g_warning ("%s: server '%s' has no <filter>, ignoring it", G_STRFUNC, id);
        lsp_config_free (config);
        return NULL;
    }

    return config;
}


static GSList *
load_markup (MooMarkupDoc *doc,
             const char   *name)
{
    MooMarkupNode *root, *child;
    GSList *list = NULL;

    root = moo_markup_get_root_element (doc, "medit-lsp");

    if (!root)
    {
        g_warning ("%s: %s has no <medit-lsp> element", G_STRFUNC, name);
        moo_markup_doc_unref (doc);
        return NULL;
    }

    for (child = root->children; child != NULL; child = child->next)
    {
        LspServerConfig *config;

        if (!MOO_MARKUP_IS_ELEMENT (child) || strcmp (child->name, "server") != 0)
            continue;

        if ((config = parse_server (child)) != NULL)
            list = g_slist_prepend (list, config);
    }

    moo_markup_doc_unref (doc);

    return g_slist_reverse (list);
}


static GSList *
load_file (const char *filename)
{
    GError *error = NULL;
    MooMarkupDoc *doc = moo_markup_parse_file (filename, &error);

    if (!doc)
    {
        g_warning ("%s: could not parse %s: %s", G_STRFUNC, filename,
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        return NULL;
    }

    return load_markup (doc, filename);
}


static GSList *
load_builtin (void)
{
    GError *error = NULL;
    MooMarkupDoc *doc;
    gsize len = 0;
    char *contents = builtin_config (&len);

    if (!contents)
        return NULL;

    doc = moo_markup_parse_memory (contents, len, &error);
    g_free (contents);

    if (!doc)
    {
        g_warning ("%s: could not parse the built-in configuration: %s", G_STRFUNC,
                   error ? error->message : "unknown error");
        g_clear_error (&error);
        return NULL;
    }

    return load_markup (doc, LSP_CONFIG_RESOURCE);
}


/*
 * The user's copy if there is one, the installed file if there is not, and the
 * built-in copy when there is neither -- which is what a run from a build
 * directory gets, and what an installation without its data files gets.
 */
GSList *
lsp_config_load (void)
{
    char *filename = lsp_config_user_file ();
    GSList *list;

    if (!filename || !g_file_test (filename, G_FILE_TEST_EXISTS))
    {
        g_free (filename);
        filename = lsp_config_system_file ();
    }

    if (!filename)
        return load_builtin ();

    list = load_file (filename);
    g_free (filename);

    return list;
}
