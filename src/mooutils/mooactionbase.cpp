/*
 *   mooactionbase.c
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

#include "mooutils/mooactionbase-private.h"
#include "mooutils/mooaction-private.h"
#include "mooutils/mooactiongroup.h"
#include "mooutils/mooaccel.h"
#include "mooutils/mooutils-gobject.h"
#include "marshals.h"

/* This file is the GtkAction/GtkStock family. Those classes are deprecated
   since GTK+ 3.10 and have no replacement short of moving to GAction/GMenu and
   named icons, which is GTK+ 4 work, so they are used knowingly. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS


enum {
    MOO_ACTION_BASE_PROPS(MOO_ACTION_BASE)
};


static void
class_init (gpointer g_iface, G_GNUC_UNUSED gpointer data)
{
    g_object_interface_install_property (g_iface,
        g_param_spec_string ("display-name", "display-name", "display-name",
                             NULL, (GParamFlags) G_PARAM_READWRITE));
    g_object_interface_install_property (g_iface,
        g_param_spec_string ("default-accel", "default-accel", "default-accel",
                             NULL, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("connect-accel", "connect-accel", "connect-accel",
                              FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("no-accel", "no-accel", "no-accel",
                              FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("force-accel-label", "force-accel-label", "force-accel-label",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("accel-editable", "accel-editable", "accel-editable",
                              TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("dead", "dead", "dead",
                              FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("active", "active", "active",
                              TRUE, (GParamFlags) G_PARAM_WRITABLE));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("has-submenu", "has-submenu", "has-submenu",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_interface_install_property (g_iface,
        g_param_spec_boolean ("use-underline", "use-underline", "use-underline",
                              TRUE, (GParamFlags) G_PARAM_READWRITE));
}


void
_moo_action_base_init_class (GObjectClass *klass)
{
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_DISPLAY_NAME,
        g_param_spec_string ("display-name", "display-name", "display-name",
                             NULL, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_DEFAULT_ACCEL,
        g_param_spec_string ("default-accel", "default-accel", "default-accel",
                             NULL, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_CONNECT_ACCEL,
        g_param_spec_boolean ("connect-accel", "connect-accel", "connect-accel",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_NO_ACCEL,
        g_param_spec_boolean ("no-accel", "no-accel", "no-accel",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_ACCEL_EDITABLE,
        g_param_spec_boolean ("accel-editable", "accel-editable", "accel-editable",
                              TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_FORCE_ACCEL_LABEL,
        g_param_spec_boolean ("force-accel-label", "force-accel-label", "force-accel-label",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_DEAD,
        g_param_spec_boolean ("dead", "dead", "dead",
                              FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_ACTIVE,
        g_param_spec_boolean ("active", "active", "active",
                              TRUE, G_PARAM_WRITABLE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_HAS_SUBMENU,
        g_param_spec_boolean ("has-submenu", "has-submenu", "has-submenu",
                              FALSE, (GParamFlags) G_PARAM_READWRITE));
    g_object_class_install_property (klass, MOO_ACTION_BASE_PROP_USE_UNDERLINE,
        g_param_spec_boolean ("use-underline", "use-underline", "use-underline",
                              TRUE, (GParamFlags) G_PARAM_READWRITE));

    g_object_class_override_property (klass,
                                      MOO_ACTION_BASE_PROP_LABEL,
                                      "label");
    g_object_class_override_property (klass,
                                      MOO_ACTION_BASE_PROP_TOOLTIP,
                                      "tooltip");
}


GType
moo_action_base_get_type (void)
{
    static GType type;

    if (G_UNLIKELY (!type))
    {
        static const GTypeInfo info = {
            sizeof (MooActionBaseClass), /* class_size */
            NULL, /* base_init */
            NULL, /* base_finalize */
            (GClassInitFunc) class_init,
            NULL, /* class_finalize */
            NULL, /* class_data */
            0,
            0, /* n_preallocs */
            NULL,
            NULL
        };

        type = g_type_register_static (G_TYPE_INTERFACE,
                                       "MooActionBase",
                                       &info, (GTypeFlags) 0);
        g_type_interface_add_prerequisite (type, GTK_TYPE_ACTION);
    }

    return type;
}


static void
set_string (gpointer    object,
            const char *id,
            const char *data)
{
    g_object_set_data_full (G_OBJECT (object), id, g_strdup (data), g_free);
}

static const char *
get_string (gpointer    object,
            const char *id)
{
    return (const char*) g_object_get_data (G_OBJECT (object), id);
}


static void
set_bool (gpointer    object,
          const char *id,
          gboolean    value)
{
    g_object_set_data (G_OBJECT (object), id, GINT_TO_POINTER (value));
}

static gboolean
get_bool (gpointer    object,
          const char *id)
{
    return g_object_get_data (G_OBJECT (object), id) != NULL;
}


static void
moo_action_base_set_display_name (MooActionBase *ab,
                                  const char    *name)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));
    g_return_if_fail (name != NULL);

    set_string (ab, "moo-action-display-name", name);
    g_object_notify (G_OBJECT (ab), "display-name");
}

const char *
_moo_action_get_display_name (gpointer action)
{
    const char *display_name;

    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), NULL);

    display_name = get_string (action, "moo-action-display-name");

    if (!display_name)
        display_name = moo_action_get_name (GTK_ACTION (action));

    return display_name;
}


static void
moo_action_base_set_default_accel (MooActionBase *ab,
                                   const char    *accel)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));

    if (accel && !accel[0])
        accel = NULL;

    set_string (ab, "moo-action-default-accel", accel);
    g_object_notify (G_OBJECT (ab), "default-accel");
}

static const char *
moo_action_base_get_default_accel (MooActionBase *ab)
{
    const char *accel;

    g_return_val_if_fail (MOO_IS_ACTION_BASE (ab), "");

    accel = get_string (ab, "moo-action-default-accel");

    if (!accel)
        accel = "";

    return accel;
}


void
_moo_action_set_no_accel (gpointer action,
                          gboolean no_accel)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    set_bool (action, "moo-action-no-accel", no_accel);
    g_object_notify (G_OBJECT (action), "no-accel");
}

gboolean
_moo_action_get_no_accel (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-no-accel");
}


static void
moo_action_base_set_connect_accel (gpointer action,
                                   gboolean connect)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    set_bool (action, "moo-action-connect-accel", connect);
    g_object_notify (G_OBJECT (action), "connect-accel");
}

gboolean
_moo_action_get_connect_accel (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-connect-accel");
}


static void
moo_action_base_set_accel_editable (gpointer action,
                                    gboolean editable)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    set_bool (action, "moo-action-accel-editable", editable);
    g_object_notify (G_OBJECT (action), "accel-editable");
}

gboolean
_moo_action_get_accel_editable (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-accel-editable");
}


static void
moo_action_base_set_force_accel_label (MooActionBase *ab,
                                       gboolean       force)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));
    set_bool (ab, "moo-action-force-accel-label", force);
    g_object_notify (G_OBJECT (ab), "force-accel-label");
}

static gboolean
moo_action_base_get_force_accel_label (MooActionBase *ab)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (ab), FALSE);
    return get_bool (ab, "moo-action-force-accel-label");
}


static void
moo_action_base_set_dead (MooActionBase *ab, gboolean dead)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));
    set_bool (ab, "moo-action-dead", dead);
    g_object_notify (G_OBJECT (ab), "dead");
}

gboolean
_moo_action_get_dead (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-dead");
}


static void
moo_action_base_set_active (MooActionBase *ab, gboolean active)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));
    g_object_set (ab, "visible", active, "sensitive", active, NULL);
}


static void
moo_action_base_set_has_submenu (MooActionBase *ab, gboolean has_submenu)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));
    set_bool (ab, "moo-action-has-submenu", has_submenu);
    g_object_notify (G_OBJECT (ab), "has-submenu");
}

gboolean
_moo_action_get_has_submenu (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-has-submenu");
}


static void
moo_action_base_set_use_underline (gpointer action,
                                   gboolean use_underline)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    set_bool (action, "moo-action-use-underline", use_underline);
    moo_action_sync_proxies (GTK_ACTION (action));
    g_object_notify (G_OBJECT (action), "use-underline");
}

static gboolean
moo_action_base_get_use_underline (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), FALSE);
    return get_bool (action, "moo-action-use-underline");
}


static void
moo_action_base_set_label (MooActionBase *ab,
                           const char    *label)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));

    g_object_set (G_OBJECT (ab), "GtkAction::label", label, NULL);
}


static void
moo_action_base_set_tooltip (MooActionBase *ab,
                             const char    *tooltip)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (ab));

    g_object_set (G_OBJECT (ab), "GtkAction::tooltip", tooltip, NULL);
}


void
_moo_action_base_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    MooActionBase *ab = MOO_ACTION_BASE (object);

    switch (property_id)
    {
        case MOO_ACTION_BASE_PROP_DISPLAY_NAME:
            moo_action_base_set_display_name (ab, g_value_get_string (value));
            break;
        case MOO_ACTION_BASE_PROP_DEFAULT_ACCEL:
            moo_action_base_set_default_accel (ab, g_value_get_string (value));
            break;
        case MOO_ACTION_BASE_PROP_CONNECT_ACCEL:
            moo_action_base_set_connect_accel (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_NO_ACCEL:
            _moo_action_set_no_accel (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_ACCEL_EDITABLE:
            moo_action_base_set_accel_editable (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_FORCE_ACCEL_LABEL:
            moo_action_base_set_force_accel_label (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_DEAD:
            moo_action_base_set_dead (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_ACTIVE:
            moo_action_base_set_active (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_HAS_SUBMENU:
            moo_action_base_set_has_submenu (ab, g_value_get_boolean (value));
            break;
        case MOO_ACTION_BASE_PROP_LABEL:
            moo_action_base_set_label (ab, g_value_get_string (value));
            break;
        case MOO_ACTION_BASE_PROP_TOOLTIP:
            moo_action_base_set_tooltip (ab, g_value_get_string (value));
            break;
        case MOO_ACTION_BASE_PROP_USE_UNDERLINE:
            moo_action_base_set_use_underline (ab, g_value_get_boolean (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


void
_moo_action_base_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    MooActionBase *ab = MOO_ACTION_BASE (object);

    switch (property_id)
    {
        case MOO_ACTION_BASE_PROP_DISPLAY_NAME:
            g_value_set_string (value, _moo_action_get_display_name (ab));
            break;
        case MOO_ACTION_BASE_PROP_DEFAULT_ACCEL:
            g_value_set_string (value, moo_action_base_get_default_accel (ab));
            break;
        case MOO_ACTION_BASE_PROP_CONNECT_ACCEL:
            g_value_set_boolean (value, _moo_action_get_connect_accel (ab));
            break;
        case MOO_ACTION_BASE_PROP_NO_ACCEL:
            g_value_set_boolean (value, _moo_action_get_no_accel (ab));
            break;
        case MOO_ACTION_BASE_PROP_ACCEL_EDITABLE:
            g_value_set_boolean (value, _moo_action_get_accel_editable (ab));
            break;
        case MOO_ACTION_BASE_PROP_FORCE_ACCEL_LABEL:
            g_value_set_boolean (value, moo_action_base_get_force_accel_label (ab));
            break;
        case MOO_ACTION_BASE_PROP_DEAD:
            g_value_set_boolean (value, _moo_action_get_dead (ab));
            break;
        case MOO_ACTION_BASE_PROP_HAS_SUBMENU:
            g_value_set_boolean (value, _moo_action_get_has_submenu (ab));
            break;
        case MOO_ACTION_BASE_PROP_USE_UNDERLINE:
            g_value_set_boolean (value, moo_action_base_get_use_underline (ab));
            break;

        case MOO_ACTION_BASE_PROP_LABEL:
            g_object_get_property (object, "GtkAction::label", value);
            break;
        case MOO_ACTION_BASE_PROP_TOOLTIP:
            g_object_get_property (object, "GtkAction::tooltip", value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}


char *
_moo_action_make_accel_path (gpointer action)
{
    MooActionGroup *group = NULL;
    MooActionCollection *collection;
    const char *name, *group_name, *collection_name;

    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), NULL);

    group = _moo_action_get_group (action);
    g_return_val_if_fail (MOO_IS_ACTION_GROUP (group), NULL);
    collection = _moo_action_group_get_collection (group);
    g_return_val_if_fail (MOO_IS_ACTION_COLLECTION (collection), NULL);

    name = moo_action_get_name (GTK_ACTION (action));
    group_name = moo_action_group_get_name (group);
    collection_name = moo_action_collection_get_name (collection);

    g_return_val_if_fail (collection_name != NULL, NULL);
    g_return_val_if_fail (name != NULL && name[0] != 0, NULL);

    if (group_name)
        return g_strdup_printf ("<MooAction>/%s/%s/%s", collection_name, group_name, name);
    else
        return g_strdup_printf ("<MooAction>/%s/%s", collection_name, name);
}


void
_moo_action_set_accel_path (gpointer    action,
                            const char *accel_path)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    moo_action_set_accel_path (GTK_ACTION (action), accel_path);
}


const char *
_moo_action_get_accel_path (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), NULL);
    return moo_action_get_accel_path (GTK_ACTION (action));
}


const char *
moo_action_get_name (GtkAction *action)
{
    return gtk_action_get_name (action);
}

gboolean
moo_action_get_sensitive (GtkAction *action)
{
    return gtk_action_get_sensitive (action);
}

void
moo_action_set_sensitive (GtkAction *action,
                          gboolean   sensitive)
{
    gtk_action_set_sensitive (action, sensitive);
}

gboolean
moo_action_get_visible (GtkAction *action)
{
    return gtk_action_get_visible (action);
}

gboolean
moo_action_is_visible (GtkAction *action)
{
    return gtk_action_is_visible (action);
}

void
moo_action_set_visible (GtkAction *action,
                        gboolean   visible)
{
    gtk_action_set_visible (action, visible);
}

void
moo_action_activate (GtkAction *action)
{
    gtk_action_activate (action);
}

const char *
moo_action_get_accel_path (GtkAction *action)
{
    return gtk_action_get_accel_path (action);
}

void
moo_action_set_accel_path (GtkAction  *action,
                           const char *accel_path)
{
    gtk_action_set_accel_path (action, accel_path);
}

gboolean
moo_toggle_action_get_active (GtkToggleAction *action)
{
    return gtk_toggle_action_get_active (action);
}

void
moo_toggle_action_set_active (GtkToggleAction *action,
                              gboolean         active)
{
    gtk_toggle_action_set_active (action, active);
}


const char *
_moo_action_get_default_accel (gpointer action)
{
    g_return_val_if_fail (MOO_IS_ACTION_BASE (action), "");
    return moo_action_base_get_default_accel (MOO_ACTION_BASE (action));
}



/* A proxy is a menu item or a tool button that mirrors an action. Its link holds
   a reference to the action and the handlers on both sides, and goes away with the
   proxy. The links of an action are kept on it as qdata, as the list GtkAction
   kept of its proxies. */
typedef struct {
    GtkAction *action;
    GtkWidget *proxy;
    gulong     activate_id;
    gboolean   toggle;
} ProxyLink;

static const char PROXIES_KEY[] = "moo-action-proxies";

static void
sync_link (ProxyLink *link)
{
    GtkAction *action = link->action;
    GtkWidget *proxy = link->proxy;
    g_autofree char *label = NULL;
    g_autofree char *tooltip = NULL;
    g_autofree char *icon_name = NULL;

    g_object_get (action, "label", &label, "tooltip", &tooltip,
                  "icon-name", &icon_name, NULL);

    gboolean use_underline = moo_action_base_get_use_underline (action);

    gtk_widget_set_sensitive (proxy, gtk_action_is_sensitive (action));
    gtk_widget_set_visible (proxy, gtk_action_is_visible (action));

    if (GTK_IS_MENU_ITEM (proxy))
    {
        GtkMenuItem *item = GTK_MENU_ITEM (proxy);
        const char *accel_path = moo_action_get_accel_path (action);

        gtk_menu_item_set_label (item, label ? label : "");
        gtk_menu_item_set_use_underline (item, use_underline);

        if (accel_path)
            gtk_menu_item_set_accel_path (item, accel_path);
    }
    else
    {
        GtkToolButton *button = GTK_TOOL_BUTTON (proxy);

        gtk_tool_button_set_label (button, label);
        gtk_tool_button_set_use_underline (button, use_underline);
        gtk_tool_button_set_icon_name (button, icon_name);
        gtk_tool_item_set_tooltip_text (GTK_TOOL_ITEM (proxy), tooltip);
    }

    if (link->toggle)
    {
        gboolean active = moo_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

        /* setting the state activates the proxy, which would toggle the action back */
        g_signal_handler_block (proxy, link->activate_id);

        if (GTK_IS_MENU_ITEM (proxy))
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (proxy), active);
        else
            gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (proxy), active);

        g_signal_handler_unblock (proxy, link->activate_id);
    }
}

static void
on_action_notify (G_GNUC_UNUSED GObject *action,
                  G_GNUC_UNUSED GParamSpec *pspec,
                  ProxyLink *link)
{
    sync_link (link);
}

static void
on_action_toggled (G_GNUC_UNUSED GtkAction *action,
                   ProxyLink *link)
{
    sync_link (link);
}

static void
on_proxy_activate (G_GNUC_UNUSED GtkWidget *proxy,
                   ProxyLink *link)
{
    moo_action_activate (link->action);
}

static void
on_proxy_destroy (GtkWidget *proxy,
                  ProxyLink *link)
{
    GtkAction *action = link->action;

    g_signal_handlers_disconnect_by_data (proxy, link);
    g_signal_handlers_disconnect_by_data (action, link);
    g_object_set_data (G_OBJECT (action), PROXIES_KEY,
                       g_slist_remove ((GSList*) g_object_get_data (G_OBJECT (action), PROXIES_KEY), link));
    g_free (link);
    g_object_unref (action);
}

static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
    ProxyLink *link = g_new0 (ProxyLink, 1);

    link->action = (GtkAction*) g_object_ref (action);
    link->proxy = proxy;
    link->toggle = GTK_IS_TOGGLE_ACTION (action) &&
                   (GTK_IS_CHECK_MENU_ITEM (proxy) || GTK_IS_TOGGLE_TOOL_BUTTON (proxy));
    link->activate_id = g_signal_connect (proxy, GTK_IS_MENU_ITEM (proxy) ? "activate" : "clicked",
                                          G_CALLBACK (on_proxy_activate), link);
    g_signal_connect (proxy, "destroy", G_CALLBACK (on_proxy_destroy), link);

    g_signal_connect (action, "notify::label", G_CALLBACK (on_action_notify), link);
    g_signal_connect (action, "notify::tooltip", G_CALLBACK (on_action_notify), link);
    g_signal_connect (action, "notify::icon-name", G_CALLBACK (on_action_notify), link);
    g_signal_connect (action, "notify::sensitive", G_CALLBACK (on_action_notify), link);
    g_signal_connect (action, "notify::visible", G_CALLBACK (on_action_notify), link);

    if (link->toggle)
        g_signal_connect (action, "toggled", G_CALLBACK (on_action_toggled), link);

    g_object_set_data (G_OBJECT (action), PROXIES_KEY,
                       g_slist_prepend ((GSList*) g_object_get_data (G_OBJECT (action), PROXIES_KEY), link));
    sync_link (link);
}

void
moo_action_sync_proxies (GtkAction *action)
{
    g_return_if_fail (GTK_IS_ACTION (action));

    GSList *links = g_slist_copy ((GSList*) g_object_get_data (G_OBJECT (action), PROXIES_KEY));

    for (GSList *l = links; l != NULL; l = l->next)
        sync_link ((ProxyLink*) l->data);

    g_slist_free (links);
}

GtkWidget *
moo_action_create_default_menu_item (GtkAction *action)
{
    g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

    return GTK_IS_TOGGLE_ACTION (action) ? gtk_check_menu_item_new ()
                                         : gtk_menu_item_new ();
}

GtkWidget *
moo_action_create_menu_item (GtkAction *action)
{
    g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

    GtkWidget *item;

    if (MOO_IS_ACTION (action) && MOO_ACTION_GET_CLASS (action)->create_menu_item)
        item = MOO_ACTION_GET_CLASS (action)->create_menu_item (action);
    else
        item = moo_action_create_default_menu_item (action);

    if (item)
        connect_proxy (action, item);

    return item;
}

GtkWidget *
moo_action_create_tool_item (GtkAction *action)
{
    g_return_val_if_fail (GTK_IS_ACTION (action), NULL);

    GtkWidget *item;

    if (_moo_action_get_has_submenu (action))
        item = GTK_WIDGET (gtk_menu_tool_button_new (NULL, NULL));
    else if (GTK_IS_TOGGLE_ACTION (action))
        item = GTK_WIDGET (gtk_toggle_tool_button_new ());
    else
        item = GTK_WIDGET (gtk_tool_button_new (NULL, NULL));

    connect_proxy (action, item);
    return item;
}


void
_moo_action_base_init_instance (gpointer action)
{
    g_return_if_fail (MOO_IS_ACTION_BASE (action));
    set_bool (action, "moo-action-use-underline", TRUE);
}

G_GNUC_END_IGNORE_DEPRECATIONS
