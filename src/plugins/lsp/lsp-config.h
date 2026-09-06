/*
 *   plugins/lsp/lsp-config.h
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

/*
 * The list of language servers, read from lsp.xml. The user's copy in the user
 * data directory replaces the one installed with medit outright; there is no
 * merging, so that what the file says is what medit does.
 */

#ifndef MOO_LSP_CONFIG_H
#define MOO_LSP_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

#define MOO_LSP_CONFIG_FILE "lsp.xml"

typedef struct {
    char        *id;
    char        *filter;        /* a MooEditFilter string, "langs:c,cpp" */
    char        *command;       /* as written, for messages */
    char       **argv;          /* the command, split the way a shell would */
    char       **root_markers;  /* file names that mark the root of a project */
    char        *init_options;  /* initializationOptions, raw JSON, may be NULL */
    char       **env;           /* NAME=VALUE, may be NULL */
    gboolean     enabled;
} LspServerConfig;

/* The list is in file order; the first matching entry wins. */
GSList     *lsp_config_load             (void);
void        lsp_config_list_free        (GSList             *list);
void        lsp_config_free             (LspServerConfig    *config);

/* NULL when the entry names a command that is not installed. */
char       *lsp_config_find_program     (LspServerConfig    *config);

char       *lsp_config_user_file        (void);
char       *lsp_config_system_file      (void);

/*
 * Copies the installed file into the user data directory when there is no user
 * copy yet, and returns its name either way; this is what the menu item opens.
 */
char       *lsp_config_ensure_user_file (GError            **error);

G_END_DECLS

#endif /* MOO_LSP_CONFIG_H */
