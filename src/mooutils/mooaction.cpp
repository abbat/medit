/*
 *   mooaction.c
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

/**
 * class:MooAction: (parent GObject) (moo.private 1)
 **/

#include "mooutils/mooaction-private.h"
#include "mooutils/mooactionbase-private.h"
#include "mooutils/mooutils-gobject.h"
#include "mooutils/mooactiongroup.h"


static void _moo_action_set_closure (MooAction  *action,
                                     MooClosure *closure);
static void moo_action_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void moo_action_get_property (GObject      *object,
                                     guint         property_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);


gpointer
_moo_action_get_window (gpointer action)
{
    MooActionGroup *group;
    MooActionCollection *collection;

    g_return_val_if_fail (MOO_IS_ACTION (action), NULL);

    group = _moo_action_get_group (action);
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);

    collection = _moo_action_group_get_collection (group);
    return _moo_action_collection_get_window (collection);
}


/*****************************************************************************/
/* MooAction
 */

G_DEFINE_TYPE_WITH_CODE (MooAction, moo_action, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOO_TYPE_ACTION_BASE, NULL)
                         G_ADD_PRIVATE (MooAction))

enum {
    ACTION_ACTIVATE,
    N_ACTION_SIGNALS
};

static guint action_signals[N_ACTION_SIGNALS];


enum {
    MOO_ACTION_BASE_PROPS(ACTION),
    ACTION_PROP_CLOSURE,
    ACTION_PROP_CLOSURE_OBJECT,
    ACTION_PROP_CLOSURE_SIGNAL,
    ACTION_PROP_CLOSURE_CALLBACK,
    ACTION_PROP_CLOSURE_PROXY_FUNC
};


static void
moo_action_init (MooAction *action)
{
    action->priv = (MooActionPrivate*) moo_action_get_instance_private (action);
    action->priv->sensitive = TRUE;
    action->priv->visible = TRUE;

    _moo_action_base_init_instance (action);
}


static void
moo_action_dispose (GObject *object)
{
    MooAction *action = MOO_ACTION (object);

    if (action->priv->closure)
    {
        moo_closure_unref (action->priv->closure);
        action->priv->closure = NULL;
    }

    G_OBJECT_CLASS (moo_action_parent_class)->dispose (object);
}


static void
moo_action_finalize (GObject *object)
{
    MooActionPrivate *priv = MOO_ACTION (object)->priv;

    g_free (priv->name);
    g_free (priv->label);
    g_free (priv->tooltip);
    g_free (priv->icon_name);

    G_OBJECT_CLASS (moo_action_parent_class)->finalize (object);
}


static void
moo_action_activate_real (MooAction *action)
{
    if (action->priv->closure)
        moo_closure_invoke (action->priv->closure);
}


static GObject *
moo_action_constructor (GType                  type,
                        guint                  n_props,
                        GObjectConstructParam *props)
{
    guint i;
    GObject *object;
    MooClosure *closure = NULL;
    gpointer closure_object = NULL;
    const char *closure_signal = NULL;
    GCallback closure_callback = NULL;
    GCallback closure_proxy_func = NULL;

    for (i = 0; i < n_props; ++i)
    {
        const char *name = props[i].pspec->name;
        GValue *value = props[i].value;

        if (!strcmp (name, "closure-object"))
            closure_object = g_value_get_object (value);
        else if (!strcmp (name, "closure-signal"))
            closure_signal = g_value_get_string (value);
        else if (!strcmp (name, "closure-callback"))
            closure_callback = (GCallback) g_value_get_pointer (value);
        else if (!strcmp (name, "closure-proxy-func"))
            closure_proxy_func = (GCallback) g_value_get_pointer (value);
    }

    if (closure_callback || closure_signal)
    {
        if (closure_object)
        {
            closure = _moo_closure_new_simple (closure_object, closure_signal,
                                               closure_callback, closure_proxy_func);
            moo_closure_ref_sink (closure);
        }
        else
            g_critical ("closure data missing");
    }

    object = G_OBJECT_CLASS(moo_action_parent_class)->constructor (type, n_props, props);

    if (closure)
    {
        _moo_action_set_closure (MOO_ACTION (object), closure);
        moo_closure_unref (closure);
    }

    return object;
}


static void
moo_action_class_init (MooActionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = moo_action_set_property;
    object_class->get_property = moo_action_get_property;
    object_class->dispose = moo_action_dispose;
    object_class->finalize = moo_action_finalize;
    object_class->constructor = moo_action_constructor;
    klass->activate = moo_action_activate_real;

    _moo_action_base_init_class (object_class);

    action_signals[ACTION_ACTIVATE] =
        g_signal_new ("activate", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MooActionClass, activate),
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_object_class_install_property (object_class, ACTION_PROP_CLOSURE,
                                     g_param_spec_boxed ("closure", "closure", "closure",
                                                         MOO_TYPE_CLOSURE,
                                                         (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (object_class, ACTION_PROP_CLOSURE_OBJECT,
                                     g_param_spec_object ("closure-object", "closure-object", "closure-object",
                                                          G_TYPE_OBJECT, (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (object_class, ACTION_PROP_CLOSURE_SIGNAL,
                                     g_param_spec_string ("closure-signal", "closure-signal", "closure-signal",
                                                          NULL, (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (object_class, ACTION_PROP_CLOSURE_CALLBACK,
                                     g_param_spec_pointer ("closure-callback", "closure-callback", "closure-callback",
                                                           (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (object_class, ACTION_PROP_CLOSURE_PROXY_FUNC,
                                     g_param_spec_pointer ("closure-proxy-func", "closure-proxy-func", "closure-proxy-func",
                                                           (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
}


static void
moo_action_set_property (GObject            *object,
                         guint               property_id,
                         const GValue       *value,
                         GParamSpec         *pspec)
{
    MooAction *action = MOO_ACTION (object);

    switch (property_id)
    {
        case ACTION_PROP_CLOSURE:
            _moo_action_set_closure (action, (MooClosure*) g_value_get_boxed (value));
            break;

        case ACTION_PROP_CLOSURE_OBJECT:
        case ACTION_PROP_CLOSURE_SIGNAL:
        case ACTION_PROP_CLOSURE_CALLBACK:
        case ACTION_PROP_CLOSURE_PROXY_FUNC:
            /* these are handled in the constructor */
            break;

        MOO_ACTION_BASE_SET_PROPERTY (ACTION);

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
moo_action_get_property (GObject    *object,
                         guint       property_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
    MooAction *action = MOO_ACTION (object);

    switch (property_id)
    {
        case ACTION_PROP_CLOSURE:
            g_value_set_boxed (value, action->priv->closure);
            break;

        MOO_ACTION_BASE_GET_PROPERTY (ACTION);

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
_moo_action_set_closure (MooAction  *action,
                         MooClosure *closure)
{
    g_return_if_fail (MOO_IS_ACTION (action));

    if (closure == action->priv->closure)
        return;

    if (action->priv->closure)
        moo_closure_unref (action->priv->closure);
    if (closure)
        moo_closure_ref_sink (closure);

    action->priv->closure = closure;
    g_object_notify (G_OBJECT (action), "closure");
}


/*****************************************************************************/
/* MooToggleAction
 */

typedef void (*MooToggleActionCallback)     (gpointer            data,
                                             gboolean            active);

struct _MooToggleActionPrivate {
    MooToggleActionCallback callback;
    MooObjectPtr *ptr;
    gpointer data;
    gboolean active;
};


G_DEFINE_TYPE_WITH_CODE (MooToggleAction, moo_toggle_action, MOO_TYPE_ACTION,
                         G_ADD_PRIVATE (MooToggleAction))

enum {
    TOGGLED_TOGGLED,
    N_TOGGLE_SIGNALS
};

static guint toggle_signals[N_TOGGLE_SIGNALS];

static void moo_toggle_action_set_property  (GObject      *object,
                                             guint         property_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void moo_toggle_action_get_property  (GObject      *object,
                                             guint         property_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);


enum {
    TOGGLE_ACTION_PROP_0,
    TOGGLE_ACTION_PROP_ACTIVE,
    TOGGLE_ACTION_PROP_TOGGLED_CALLBACK,
    TOGGLE_ACTION_PROP_TOGGLED_OBJECT,
    TOGGLE_ACTION_PROP_TOGGLED_DATA
};


static void
moo_toggle_action_init (MooToggleAction *action)
{
    action->priv = (MooToggleActionPrivate*) moo_toggle_action_get_instance_private (action);
}


static void
moo_toggle_action_dispose (GObject *object)
{
    MooToggleAction *action = MOO_TOGGLE_ACTION (object);

    if (action->priv->ptr)
    {
        _moo_object_ptr_free (action->priv->ptr);
        action->priv->ptr = NULL;
    }

    G_OBJECT_CLASS (moo_toggle_action_parent_class)->dispose (object);
}


gboolean
moo_toggle_action_get_active (MooToggleAction *action)
{
    g_return_val_if_fail (MOO_IS_TOGGLE_ACTION (action), FALSE);
    return action->priv->active;
}


/* Like gtk_toggle_action_set_active(): goes through "activate", so a handler
   of it sees the change, but does not look at the sensitivity. */
void
moo_toggle_action_set_active (MooToggleAction *action,
                              gboolean         active)
{
    g_return_if_fail (MOO_IS_TOGGLE_ACTION (action));

    if (!action->priv->active != !active)
        g_signal_emit_by_name (action, "activate");
}


static void
moo_toggle_action_activate (MooAction *base)
{
    MooToggleAction *action = MOO_TOGGLE_ACTION (base);

    action->priv->active = !action->priv->active;
    g_object_notify (G_OBJECT (action), "active");
    g_signal_emit (action, toggle_signals[TOGGLED_TOGGLED], 0);
}


static void
moo_toggle_action_toggled (MooToggleAction *gtkaction)
{
    MooToggleAction *action = MOO_TOGGLE_ACTION (gtkaction);

    if (action->priv->callback)
    {
        if (MOO_OBJECT_PTR_GET (action->priv->ptr))
        {
            GObject *obj = MOO_OBJECT_PTR_GET (action->priv->ptr);
            g_object_ref (obj);
            action->priv->callback (obj, moo_toggle_action_get_active (gtkaction));
            g_object_unref (obj);
        }
        else
        {
            action->priv->callback (action->priv->data, moo_toggle_action_get_active (gtkaction));
        }
    }
}


static void
_moo_toggle_action_set_callback (MooToggleAction    *action,
                                 MooToggleActionCallback callback,
                                 gpointer            data,
                                 gboolean            object)
{
    g_return_if_fail (MOO_IS_TOGGLE_ACTION (action));
    g_return_if_fail (!object || G_IS_OBJECT (data));

    action->priv->callback = callback;
    if (action->priv->ptr)
        _moo_object_ptr_free (action->priv->ptr);
    action->priv->ptr = NULL;
    action->priv->data = NULL;

    if (callback)
    {
        if (object)
            action->priv->ptr = _moo_object_ptr_new (G_OBJECT (data), NULL, NULL);
        else
            action->priv->data = data;
    }
}


static GObject *
moo_toggle_action_constructor (GType                  type,
                               guint                  n_props,
                               GObjectConstructParam *props)
{
    guint i;
    GObject *object;
    MooToggleAction *action;
    MooToggleActionCallback toggled_callback = NULL;
    gpointer toggled_data = NULL;
    gpointer toggled_object = NULL;

    for (i = 0; i < n_props; ++i)
    {
        const char *name = props[i].pspec->name;
        GValue *value = props[i].value;

        if (!strcmp (name, "toggled-callback"))
            toggled_callback = (MooToggleActionCallback) g_value_get_pointer (value);
        else if (!strcmp (name, "toggled-data"))
            toggled_data = g_value_get_pointer (value);
        else if (!strcmp (name, "toggled-object"))
            toggled_object = g_value_get_object (value);
    }

    object = G_OBJECT_CLASS(moo_toggle_action_parent_class)->constructor (type, n_props, props);
    action = MOO_TOGGLE_ACTION (object);

    if (toggled_callback)
    {
        if (toggled_object)
            _moo_toggle_action_set_callback (action, toggled_callback, toggled_object, TRUE);
        else
            _moo_toggle_action_set_callback (action, toggled_callback, toggled_data, FALSE);
    }

    return object;
}


static void
moo_toggle_action_class_init (MooToggleActionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MooToggleActionClass *toggle_action_class = MOO_TOGGLE_ACTION_CLASS (klass);

    object_class->set_property = moo_toggle_action_set_property;
    object_class->get_property = moo_toggle_action_get_property;
    object_class->dispose = moo_toggle_action_dispose;
    object_class->constructor = moo_toggle_action_constructor;
    MOO_ACTION_CLASS (klass)->activate = moo_toggle_action_activate;
    toggle_action_class->toggled = moo_toggle_action_toggled;

    toggle_signals[TOGGLED_TOGGLED] =
        g_signal_new ("toggled", G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MooToggleActionClass, toggled),
                      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_object_class_install_property (object_class, TOGGLE_ACTION_PROP_ACTIVE,
                                     g_param_spec_boolean ("active", "active", "active",
                                                           FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY)));

    g_object_class_install_property (object_class, TOGGLE_ACTION_PROP_TOGGLED_CALLBACK,
                                     g_param_spec_pointer ("toggled-callback", "toggled-callback", "toggled-callback",
                                                           (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (object_class, TOGGLE_ACTION_PROP_TOGGLED_OBJECT,
                                     g_param_spec_object ("toggled-object", "toggled-object", "toggled-object",
                                                          G_TYPE_OBJECT, (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (object_class, TOGGLE_ACTION_PROP_TOGGLED_DATA,
                                     g_param_spec_pointer ("toggled-data", "toggled-data", "toggled-data",
                                                           (GParamFlags) (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)));
}


static void
moo_toggle_action_set_property (GObject            *object,
                                guint               property_id,
                                const GValue       *value,
                                GParamSpec         *pspec)
{
    switch (property_id)
    {
        case TOGGLE_ACTION_PROP_ACTIVE:
            moo_toggle_action_set_active (MOO_TOGGLE_ACTION (object), g_value_get_boolean (value));
            break;

        case TOGGLE_ACTION_PROP_TOGGLED_CALLBACK:
        case TOGGLE_ACTION_PROP_TOGGLED_OBJECT:
        case TOGGLE_ACTION_PROP_TOGGLED_DATA:
            /* these are handled in the constructor */
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


static void
moo_toggle_action_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    switch (property_id)
    {
        case TOGGLE_ACTION_PROP_ACTIVE:
            g_value_set_boolean (value, MOO_TOGGLE_ACTION (object)->priv->active);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


/**************************************************************************/
/* _moo_sync_toggle_action
 */

typedef struct {
    MooObjectWatch parent;
    GParamSpec *pspec;
    gboolean invert;
} ToggleWatch;

static void action_toggled          (ToggleWatch    *watch);
static void prop_changed            (ToggleWatch    *watch);
static void toggle_watch_destroy    (MooObjectWatch *watch);

static MooObjectWatchClass ToggleWatchClass = {NULL, NULL, toggle_watch_destroy};


static ToggleWatch *
toggle_watch_new (GObject    *master,
                  const char *prop,
                  MooAction  *action,
                  gboolean    invert)
{
    ToggleWatch *watch;
    GObjectClass *klass;
    GParamSpec *pspec;
    char *signal;

    g_return_val_if_fail (G_IS_OBJECT (master), NULL);
    g_return_val_if_fail (MOO_IS_TOGGLE_ACTION (action), NULL);
    g_return_val_if_fail (prop != NULL, NULL);

    klass = G_OBJECT_CLASS (g_type_class_peek (G_OBJECT_TYPE (master)));
    pspec = g_object_class_find_property (klass, prop);

    if (!pspec)
    {
        g_warning ("no property '%s' in class '%s'",
                   prop, g_type_name (G_OBJECT_TYPE (master)));
        return NULL;
    }

    watch = _moo_object_watch_new (ToggleWatch, &ToggleWatchClass,
                                   master, action, NULL, NULL);

    watch->pspec = pspec;
    watch->invert = invert;

    signal = g_strdup_printf ("notify::%s", prop);

    g_signal_connect_swapped (master, signal,
                              G_CALLBACK (prop_changed),
                              watch);
    g_signal_connect_swapped (action, "toggled",
                              G_CALLBACK (action_toggled),
                              watch);

    g_free (signal);
    return watch;
}


static void
toggle_watch_destroy (MooObjectWatch *watch)
{
    if (MOO_OBJECT_PTR_GET (watch->source))
    {
        g_signal_handlers_disconnect_by_func (MOO_OBJECT_PTR_GET (watch->source),
                                              (gpointer) prop_changed,
                                              watch);
    }

    if (MOO_OBJECT_PTR_GET (watch->target))
    {
        g_assert (MOO_IS_TOGGLE_ACTION (MOO_OBJECT_PTR_GET (watch->target)));
        g_signal_handlers_disconnect_by_func (MOO_OBJECT_PTR_GET (watch->target),
                                              (gpointer) action_toggled,
                                              watch);
    }
}


void
_moo_sync_toggle_action (MooAction  *action,
                         gpointer    master,
                         const char *prop,
                         gboolean    invert)
{
    ToggleWatch *watch;

    g_return_if_fail (MOO_IS_TOGGLE_ACTION (action));
    g_return_if_fail (G_IS_OBJECT (master));
    g_return_if_fail (prop != NULL);

    watch = toggle_watch_new (G_OBJECT (master), prop, action, invert);
    g_return_if_fail (watch != NULL);

    prop_changed (watch);
}


static void
prop_changed (ToggleWatch *watch)
{
    gboolean value, active, equal;
    gpointer action;

    g_object_get (MOO_OBJECT_PTR_GET (watch->parent.source),
                  watch->pspec->name, &value, NULL);

    action = MOO_OBJECT_PTR_GET (watch->parent.target);
    g_assert (MOO_IS_TOGGLE_ACTION (action));
    active = moo_toggle_action_get_active (MOO_TOGGLE_ACTION (action));

    if (!watch->invert)
        equal = !value == !active;
    else
        equal = !value != !active;

    if (!equal)
        moo_toggle_action_set_active (MOO_TOGGLE_ACTION (action), watch->invert ? !value : value);
}


static void
action_toggled (ToggleWatch *watch)
{
    gboolean value, active, equal;
    gpointer action;

    g_object_get (MOO_OBJECT_PTR_GET (watch->parent.source),
                  watch->pspec->name, &value, NULL);

    action = MOO_OBJECT_PTR_GET (watch->parent.target);
    g_assert (MOO_IS_TOGGLE_ACTION (action));
    active = moo_toggle_action_get_active (MOO_TOGGLE_ACTION (action));

    if (!watch->invert)
        equal = !value == !active;
    else
        equal = !value != !active;

    if (!equal)
        g_object_set (MOO_OBJECT_PTR_GET (watch->parent.source),
                      watch->pspec->name,
                      watch->invert ? !active : active, NULL);
}



