/*
 *   plugins/lsp/lsp-diagnostics.h
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
 * What a server says is wrong with a document, and how it is shown: a squiggly
 * underline on the text, an icon in the margin beside the line, and a line in
 * the diagnostics pane.
 */

#ifndef MOO_LSP_DIAGNOSTICS_H
#define MOO_LSP_DIAGNOSTICS_H

#include "plugins/lsp/lsp-server.h"
#include "mooedit/mooedit.h"

G_BEGIN_DECLS

typedef enum {
    LSP_SEVERITY_ERROR       = 1,
    LSP_SEVERITY_WARNING     = 2,
    LSP_SEVERITY_INFORMATION = 3,
    LSP_SEVERITY_HINT        = 4
} LspSeverity;

typedef struct {
    int   severity;
    int   start_line;
    int   start_character;
    int   end_line;
    int   end_character;
    char *message;
    char *source;       /* which tool inside the server said it, may be NULL */
    char *code;         /* the server's own identifier for it, may be NULL */
} LspDiagnostic;

GSList     *lsp_diagnostics_parse   (JsonArray          *array);
void        lsp_diagnostics_free    (GSList             *list);

/* Translated, for the pane. */
const char *lsp_severity_name       (int                 severity);

/*
 * Replaces whatever was shown on the document with this list. The positions
 * are the server's, so the encoding it agreed to is needed to place them.
 */
void        lsp_diagnostics_apply   (MooEdit            *doc,
                                     GSList             *list,
                                     LspPositionEncoding encoding);
void        lsp_diagnostics_clear   (MooEdit            *doc);

G_END_DECLS

#endif /* MOO_LSP_DIAGNOSTICS_H */
