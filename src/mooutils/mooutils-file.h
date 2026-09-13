#ifndef MOO_UTILS_FILE_H
#define MOO_UTILS_FILE_H

#include "moocpp/array.h"

#ifdef __cplusplus
using MooFileArray = MooObjectArray<GFile>;
#endif

G_BEGIN_DECLS

static inline void
moo_file_free (GFile *file)
{
    if (file)
        g_object_unref (file);
}

gboolean     moo_file_fnmatch           (GFile      *file,
                                         const char *glob);
char        *moo_file_get_display_name  (GFile *file);

G_END_DECLS

#endif /* MOO_UTILS_FILE_H */
