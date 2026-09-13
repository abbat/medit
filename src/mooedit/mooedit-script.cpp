#include "mooedit/mooedit-script.h"

char *
moo_edit_get_text (MooEdit           *doc,
                   const GtkTextIter *start,
                   const GtkTextIter *end)
{
    GtkTextBuffer *buffer;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    g_return_val_if_fail (MOO_IS_EDIT (doc), NULL);

    buffer = moo_edit_get_buffer (doc);

    if (start)
        start_iter = *start;
    else
        gtk_text_buffer_get_start_iter (buffer, &start_iter);

    if (end)
        end_iter = *end;
    else
        gtk_text_buffer_get_end_iter (buffer, &end_iter);

    return gtk_text_buffer_get_slice (buffer, &start_iter, &end_iter, TRUE);
}

/**
 * moo_edit_get_selected_text:
 *
 * Returns: (type utf8): selected text.
 **/
char *
moo_edit_get_selected_text (MooEdit *doc)
{
    GtkTextBuffer *buffer;
    GtkTextIter start, end;

    g_return_val_if_fail (MOO_IS_EDIT (doc), NULL);

    buffer = moo_edit_get_buffer (doc);
    gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
    return gtk_text_buffer_get_slice (buffer, &start, &end, TRUE);
}
