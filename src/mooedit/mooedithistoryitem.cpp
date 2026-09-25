#include "mooedithistoryitem.h"

#define KEY_ENCODING "encoding"
#define KEY_LINE "line"
#define KEY_FRECENCY "frecency"
#define KEY_FRECENCY_TIME "frecency-time"

#define FRECENCY_HALF_LIFE_SEC (3.0 * 24.0 * 3600.0)

void
_moo_edit_history_item_set_encoding (MooHistoryItem *item,
                                     const char     *encoding)
{
    g_return_if_fail (item != NULL);
    moo_history_item_set (item, KEY_ENCODING, encoding);
}

void
_moo_edit_history_item_set_line (MooHistoryItem *item,
                                 int             line)
{
    char *value = NULL;

    g_return_if_fail (item != NULL);

    if (line >= 0)
        value = g_strdup_printf ("%d", line + 1);

    moo_history_item_set (item, KEY_LINE, value);
    g_free (value);
}

const char *
_moo_edit_history_item_get_encoding (MooHistoryItem *item)
{
    g_return_val_if_fail (item != NULL, NULL);
    return moo_history_item_get (item, KEY_ENCODING);
}

int
_moo_edit_history_item_get_line (MooHistoryItem *item)
{
    const char *strval;

    g_return_val_if_fail (item != NULL, -1);

    strval = moo_history_item_get (item, KEY_LINE);

    if (strval && strval[0])
        return strtol (strval, NULL, 10) - 1;
    else
        return -1;
}

static double
frecency_decay (double f,
                gint64 t,
                gint64 now)
{
    return f * exp2 (-(double) (now - t) / FRECENCY_HALF_LIFE_SEC);
}

double
_moo_edit_history_item_get_frecency (MooHistoryItem *item,
                                     gint64          now)
{
    const char *fval;
    const char *tval;

    g_return_val_if_fail (item != NULL, 0.0);

    fval = moo_history_item_get (item, KEY_FRECENCY);
    tval = moo_history_item_get (item, KEY_FRECENCY_TIME);

    if (!fval || !fval[0] || !tval || !tval[0])
        return 0.0;

    return frecency_decay (g_ascii_strtod (fval, NULL),
                           g_ascii_strtoll (tval, NULL, 10), now);
}

void
_moo_edit_history_item_visit (MooHistoryItem *item)
{
    gint64 now = g_get_real_time () / G_USEC_PER_SEC;
    double f = _moo_edit_history_item_get_frecency (item, now) + 1.0;
    char fbuf[G_ASCII_DTOSTR_BUF_SIZE];
    char *tval;

    g_ascii_formatd (fbuf, sizeof fbuf, "%.6f", f);
    tval = g_strdup_printf ("%" G_GINT64_FORMAT, now);

    moo_history_item_set (item, KEY_FRECENCY, fbuf);
    moo_history_item_set (item, KEY_FRECENCY_TIME, tval);

    g_free (tval);
}

void
_moo_edit_history_item_carry_frecency (MooHistoryItem *dest,
                                       MooHistoryItem *src)
{
    g_return_if_fail (dest != NULL);

    if (!src)
        return;

    moo_history_item_set (dest, KEY_FRECENCY, moo_history_item_get (src, KEY_FRECENCY));
    moo_history_item_set (dest, KEY_FRECENCY_TIME, moo_history_item_get (src, KEY_FRECENCY_TIME));
}
