#ifndef MOO_EDIT_SCRIPT_H
#define MOO_EDIT_SCRIPT_H

#include <mooedit/mooedit.h>

G_BEGIN_DECLS

char        *moo_edit_get_text                  (MooEdit            *doc,
                                                 const GtkTextIter  *start,
                                                 const GtkTextIter  *end);
char        *moo_edit_get_selected_text         (MooEdit            *doc);

G_END_DECLS

#endif /* MOO_EDIT_SCRIPT_H */
