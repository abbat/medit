/*
 *   moofilewatch.h
 *
 *   Copyright (C) 2004-2010 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WANT_STAT_MONITOR

#include <time.h>
#include <mooglib/moo-glib.h>
#include <mooglib/moo-stat.h>
#include <sys/types.h>
#include "mooutils/mooutils-misc.h"
#include "mooutils/mooutils-mem.h"
#include "mooutils/moofilewatch.h"
#include "mooutils/mootype-macros.h"
#include "marshals.h"
#include "mooutils/moolist.h"

#if 1
static void  G_GNUC_PRINTF(1,2) DEBUG_PRINT (G_GNUC_UNUSED const char *format, ...)
{
}
#else
#define DEBUG_PRINT _moo_message
#endif

typedef struct {
    guint id;
    MooFileWatch *watch;
    char *filename;

    MooFileWatchCallback callback;
    GDestroyNotify notify;
    gpointer data;

    MgwStatBuf statbuf;

    guint isdir : 1;
    guint alive : 1;
} Monitor;

struct WatchFuncs {
    gboolean (*start)           (MooFileWatch   *watch,
                                 GError        **error);
    gboolean (*shutdown)        (MooFileWatch   *watch,
                                 GError        **error);

    gboolean (*start_monitor)   (MooFileWatch   *watch,
                                 Monitor        *monitor,
                                 GError        **error);
    void     (*stop_monitor)    (MooFileWatch   *watch,
                                 Monitor        *monitor);
};

MOO_DEFINE_SLIST(MonitorList, monitor_list, Monitor)

struct _MooFileWatch {
    guint ref_count;
    guint id;
    guint stat_timeout;
    MonitorList *monitors;
    GHashTable *requests;  /* int -> Monitor* */
    guint alive : 1;
};

typedef enum
{
    MOO_FILE_WATCH_ERROR_CLOSED,
    MOO_FILE_WATCH_ERROR_FAILED,
    MOO_FILE_WATCH_ERROR_NOT_IMPLEMENTED,
    MOO_FILE_WATCH_ERROR_TOO_MANY,
    MOO_FILE_WATCH_ERROR_NOT_DIR,
    MOO_FILE_WATCH_ERROR_IS_DIR,
    MOO_FILE_WATCH_ERROR_NONEXISTENT,
    MOO_FILE_WATCH_ERROR_BAD_FILENAME,
    MOO_FILE_WATCH_ERROR_ACCESS_DENIED
} MooFileWatchError;

MOO_DEFINE_BOXED_TYPE_R (MooFileWatch, moo_file_watch)

#define MOO_FILE_WATCH_ERROR (moo_file_watch_error_quark ())
MOO_DEFINE_QUARK_STATIC (moo-file-watch-error, moo_file_watch_error_quark)

#ifdef WANT_STAT_MONITOR
static gboolean watch_stat_start            (MooFileWatch   *watch,
                                             GError        **error);
static gboolean watch_stat_shutdown         (MooFileWatch   *watch,
                                             GError        **error);
static gboolean watch_stat_start_monitor    (MooFileWatch   *watch,
                                             Monitor        *monitor,
                                             GError        **error);
#endif

static Monitor *monitor_new                 (MooFileWatch   *watch,
                                             const char     *filename,
                                             MooFileWatchCallback callback,
                                             gpointer        data,
                                             GDestroyNotify  notify);
static void     monitor_free                (Monitor        *monitor);


static struct WatchFuncs watch_funcs = {
    watch_stat_start,
    watch_stat_shutdown,
    watch_stat_start_monitor,
    NULL
};


static guint
get_new_monitor_id (void)
{
    static guint id = 0;

    if (!++id)
        ++id;

    return id;
}

static guint
get_new_watch_id (void)
{
    static guint id = 0;

    if (!++id)
        ++id;

    return id;
}


MooFileWatch *
moo_file_watch_ref (MooFileWatch *watch)
{
    g_return_val_if_fail (watch != NULL, watch);
    watch->ref_count++;
    return watch;
}


void
moo_file_watch_unref (MooFileWatch *watch)
{
    GError *error = NULL;

    g_return_if_fail (watch != NULL);

    if (--watch->ref_count)
        return;

    if (watch->alive)
    {
        g_warning ("finalizing open watch");

        if (!moo_file_watch_close (watch, &error))
        {
            g_warning ("error in moo_file_watch_close(): %s",
                       moo_error_message (error));
            g_error_free (error);
        }
    }

    g_hash_table_destroy (watch->requests);
    g_free (watch);
}


MooFileWatch *
moo_file_watch_new (GError **error)
{
    MooFileWatch *watch;

    watch = g_new0 (MooFileWatch, 1);

    watch->requests = g_hash_table_new (g_direct_hash, g_direct_equal);

    watch->id = get_new_watch_id ();
    watch->ref_count = 1;

    if (!watch_funcs.start (watch, error))
    {
        moo_file_watch_unref (watch);
        return NULL;
    }

    watch->alive = TRUE;
    return watch;
}


static MooFileEvent *
moo_file_event_new (const char      *filename,
                    guint            monitor_id,
                    MooFileEventCode code)
{
    MooFileEvent *event;

    event = g_new0 (MooFileEvent, 1);
    event->filename = g_strdup (filename);
    event->monitor_id = monitor_id;
    event->code = code;
    event->error = NULL;

    return event;
}

static MooFileEvent *
moo_file_event_copy (MooFileEvent *event)
{
    MooFileEvent *copy;

    copy = moo_file_event_new (event->filename,
                               event->monitor_id,
                               event->code);

    if (event->error)
        copy->error = g_error_copy (event->error);

    return copy;
}

static void
moo_file_event_free (MooFileEvent *event)
{
    if (event)
    {
        if (event->error)
            g_error_free (event->error);
        g_free (event->filename);
        g_free (event);
    }
}

MOO_DEFINE_BOXED_TYPE_C (MooFileEvent, moo_file_event)


gboolean
moo_file_watch_close (MooFileWatch   *watch,
                      GError        **error)
{
    MonitorList *monitors;

    g_return_val_if_fail (watch != NULL, FALSE);

    if (!watch->alive)
        return TRUE;

    watch->alive = FALSE;
    monitors = watch->monitors;
    watch->monitors = NULL;

    while (monitors)
    {
        Monitor *mon = monitors->data;

        if (watch_funcs.stop_monitor)
            watch_funcs.stop_monitor (watch, mon);

        monitor_free (mon);
        monitors = monitor_list_delete_link (monitors, monitors);
    }

    return watch_funcs.shutdown (watch, error);
}


guint
moo_file_watch_create_monitor (MooFileWatch   *watch,
                               const char     *filename,
                               MooFileWatchCallback callback,
                               gpointer        data,
                               GDestroyNotify  notify,
                               GError        **error)
{
    Monitor *monitor;

    g_return_val_if_fail (watch != NULL, 0);
    g_return_val_if_fail (filename != NULL, 0);
    g_return_val_if_fail (callback != NULL, 0);

    if (!watch->alive)
    {
        g_set_error (error, MOO_FILE_WATCH_ERROR,
                     MOO_FILE_WATCH_ERROR_CLOSED,
                     "MooFileWatch %u closed",
                     watch->id);
        return 0;
    }

    monitor = monitor_new (watch, filename, callback, data, notify);

    if (!watch_funcs.start_monitor (watch, monitor, error))
    {
        monitor_free (monitor);
        return 0;
    }

    monitor->alive = TRUE;
    watch->monitors = monitor_list_prepend (watch->monitors, monitor);
    g_hash_table_insert (watch->requests, GUINT_TO_POINTER (monitor->id), monitor);

    DEBUG_PRINT ("created monitor %d for '%s'", monitor->id, monitor->filename);

    return monitor->id;
}


void
moo_file_watch_cancel_monitor (MooFileWatch *watch,
                               guint         monitor_id)
{
    Monitor *monitor;

    g_return_if_fail (watch != NULL);

    monitor = (Monitor*) g_hash_table_lookup (watch->requests,
                                              GUINT_TO_POINTER (monitor_id));
    g_return_if_fail (monitor != NULL);

    watch->monitors = monitor_list_remove (watch->monitors, monitor);
    g_hash_table_remove (watch->requests, GUINT_TO_POINTER (monitor->id));

    if (monitor->alive)
        DEBUG_PRINT ("stopping monitor %d for '%s'",
                     monitor->id, monitor->filename);
    else
        DEBUG_PRINT ("stopping dead monitor %d for '%s'",
                     monitor->id, monitor->filename);

    if (monitor->alive && watch_funcs.stop_monitor)
        watch_funcs.stop_monitor (watch, monitor);

    monitor_free (monitor);
}


static void
moo_file_watch_emit_event (MooFileWatch *watch,
                           MooFileEvent *event,
                           Monitor      *monitor)
{
    moo_file_watch_ref (watch);

    if (monitor->alive || event->code == MOO_FILE_EVENT_ERROR)
    {
        static const char *names[] = {
            "changed", "created", "deleted", "error"
        };
        DEBUG_PRINT ("emitting event %s for %s", names[event->code], event->filename);
        monitor->callback (watch, event, monitor->data);
    }

    moo_file_watch_unref (watch);
}


static Monitor *
monitor_new (MooFileWatch   *watch,
             const char     *filename,
             MooFileWatchCallback callback,
             gpointer        data,
             GDestroyNotify  notify)
{
    Monitor *mon;

    mon = g_new0 (Monitor, 1);
    mon->watch = watch;
    mon->filename = g_strdup (filename);

    mon->callback = callback;
    mon->notify = notify;
    mon->data = data;

    return mon;
}


static void
monitor_free (Monitor *monitor)
{
    if (monitor)
    {
        if (monitor->notify)
            monitor->notify (monitor->data);
        g_free (monitor->filename);
        g_free (monitor);
    }
}


/*****************************************************************************/
/* stat()
 */

#ifdef WANT_STAT_MONITOR

#define MOO_STAT_PRIORITY   G_PRIORITY_DEFAULT
#define MOO_STAT_TIMEOUT    500

static MooFileWatchError errno_to_file_error    (mgw_errno_t     code);
static gboolean do_stat                         (MooFileWatch   *watch);


static gboolean
watch_stat_start (MooFileWatch *watch,
                  G_GNUC_UNUSED GError **error)
{
    watch->stat_timeout =
            g_timeout_add_full (MOO_STAT_PRIORITY,
                                MOO_STAT_TIMEOUT,
                                (GSourceFunc) do_stat,
                                watch, NULL);
    return TRUE;
}


static gboolean
watch_stat_shutdown (MooFileWatch *watch,
                     G_GNUC_UNUSED GError **error)
{
    if (watch->stat_timeout)
        g_source_remove (watch->stat_timeout);
    watch->stat_timeout = 0;
    return TRUE;
}


static gboolean
watch_stat_start_monitor (MooFileWatch   *watch,
                          Monitor        *monitor,
                          GError        **error)
{
    MgwStatBuf buf;
    mgw_errno_t err;

    g_return_val_if_fail (watch != NULL, FALSE);
    g_return_val_if_fail (monitor->filename != NULL, FALSE);

    if (mgw_stat (monitor->filename, &buf, &err) != 0)
    {
        g_set_error (error, MOO_FILE_WATCH_ERROR,
                     errno_to_file_error (err),
                     "stat: %s", mgw_strerror (err));
        return FALSE;
    }

    monitor->isdir = buf.isdir;

    monitor->id = get_new_monitor_id ();
    monitor->statbuf = buf;

    return TRUE;
}


static gboolean
do_stat (MooFileWatch *watch)
{
    MonitorList *lm;
    GSList *list = NULL, *lid;
    GSList *to_remove = NULL;
    gboolean result = TRUE;

    g_return_val_if_fail (watch != NULL, FALSE);

    moo_file_watch_ref (watch);

    if (!watch->monitors)
        goto out;

    for (lm = watch->monitors; lm != NULL; lm = lm->next)
    {
        Monitor *m = lm->data;
        list = g_slist_prepend (list, GUINT_TO_POINTER (m->id));
    }

    /* Order of list is correct now, watch->monitors is last-added-first */
    for (lid = list; lid != NULL; lid = lid->next)
    {
        gboolean do_emit = FALSE;
        MooFileEvent event;
        Monitor *monitor;
        mgw_time_t old;
        mgw_errno_t err;

        monitor = (Monitor*) g_hash_table_lookup (watch->requests, lid->data);

        if (!monitor || !monitor->alive)
            continue;

        old = monitor->statbuf.mtime;

        event.monitor_id = monitor->id;
        event.filename = monitor->filename;
        event.error = NULL;

        if (mgw_stat (monitor->filename, &monitor->statbuf, &err) != 0)
        {
            if (err.value == MGW_ENOENT)
            {
                event.code = MOO_FILE_EVENT_DELETED;
                to_remove = g_slist_prepend (to_remove, GUINT_TO_POINTER (monitor->id));
            }
            else
            {
                event.code = MOO_FILE_EVENT_ERROR;
                g_set_error (&event.error, MOO_FILE_WATCH_ERROR,
                             errno_to_file_error (err),
                             "stat failed: %s",
                             mgw_strerror (err));
                monitor->alive = FALSE;
            }

            do_emit = TRUE;
        }
        else if (monitor->statbuf.mtime.value > old.value)
        {
            event.code = MOO_FILE_EVENT_CHANGED;
            do_emit = TRUE;
        }

        if (do_emit)
            moo_file_watch_emit_event (watch, &event, monitor);

        if (event.error)
            g_error_free (event.error);
    }

    for (lid = to_remove; lid != NULL; lid = lid->next)
        if (g_hash_table_lookup (watch->requests, GUINT_TO_POINTER (lid->data)))
            moo_file_watch_cancel_monitor (watch, GPOINTER_TO_UINT (lid->data));

    g_slist_free (to_remove);
    g_slist_free (list);

out:
    moo_file_watch_unref (watch);
    return result;
}


static MooFileWatchError
errno_to_file_error (mgw_errno_t code)
{
    MooFileWatchError fcode = MOO_FILE_WATCH_ERROR_FAILED;

    switch (code.value)
    {
#ifdef EACCESS
        case MGW_EACCES:
            fcode = MOO_FILE_WATCH_ERROR_ACCESS_DENIED;
            break;
#endif
        case MGW_ENAMETOOLONG:
            fcode = MOO_FILE_WATCH_ERROR_BAD_FILENAME;
            break;
        case MGW_ENOENT:
            fcode = MOO_FILE_WATCH_ERROR_NONEXISTENT;
            break;
        case MGW_ENOTDIR:
            fcode = MOO_FILE_WATCH_ERROR_NOT_DIR;
            break;

        default:
            break;
    }

    return fcode;
}


#endif /* WANT_STAT_MONITOR */
