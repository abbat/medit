#ifndef MOO_EDIT_TYPES_H
#define MOO_EDIT_TYPES_H

#include "moocpp/array.h"
#include "mooutils/moolist.h"
#include "mooutils/mootype-macros.h"
#include "mooedit/mooedit-enums.h"

typedef struct MooOpenInfo MooOpenInfo;
typedef struct MooSaveInfo MooSaveInfo;
typedef struct MooReloadInfo MooReloadInfo;

typedef struct MooEdit MooEdit;
typedef struct MooEditView MooEditView;
typedef struct MooEditWindow MooEditWindow;
typedef struct MooEditor MooEditor;
typedef struct MooEditTab MooEditTab;

#ifdef __cplusplus
using MooEditArray = MooObjectArray<MooEdit>;
using MooEditViewArray = MooObjectArray<MooEditView>;
using MooEditTabArray = MooObjectArray<MooEditTab>;
using MooEditWindowArray = MooObjectArray<MooEditWindow>;
#endif
MOO_DEFINE_SLIST (MooEditList, moo_edit_list, MooEdit)

#ifdef __cplusplus
using MooOpenInfoArray = MooObjectArray<MooOpenInfo>;
#endif

G_BEGIN_DECLS

#define MOO_TYPE_LINE_END (moo_type_line_end ())
GType   moo_type_line_end   (void) G_GNUC_CONST;

#define MOO_EDIT_RELOAD_ERROR (moo_edit_reload_error_quark ())
#define MOO_EDIT_SAVE_ERROR (moo_edit_save_error_quark ())

MOO_DECLARE_QUARK (moo-edit-reload-error, moo_edit_reload_error_quark)
MOO_DECLARE_QUARK (moo-edit-save-error, moo_edit_save_error_quark)

G_END_DECLS

#endif /* MOO_EDIT_TYPES_H */
