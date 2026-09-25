#ifndef MOO_EDIT_HISTORY_ITEM_H
#define MOO_EDIT_HISTORY_ITEM_H

#include "mooutils/moohistorymgr.h"

G_BEGIN_DECLS

void         _moo_edit_history_item_set_encoding    (MooHistoryItem *item,
                                                     const char     *encoding);
void         _moo_edit_history_item_set_line        (MooHistoryItem *item,
                                                     int             line);
const char  *_moo_edit_history_item_get_encoding    (MooHistoryItem *item);
int          _moo_edit_history_item_get_line        (MooHistoryItem *item);

/* Frecency: an exponentially-decayed visit count, half-life 72 hours. Call
   _visit() each time the file is opened; _get_frecency() reports its current
   value without changing it, decayed to `now` (g_get_real_time() / G_USEC_PER_SEC).
   moo_history_mgr_add_file() replaces a URI's stored item wholesale rather than
   merging keys, so _carry_frecency() must copy the old item's frecency onto the
   fresh one before _visit() is called on it. */
void         _moo_edit_history_item_visit           (MooHistoryItem *item);
double       _moo_edit_history_item_get_frecency    (MooHistoryItem *item,
                                                     gint64          now);
void         _moo_edit_history_item_carry_frecency  (MooHistoryItem *dest,
                                                     MooHistoryItem *src);

G_END_DECLS

#endif /* MOO_EDIT_HISTORY_ITEM_H */
