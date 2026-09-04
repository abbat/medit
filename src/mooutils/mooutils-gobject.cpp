/*
 *   mooutils-gobject.c
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

#include "mooutils/mooutils-gobject-private.h"
#include "mooutils/mooclosure.h"
#include "mooutils/mootype-macros.h"
#include <gobject/gvaluecollector.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


/*****************************************************************************/
/* GType type
 */

#define MOO_GTYPE_PEEK(val_) (val_)->data[0].v_pointer

static void
moo_gtype_value_init (G_GNUC_UNUSED GValue *value)
{
}


static void
moo_gtype_value_copy (const GValue   *src,
                      GValue         *dest)
{
    MOO_GTYPE_PEEK(dest) = MOO_GTYPE_PEEK(src);
}


static char*
moo_gtype_collect_value (GValue         *value,
                         G_GNUC_UNUSED guint n_collect_values,
                         GTypeCValue    *collect_values,
                         G_GNUC_UNUSED guint collect_flags)
{
    MOO_GTYPE_PEEK(value) = collect_values->v_pointer;
    return NULL;
}


static char*
moo_gtype_lcopy_value (const GValue   *value,
                       G_GNUC_UNUSED guint n_collect_values,
                       GTypeCValue    *collect_values,
                       G_GNUC_UNUSED guint collect_flags)
{
    GType *ptr = (GType*) collect_values->v_pointer;
    *ptr = _moo_value_get_gtype (value);
    return NULL;
}


GType
_moo_gtype_get_type (void)
{
    static GType type = 0;

    if (G_UNLIKELY (!type))
    {
        static GTypeValueTable val_table = {
            moo_gtype_value_init,
            NULL,
            moo_gtype_value_copy,
            NULL,
            (char*) "p",
            moo_gtype_collect_value,
            (char*) "p",
            moo_gtype_lcopy_value
        };

        static GTypeInfo info = {
            /* interface types, classed types, instantiated types */
            0, /*class_size*/
            NULL, /*base_init*/
            NULL, /*base_finalize*/
            NULL,/*class_init*/
            NULL,/*class_finalize*/
            NULL,/*class_data*/
            0,/*instance_size*/
            0,/*n_preallocs*/
            NULL,/*instance_init*/
            /* value handling */
            &val_table
        };

        static GTypeFundamentalInfo finfo = { (GTypeFundamentalFlags) 0 };

        type = g_type_register_fundamental (g_type_fundamental_next (),
                                            "MooGType",
                                            &info, &finfo, (GTypeFlags) 0);
    }

    return type;
}


GType
_moo_value_get_gtype (const GValue *value)
{
    return (GType) MOO_GTYPE_PEEK(value);
}


/*****************************************************************************/
/* Converting values forth and back
 */

static char *
flags_to_string (int flags)
{
    if (flags)
        return g_strdup_printf ("%d", flags);
    else
        return g_strdup ("");
}


static gboolean
string_to_int (const char *string,
               int        *dest)
{
    char *end;
    long val;

    errno = 0;
    val = strtol (string, &end, 10);

    if (errno || !end || *end)
        return FALSE;

#if G_MAXINT != G_MAXLONG
    if (val > G_MAXINT || val < G_MININT)
        return FALSE;
#endif

    *dest = val;
    return TRUE;
}

static gboolean
string_to_uint (const char *string,
                guint      *dest)
{
    char *end;
    guint64 val;
    mgw_errno_t err;

    val = mgw_ascii_strtoull (string, &end, 10, &err);

    if (mgw_errno_is_set (err) || !end || *end)
        return FALSE;

    if (val > G_MAXUINT)
        return FALSE;

    *dest = val;
    return TRUE;
}

static gboolean
string_to_flags (const char *string,
                 GType       flags_type,
                 guint      *dest)
{
    gpointer klass;
    GFlagsClass *flags_class;
    guint ival = 0;
    gboolean seen_something = FALSE;
    gboolean error = FALSE;
    const char *end;

    if (!string || !string[0])
    {
        *dest = 0;
        return TRUE;
    }

    if (string_to_uint (string, dest))
        return TRUE;

    klass = g_type_class_ref (flags_type);
    g_return_val_if_fail (G_IS_FLAGS_CLASS (klass), FALSE);
    flags_class = G_FLAGS_CLASS (klass);

    while (*string && !error)
    {
        GFlagsValue *flags_value;
        char *single;
        guint tmp;

        while (g_ascii_isspace (string[0]) || string[0] == '|')
            string++;
        if (!string[0])
            break;
        end = string;
        while (end[0] && !g_ascii_isspace (end[0]) && end[0] != '|')
            end++;

        single = g_strndup (string, end - string);

        flags_value = g_flags_get_value_by_name (flags_class, single);
        if (!flags_value)
            flags_value = g_flags_get_value_by_nick (flags_class, single);

        if (flags_value)
            ival |= flags_value->value;
        else if (string_to_uint (single, &tmp))
            ival |= tmp;
        else
            error = TRUE;

        string = end;
        seen_something = TRUE;
        g_free (single);
    }

    if (!seen_something)
        error = TRUE;

    if (!error)
        *dest = ival;

    g_type_class_unref (flags_class);
    return !error;
}

static gboolean
string_to_enum (const char *string,
                GType       enum_type,
                int        *dest)
{
    gpointer klass;
    GEnumClass *enum_class;
    GEnumValue *enum_value;

    *dest = 0;

    if (!string || !string[0] || string_to_int (string, dest))
        return TRUE;

    klass = g_type_class_ref (enum_type);
    g_return_val_if_fail (G_IS_ENUM_CLASS (klass), FALSE);
    enum_class = G_ENUM_CLASS (klass);

    enum_value = g_enum_get_value_by_name (enum_class, string);
    if (!enum_value)
        enum_value = g_enum_get_value_by_nick (enum_class, string);

    if (enum_value)
    {
        *dest = enum_value->value;
        g_type_class_unref (enum_class);
        return TRUE;
    }

    g_type_class_unref (enum_class);
    return FALSE;
}


gboolean
_moo_value_convert (const GValue *src,
                    GValue       *dest)
{
    GType src_type, dest_type;

    g_return_val_if_fail (G_IS_VALUE (src) && G_IS_VALUE (dest), FALSE);

    src_type = G_VALUE_TYPE (src);
    dest_type = G_VALUE_TYPE (dest);

    g_return_val_if_fail (_moo_value_type_supported (src_type), FALSE);
    g_return_val_if_fail (_moo_value_type_supported (dest_type), FALSE);

    if (src_type == dest_type)
    {
        g_value_copy (src, dest);
        return TRUE;
    }

    if (dest_type == G_TYPE_STRING)
    {
        if (src_type == G_TYPE_BOOLEAN)
        {
            const char *string =
                    g_value_get_boolean (src) ? "TRUE" : "FALSE";
            g_value_set_static_string (dest, string);
            return TRUE;
        }

        if (src_type == G_TYPE_DOUBLE)
        {
            char *string =
                    g_strdup_printf ("%f", g_value_get_double (src));
            g_value_take_string (dest, string);
            return TRUE;
        }

        if (src_type == G_TYPE_INT)
        {
            char *string =
                    g_strdup_printf ("%d", g_value_get_int (src));
            g_value_take_string (dest, string);
            return TRUE;
        }

        if (src_type == G_TYPE_UINT)
        {
            char *string =
                    g_strdup_printf ("%u", g_value_get_uint (src));
            g_value_take_string (dest, string);
            return TRUE;
        }

        if (src_type == GDK_TYPE_COLOR)
        {
            char string[14];
            const GdkColor *color = (const GdkColor*) g_value_get_boxed (src);

            if (!color)
            {
                g_value_set_string (dest, NULL);
                return TRUE;
            }
            else
            {
                g_snprintf (string, 8, "#%02x%02x%02x",
                            color->red >> 8,
                            color->green >> 8,
                            color->blue >> 8);
                g_value_set_string (dest, string);
                return TRUE;
            }
        }

        if (G_TYPE_IS_ENUM (src_type))
        {
            gpointer klass;
            GEnumClass *enum_class;
            GEnumValue *enum_value;

            klass = g_type_class_ref (src_type);
            g_return_val_if_fail (G_IS_ENUM_CLASS (klass), FALSE);
            enum_class = G_ENUM_CLASS (klass);

            enum_value = g_enum_get_value (enum_class,
                                           g_value_get_enum (src));

            if (!enum_value)
            {
                char *string = g_strdup_printf ("%d", g_value_get_enum (src));
                g_value_take_string (dest, string);
            }
            else
            {
                g_value_set_string (dest, enum_value->value_nick);
            }

            g_type_class_unref (klass);
            return TRUE;
        }

        if (G_TYPE_IS_FLAGS (src_type))
        {
            char *string = flags_to_string (g_value_get_flags (src));
            g_value_take_string (dest, string);
            return TRUE;
        }

        g_return_val_if_reached (FALSE);
    }

    if (src_type == G_TYPE_STRING)
    {
        const char *string = g_value_get_string (src);

        if (dest_type == G_TYPE_BOOLEAN)
        {
            if (!string || !string[0])
                g_value_set_boolean (dest, FALSE);
            else if (g_ascii_strcasecmp (string, "1") == 0 ||
                     g_ascii_strcasecmp (string, "yes") == 0 ||
                     g_ascii_strcasecmp (string, "true") == 0)
                g_value_set_boolean (dest, TRUE);
            else if (g_ascii_strcasecmp (string, "0") == 0 ||
                     g_ascii_strcasecmp (string, "no") == 0 ||
                     g_ascii_strcasecmp (string, "false") == 0)
                g_value_set_boolean (dest, FALSE);
            else
                return FALSE;
            return TRUE;
        }

        if (dest_type == G_TYPE_DOUBLE)
        {
            double val = 0.;
            char *end;

            if (string && string[0])
            {
                mgw_errno_t err;
                val = mgw_ascii_strtod (string, &end, &err);
                if (mgw_errno_is_set (err) || !end || *end)
                    return FALSE;
            }

            g_value_set_double (dest, val);
            return TRUE;
        }

        if (dest_type == G_TYPE_INT)
        {
            int val = 0;

            if (string && string[0] && !string_to_int (string, &val))
                return FALSE;

            g_value_set_int (dest, val);
            return TRUE;
        }

        if (dest_type == G_TYPE_UINT)
        {
            guint val = 0;

            if (string && string[0] && !string_to_uint (string, &val))
                return FALSE;

            g_value_set_uint (dest, val);
            return TRUE;
        }

        if (dest_type == GDK_TYPE_COLOR)
        {
            GdkColor color;

            if (!string || !string[0])
            {
                g_value_set_boxed (dest, NULL);
                return TRUE;
            }

            g_return_val_if_fail (gdk_color_parse (string, &color),
                                  FALSE);

            g_value_set_boxed (dest, &color);
            return TRUE;
        }

        if (G_TYPE_IS_ENUM (dest_type))
        {
            int ival;

            if (string_to_enum (string, dest_type, &ival))
            {
                g_value_set_enum (dest, ival);
                return TRUE;
            }

            return FALSE;
        }

        if (G_TYPE_IS_FLAGS (dest_type))
        {
            guint flags;

            if (string_to_flags (string, dest_type, &flags))
            {
                g_value_set_flags (dest, flags);
                return TRUE;
            }

            return FALSE;
        }

        g_return_val_if_reached (FALSE);
    }

    if (G_TYPE_IS_ENUM (src_type) && dest_type == G_TYPE_INT)
    {
        g_value_set_int (dest, g_value_get_enum (src));
        return TRUE;
    }

    if (G_TYPE_IS_ENUM (dest_type) && src_type == G_TYPE_INT)
    {
        g_value_set_enum (dest, g_value_get_int (src));
        return TRUE;
    }

    if (G_TYPE_IS_FLAGS (src_type) && dest_type == G_TYPE_INT)
    {
        g_value_set_int (dest, g_value_get_flags (src));
        return TRUE;
    }

    if (G_TYPE_IS_FLAGS (dest_type) && src_type == G_TYPE_INT)
    {
        g_value_set_flags (dest, g_value_get_int (src));
        return TRUE;
    }

    if (src_type == G_TYPE_DOUBLE && dest_type == G_TYPE_INT)
    {
        g_value_set_int (dest, g_value_get_double (src));
        return TRUE;
    }

    if (dest_type == G_TYPE_DOUBLE && src_type == G_TYPE_INT)
    {
        g_value_set_double (dest, g_value_get_int (src));
        return TRUE;
    }

    g_return_val_if_reached (FALSE);
}


gboolean
_moo_value_equal (const GValue *a,
                  const GValue *b)
{
    GType type;

    g_return_val_if_fail (G_IS_VALUE (a) && G_IS_VALUE (b), a == b);
    g_return_val_if_fail (G_VALUE_TYPE (a) == G_VALUE_TYPE (b), a == b);

    type = G_VALUE_TYPE (a);

    if (type == G_TYPE_BOOLEAN)
    {
        gboolean ba = g_value_get_boolean (a);
        gboolean bb = g_value_get_boolean (b);
        return (ba && bb) || (!ba && !bb);
    }

    if (type == G_TYPE_INT)
        return g_value_get_int (a) == g_value_get_int (b);

    if (type == G_TYPE_UINT)
        return g_value_get_uint (a) == g_value_get_uint (b);

    if (type == G_TYPE_DOUBLE)
        return g_value_get_double (a) == g_value_get_double (b);

    if (type == G_TYPE_STRING)
    {
        const char *sa, *sb;

        sa = g_value_get_string (a);
        sb = g_value_get_string (b);

        if (!sa || !sb)
            return sa == sb;
        else
            return !strcmp (sa, sb);
    }

    if (type == GDK_TYPE_COLOR)
    {
        const GdkColor *ca, *cb;

        ca = (const GdkColor*) g_value_get_boxed (a);
        cb = (const GdkColor*) g_value_get_boxed (b);

        if (!ca || !cb)
            return ca == cb;
        else
            return ca->red == cb->red &&
                    ca->green == cb->green &&
                    ca->blue == cb->blue;
    }

    if (G_TYPE_IS_ENUM (type))
        return g_value_get_enum (a) == g_value_get_enum (b);

    if (G_TYPE_IS_FLAGS (type))
        return g_value_get_flags (a) == g_value_get_flags (b);

    g_return_val_if_reached (a == b);
}


gboolean
_moo_value_type_supported (GType type)
{
    return type == G_TYPE_BOOLEAN ||
            type == G_TYPE_INT ||
            type == G_TYPE_UINT ||
            type == G_TYPE_DOUBLE ||
            type == G_TYPE_STRING ||
            type == GDK_TYPE_COLOR ||
            G_TYPE_IS_ENUM (type) ||
            G_TYPE_IS_FLAGS (type);
}


static gboolean
_moo_value_convert_to_bool (const GValue *val,
                            gboolean     *dest)
{
    GValue result = {0};

    g_value_init (&result, G_TYPE_BOOLEAN);

    if (_moo_value_convert (val, &result))
    {
        *dest = g_value_get_boolean (&result);
        return TRUE;
    }

    return FALSE;
}


static gboolean
_moo_value_convert_to_int (const GValue *val,
                           int          *dest)
{
    GValue result = {0};

    g_value_init (&result, G_TYPE_INT);

    if (_moo_value_convert (val, &result))
    {
        *dest = g_value_get_int (&result);
        return TRUE;
    }

    return FALSE;
}

static gboolean
_moo_value_convert_to_uint (const GValue *val,
                            guint        *dest)
{
    GValue result = {0};

    g_value_init (&result, G_TYPE_UINT);

    if (_moo_value_convert (val, &result))
    {
        *dest = g_value_get_uint (&result);
        return TRUE;
    }

    return FALSE;
}

double
_moo_value_convert_to_double (const GValue *val)
{
    GValue result = {0};
    g_value_init (&result, G_TYPE_DOUBLE);
    if (!_moo_value_convert (val, &result))
        g_warning ("%s: could not convert value to double", G_STRFUNC);
    return g_value_get_double (&result);
}




const char*
_moo_value_convert_to_string (const GValue *val)
{
    static GValue result;

    if (G_IS_VALUE (&result))
        g_value_unset (&result);

    g_value_init (&result, G_TYPE_STRING);

    if (!_moo_value_convert (val, &result))
        return NULL;
    else
        return g_value_get_string (&result);
}


gboolean
_moo_value_convert_from_string (const char *string,
                                GValue     *val)
{
    GValue str_val = {0};
    gboolean result;

    g_return_val_if_fail (G_IS_VALUE (val), FALSE);
    g_return_val_if_fail (string != NULL, FALSE);

    g_value_init (&str_val, G_TYPE_STRING);
    g_value_set_static_string (&str_val, string);
    result = _moo_value_convert (&str_val, val);
    g_value_unset (&str_val);

    return result;
}


int
_moo_convert_string_to_int (const char *string,
                            int         default_val)
{
    int int_val = default_val;

    if (string && string[0])
    {
        GValue str_val = {0};

        g_value_init (&str_val, G_TYPE_STRING);
        g_value_set_static_string (&str_val, string);

        if (!_moo_value_convert_to_int (&str_val, &int_val))
            g_warning ("%s: could not convert string '%s' to int",
                       G_STRFUNC, string);

        g_value_unset (&str_val);
    }

    return int_val;
}

guint
_moo_convert_string_to_uint (const char *string,
                             guint       default_val)
{
    guint int_val = default_val;

    if (string && string[0])
    {
        GValue str_val = {0};

        g_value_init (&str_val, G_TYPE_STRING);
        g_value_set_static_string (&str_val, string);

        if (!_moo_value_convert_to_uint (&str_val, &int_val))
            g_warning ("%s: could not convert string '%s' to uint",
                       G_STRFUNC, string);

        g_value_unset (&str_val);
    }

    return int_val;
}

gboolean
_moo_convert_string_to_bool (const char *string,
                             gboolean    default_val)
{
    gboolean bool_val = default_val;

    if (string && string[0])
    {
        GValue str_val = {0};

        g_value_init (&str_val, G_TYPE_STRING);
        g_value_set_static_string (&str_val, string);

        if (!_moo_value_convert_to_bool (&str_val, &bool_val))
            g_warning ("%s: could not convert string '%s' to boolean",
                       G_STRFUNC, string);

        g_value_unset (&str_val);
    }

    return bool_val;
}


const char*
_moo_convert_bool_to_string (gboolean value)
{
    GValue bool_val = {0};

    g_value_init (&bool_val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&bool_val, value);

    return _moo_value_convert_to_string (&bool_val);
}


gboolean
_moo_value_change_type (GValue *val,
                        GType   new_type)
{
    GValue tmp = {0};
    gboolean result;

    g_return_val_if_fail (G_IS_VALUE (val), FALSE);
    g_return_val_if_fail (_moo_value_type_supported (new_type), FALSE);

    g_value_init (&tmp, new_type);
    result = _moo_value_convert (val, &tmp);

    if (result)
    {
        g_value_unset (val);
        *val = tmp;
    }

    return result;
}

/*****************************************************************************/
/* GParameter array manipulation
 */



void
_moo_param_array_free (GParameter *array,
                       guint       len)
{
    guint i;

    for (i = 0; i < len; ++i)
    {
        g_value_unset (&array[i].value);
        g_free ((char*)array[i].name);
    }

    g_free (array);
}


/*****************************************************************************/
/* Signal that does not require class method
 */

guint
_moo_signal_new_cb (const gchar        *signal_name,
                    GType               itype,
                    GSignalFlags        signal_flags,
                    GCallback           handler,
                    GSignalAccumulator  accumulator,
                    gpointer            accu_data,
                    GSignalCMarshaller  c_marshaller,
                    GType               return_type,
                    guint               n_params,
                    ...)
{
    va_list args;
    guint signal_id;
    GClosure *closure = NULL;

    g_return_val_if_fail (signal_name != NULL, 0);

    va_start (args, n_params);

    if (handler)
        closure = g_cclosure_new (handler, NULL, NULL);

    signal_id = g_signal_new_valist (signal_name, itype, signal_flags, closure,
                                     accumulator, accu_data, c_marshaller,
                                     return_type, n_params, args);

    va_end (args);

    return signal_id;
}


/****************************************************************************/
/* Property watch
 */

static GHashTable *watches = NULL;
static guint watch_last_id = 0;

#define Watch MooObjectWatch
#define WatchClass MooObjectWatchClass
#define watch_new _moo_object_watch_new
#define watch_alloc _moo_object_watch_alloc


static void
watch_destroy (Watch *w)
{
    if (w)
    {
        if (w->klass->destroy)
            w->klass->destroy (w);
        if (w->notify)
            w->notify (w->notify_data);
        _moo_object_ptr_free (w->source);
        _moo_object_ptr_free (w->target);
        g_free (w);
    }
}


static void
watch_source_died (Watch *w, GObject*)
{
    if (w->klass->source_notify)
        w->klass->source_notify (w);
    _moo_object_ptr_free (w->source);
    w->source = NULL;
    g_hash_table_remove (watches, GUINT_TO_POINTER (w->id));
}

static void
watch_target_died (Watch *w, GObject*)
{
    if (w->klass->target_notify)
        w->klass->target_notify (w);
    _moo_object_ptr_free (w->target);
    w->target = NULL;
    g_hash_table_remove (watches, GUINT_TO_POINTER (w->id));
}


Watch *
watch_alloc (gsize          size,
             WatchClass    *klass,
             gpointer       source,
             gpointer       target,
             GDestroyNotify notify,
             gpointer       notify_data)
{
    Watch *w;

    g_return_val_if_fail (size >= sizeof (Watch), NULL);
    g_return_val_if_fail (G_IS_OBJECT (source), NULL);
    g_return_val_if_fail (G_IS_OBJECT (target), NULL);

    w = (Watch*) g_malloc0 (size);
    w->source = _moo_object_ptr_new (G_OBJECT (source), (GWeakNotify) watch_source_died, w);
    w->target = _moo_object_ptr_new (G_OBJECT (target), (GWeakNotify) watch_target_died, w);
    w->klass = klass;
    w->notify = notify;
    w->notify_data = notify_data;
    w->id = ++watch_last_id;

    if (!watches)
        watches = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, (GDestroyNotify) watch_destroy);
    g_hash_table_insert (watches, GUINT_TO_POINTER (w->id), w);

    return w;
}


typedef struct {
    Watch parent;
    GParamSpec *source_pspec;
    GParamSpec *target_pspec;
    MooTransformPropFunc transform;
    gpointer transform_data;
} PropWatch;


static void prop_watch_check        (PropWatch  *watch);
static void prop_watch_destroy      (Watch      *watch);

static WatchClass PropWatchClass = {NULL, NULL, prop_watch_destroy};


static PropWatch*
prop_watch_new (GObject            *target,
                const char         *target_prop,
                GObject            *source,
                const char         *source_prop,
                const char         *signal,
                MooTransformPropFunc transform,
                gpointer            transform_data,
                GDestroyNotify      destroy_notify,
                gpointer            destroy_notify_data)
{
    PropWatch *watch;
    GObjectClass *target_class, *source_class;
    char *signal_name = NULL;
    GParamSpec *source_pspec;
    GParamSpec *target_pspec;

    g_return_val_if_fail (G_IS_OBJECT (target), NULL);
    g_return_val_if_fail (G_IS_OBJECT (source), NULL);
    g_return_val_if_fail (target_prop != NULL, NULL);
    g_return_val_if_fail (source_prop != NULL, NULL);
    g_return_val_if_fail (transform != NULL, NULL);

    target_class = (GObjectClass*) g_type_class_peek (G_OBJECT_TYPE (target));
    source_class = (GObjectClass*) g_type_class_peek (G_OBJECT_TYPE (source));

    source_pspec = g_object_class_find_property (source_class, source_prop);
    target_pspec = g_object_class_find_property (target_class, target_prop);

    if (!source_pspec || !target_pspec)
    {
        if (!source_pspec)
            g_warning ("no property '%s' in class '%s'",
                       source_prop, g_type_name (G_OBJECT_TYPE (source)));
        if (!target_pspec)
            g_warning ("no property '%s' in class '%s'",
                       target_prop, g_type_name (G_OBJECT_TYPE (target)));
        return NULL;
    }

    watch = watch_new (PropWatch, &PropWatchClass, source, target,
                       destroy_notify, destroy_notify_data);

    watch->source_pspec = source_pspec;
    watch->target_pspec = target_pspec;
    watch->transform = transform;
    watch->transform_data = transform_data;

    if (!signal)
    {
        signal_name = g_strdup_printf ("notify::%s", source_prop);
        signal = signal_name;
    }

    g_signal_connect_swapped (source, signal,
                              G_CALLBACK (prop_watch_check),
                              watch);

    g_free (signal_name);
    return watch;
}


static void
prop_watch_destroy (Watch *watch)
{
    if (MOO_OBJECT_PTR_GET (watch->source))
        g_signal_handlers_disconnect_by_func (MOO_OBJECT_PTR_GET (watch->source),
                                              (gpointer) prop_watch_check,
                                              watch);
}


guint
_moo_add_property_watch (gpointer            target,
                         const char         *target_prop,
                         gpointer            source,
                         const char         *source_prop,
                         MooTransformPropFunc transform,
                         gpointer            transform_data,
                         GDestroyNotify      destroy_notify)
{
    PropWatch *watch;

    g_return_val_if_fail (G_IS_OBJECT (target), 0);
    g_return_val_if_fail (G_IS_OBJECT (source), 0);
    g_return_val_if_fail (target_prop != NULL, 0);
    g_return_val_if_fail (source_prop != NULL, 0);
    g_return_val_if_fail (transform != NULL, 0);

    watch = prop_watch_new (G_OBJECT (target), target_prop, 
                            G_OBJECT (source), source_prop,
                            NULL, transform, transform_data,
                            destroy_notify, transform_data);

    if (!watch)
        return 0;

    prop_watch_check (watch);
    return watch->parent.id;
}


static void
prop_watch_check (PropWatch *watch)
{
    GValue source_val = {0}, target_val = {0}, old_target_val = {0};
    GObject *source, *target;

    source = MOO_OBJECT_PTR_GET (watch->parent.source);
    target = MOO_OBJECT_PTR_GET (watch->parent.target);
    g_return_if_fail (source && target);

    g_value_init (&source_val, watch->source_pspec->value_type);
    g_value_init (&target_val, watch->target_pspec->value_type);
    g_value_init (&old_target_val, watch->target_pspec->value_type);

    g_object_ref (source);
    g_object_ref (target);

    g_object_get_property (source,
                           watch->source_pspec->name,
                           &source_val);
    g_object_get_property (target,
                           watch->target_pspec->name,
                           &old_target_val);

    watch->transform (&target_val, &source_val, watch->transform_data);

    if (g_param_values_cmp (watch->target_pspec, &target_val, &old_target_val))
        g_object_set_property (target,
                               watch->target_pspec->name,
                               &target_val);

    g_object_unref (source);
    g_object_unref (target);
    g_value_unset (&source_val);
    g_value_unset (&target_val);
    g_value_unset (&old_target_val);
}


static void
_moo_copy_boolean (GValue             *target,
                   const GValue       *source,
                   G_GNUC_UNUSED gpointer dummy)
{
    g_value_set_boolean (target, g_value_get_boolean (source) ? TRUE : FALSE);
}


static void
_moo_invert_boolean (GValue             *target,
                     const GValue       *source,
                     G_GNUC_UNUSED gpointer dummy)
{
    g_value_set_boolean (target, !g_value_get_boolean (source));
}


guint
moo_bind_bool_property (gpointer            target,
                        const char         *target_prop,
                        gpointer            source,
                        const char         *source_prop,
                        gboolean            invert)
{
    g_return_val_if_fail (G_IS_OBJECT (target), 0);
    g_return_val_if_fail (G_IS_OBJECT (source), 0);
    g_return_val_if_fail (target_prop && source_prop, 0);

    if (invert)
        return _moo_add_property_watch (target, target_prop, source, source_prop,
                                        _moo_invert_boolean, NULL, NULL);
    else
        return _moo_add_property_watch (target, target_prop, source, source_prop,
                                        _moo_copy_boolean, NULL, NULL);
}




void
moo_bind_sensitive (GtkWidget *btn,
                    GtkWidget *dependent,
                    gboolean   invert)
{
    g_return_if_fail (G_IS_OBJECT (btn));
    g_return_if_fail (G_IS_OBJECT (dependent));
    moo_bind_bool_property (dependent, "sensitive", btn, "active", invert);
}




/*****************************************************************************/
/* Data store
 */

struct _MooData
{
    GHashTable *hash;
    guint ref_count;
    GHashFunc hash_func;
    GEqualFunc key_equal_func;
    GDestroyNotify key_destroy_func;
};


static MooPtr *
_moo_ptr_ref (MooPtr *ptr)
{
    if (ptr)
        ptr->ref_count++;
    return ptr;
}

static void
_moo_ptr_unref (MooPtr *ptr)
{
    if (ptr && !--(ptr->ref_count))
    {
        if (ptr->free_func)
            ptr->free_func (ptr->data);
        g_free (ptr);
    }
}

MOO_DEFINE_BOXED_TYPE_R (MooPtr, _moo_ptr)

static MooPtr *
_moo_ptr_new (gpointer        data,
              GDestroyNotify  free_func)
{
    MooPtr *ptr;

    g_return_val_if_fail (data != NULL, NULL);

    ptr = g_new (MooPtr, 1);

    ptr->data = data;
    ptr->free_func = free_func;
    ptr->ref_count = 1;

    return ptr;
}


static MooData *
_moo_data_ref (MooData *data)
{
    if (data)
        data->ref_count++;
    return data;
}


void
_moo_data_unref (MooData *data)
{
    if (data && !--(data->ref_count))
    {
        g_hash_table_destroy (data->hash);
        g_free (data);
    }
}

MOO_DEFINE_BOXED_TYPE_R (MooData, _moo_data)


static void
free_gvalue (GValue *val)
{
    if (val)
    {
        g_value_unset (val);
        g_free (val);
    }
}


static GValue *
copy_gvalue (const GValue *val)
{
    GValue *copy;

    g_return_val_if_fail (G_IS_VALUE (val), NULL);

    copy = g_new (GValue, 1);
    copy->g_type = 0;
    g_value_init (copy, G_VALUE_TYPE (val));
    g_value_copy (val, copy);

    return copy;
}


MooData *
_moo_data_new (GHashFunc       hash_func,
               GEqualFunc      key_equal_func,
               GDestroyNotify  key_destroy_func)
{
    MooData *data;

    g_return_val_if_fail (hash_func != NULL, NULL);
    g_return_val_if_fail (key_equal_func != NULL, NULL);

    data = g_new0 (MooData, 1);

    data->hash = g_hash_table_new_full (hash_func, key_equal_func,
                                        key_destroy_func,
                                        (GDestroyNotify) free_gvalue);
    data->ref_count = 1;
    data->hash_func = hash_func;
    data->key_equal_func = key_equal_func;
    data->key_destroy_func = key_destroy_func;

    return data;
}


void
_moo_data_insert_value (MooData        *data,
                        gpointer        key,
                        const GValue   *value)
{
    g_return_if_fail (data != NULL);
    g_return_if_fail (G_IS_VALUE (value));
    g_hash_table_insert (data->hash, key, copy_gvalue (value));
}


void
_moo_data_insert_ptr (MooData        *data,
                      gpointer        key,
                      gpointer        value,
                      GDestroyNotify  destroy)
{
    MooPtr *ptr;
    GValue gval = {0};

    g_return_if_fail (data != NULL);
    g_return_if_fail (value != NULL);

    ptr = _moo_ptr_new (value, destroy);
    g_value_init (&gval, MOO_TYPE_PTR);
    g_value_set_boxed (&gval, ptr);

    _moo_data_insert_value (data, key, &gval);

    g_value_unset (&gval);
    _moo_ptr_unref (ptr);
}


void
_moo_data_remove (MooData  *data,
                  gpointer  key)
{
    g_return_if_fail (data != NULL);
    g_hash_table_remove (data->hash, key);
}


void
_moo_data_clear (MooData *data)
{
    g_return_if_fail (data != NULL);

    g_hash_table_destroy (data->hash);

    data->hash = g_hash_table_new_full (data->hash_func,
                                        data->key_equal_func,
                                        data->key_destroy_func,
                                        (GDestroyNotify) free_gvalue);
}




gboolean
_moo_data_get_value (MooData        *data,
                     gpointer        key,
                     GValue         *dest)
{
    GValue *value;

    g_return_val_if_fail (data != NULL, FALSE);
    g_return_val_if_fail (!dest || dest->g_type == 0, FALSE);

    value = (GValue*) g_hash_table_lookup (data->hash, key);

    if (value && dest)
    {
        g_value_init (dest, G_VALUE_TYPE (value));
        g_value_copy (value, dest);
    }

    return value != NULL;
}


gpointer
_moo_data_get_ptr (MooData  *data,
                   gpointer  key)
{
    MooPtr *ptr;
    GValue *value;

    g_return_val_if_fail (data != NULL, NULL);

    value = (GValue*) g_hash_table_lookup (data->hash, key);

    if (value)
    {
        g_return_val_if_fail (G_VALUE_TYPE (value) == MOO_TYPE_PTR, NULL);
        ptr = (MooPtr*) g_value_get_boxed (value);
        return ptr->data;
    }
    else
    {
        return NULL;
    }
}
