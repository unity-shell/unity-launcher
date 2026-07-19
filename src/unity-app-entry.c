#include "unity-app-entry.h"

#include <astal-wlr.h>

struct _UnityAppEntry
{
  GObject     parent_instance;

  gchar      *app_id;
  GAppInfo   *app_info;
  GListModel *toplevels;

  gboolean    pinned;
  gboolean    running;
  gboolean    activated;
};

/**
 * UnityAppEntry:
 *
 * One row in the launcher's app list.
 *
 * An entry carries the app identity, an optional #GAppInfo, and a live
 * #GListModel of the app's currently open toplevels, filtered from the toplevel
 * service by app-id. The derived #UnityAppEntry:running and
 * #UnityAppEntry:activated booleans are notifiable properties, so tiles react to
 * changes. UnityAppList produces these entries. Consumers do not construct them.
 */
G_DEFINE_FINAL_TYPE (UnityAppEntry, unity_app_entry, G_TYPE_OBJECT)

typedef enum
{
  PROP_APP_ID = 1,
  PROP_APP_INFO,
  PROP_TOPLEVELS,
  PROP_PINNED,
  PROP_RUNNING,
  PROP_ACTIVATED,
} UnityAppEntryProperty;
static GParamSpec *properties[PROP_ACTIVATED + 1];

static void
recompute_derived (UnityAppEntry *self)
{
  guint    n         = g_list_model_get_n_items (self->toplevels);
  gboolean running   = (n > 0);
  gboolean activated = FALSE;

  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (self->toplevels, i);
      if (astal_wlr_toplevel_get_activated (tl))
        {
          activated = TRUE;
          break;
        }
    }

  if (self->running != running)
    {
      self->running = running;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RUNNING]);
    }
  if (self->activated != activated)
    {
      self->activated = activated;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATED]);
    }
}

static void
on_toplevel_activated_changed (AstalWlrToplevel *toplevel, GParamSpec *pspec,
                               UnityAppEntry *self)
{
  (void) toplevel; (void) pspec;
  recompute_derived (self);
}

static void
attach_toplevel_listener (UnityAppEntry *self, AstalWlrToplevel *toplevel)
{
  g_signal_connect_object (toplevel, "notify::activated",
                           G_CALLBACK (on_toplevel_activated_changed), self, 0);
}

static void
on_toplevels_items_changed (GListModel *model, guint position, guint removed,
                            guint added, UnityAppEntry *self)
{
  (void) removed;

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (model, position + i);
      attach_toplevel_listener (self, tl);
    }
  recompute_derived (self);
}

static void
unity_app_entry_dispose (GObject *object)
{
  UnityAppEntry *self = UNITY_APP_ENTRY (object);

  g_clear_object (&self->toplevels);
  g_clear_object (&self->app_info);

  G_OBJECT_CLASS (unity_app_entry_parent_class)->dispose (object);
}

static void
unity_app_entry_finalize (GObject *object)
{
  UnityAppEntry *self = UNITY_APP_ENTRY (object);

  g_clear_pointer (&self->app_id, g_free);

  G_OBJECT_CLASS (unity_app_entry_parent_class)->finalize (object);
}

static void
unity_app_entry_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  UnityAppEntry *self = UNITY_APP_ENTRY (object);
  switch ((UnityAppEntryProperty) id)
    {
    case PROP_APP_ID:    g_value_set_string  (value, self->app_id);    break;
    case PROP_APP_INFO:  g_value_set_object  (value, self->app_info);  break;
    case PROP_TOPLEVELS: g_value_set_object  (value, self->toplevels); break;
    case PROP_PINNED:    g_value_set_boolean (value, self->pinned);    break;
    case PROP_RUNNING:   g_value_set_boolean (value, self->running);   break;
    case PROP_ACTIVATED: g_value_set_boolean (value, self->activated); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
unity_app_entry_class_init (UnityAppEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = unity_app_entry_dispose;
  object_class->finalize     = unity_app_entry_finalize;
  object_class->get_property = unity_app_entry_get_property;

  properties[PROP_APP_ID] = g_param_spec_string (
    "app-id", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_APP_INFO] = g_param_spec_object (
    "app-info", NULL, NULL, G_TYPE_APP_INFO, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_TOPLEVELS] = g_param_spec_object (
    "toplevels", NULL, NULL, G_TYPE_LIST_MODEL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_PINNED] = g_param_spec_boolean (
    "pinned", NULL, NULL, FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_RUNNING] = g_param_spec_boolean (
    "running", NULL, NULL, FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_ACTIVATED] = g_param_spec_boolean (
    "activated", NULL, NULL, FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
unity_app_entry_init (UnityAppEntry *self)
{
  (void) self;
}

UnityAppEntry *
_unity_app_entry_new (const gchar *app_id, GAppInfo *app_info, GListModel *toplevels)
{
  UnityAppEntry *self;

  g_return_val_if_fail (app_id != NULL, NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (toplevels), NULL);

  self = g_object_new (UNITY_TYPE_APP_ENTRY, NULL);
  self->app_id    = g_strdup (app_id);
  self->app_info  = app_info ? g_object_ref (app_info) : NULL;
  self->toplevels = g_object_ref (toplevels);

  guint n = g_list_model_get_n_items (toplevels);
  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (toplevels, i);
      attach_toplevel_listener (self, tl);
    }
  g_signal_connect_object (toplevels, "items-changed",
                           G_CALLBACK (on_toplevels_items_changed), self, 0);

  recompute_derived (self);
  return self;
}

void
_unity_app_entry_set_pinned (UnityAppEntry *self, gboolean pinned)
{
  g_return_if_fail (UNITY_IS_APP_ENTRY (self));
  pinned = !!pinned;
  if (self->pinned == pinned)
    return;
  self->pinned = pinned;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PINNED]);
}

/**
 * unity_app_entry_get_app_id:
 * @self: a #UnityAppEntry
 *
 * Gets the canonical .desktop id of the app.
 *
 * Returns: (transfer none): the app id
 */
const gchar *
unity_app_entry_get_app_id (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), NULL);
  return self->app_id;
}

/**
 * unity_app_entry_get_app_info:
 * @self: a #UnityAppEntry
 *
 * Gets the app's #GAppInfo, if one was resolved.
 *
 * Returns: (transfer none) (nullable): the app info, or %NULL for a running-only app
 */
GAppInfo *
unity_app_entry_get_app_info (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), NULL);
  return self->app_info;
}

/**
 * unity_app_entry_get_toplevels:
 * @self: a #UnityAppEntry
 *
 * Gets the live model of the app's open toplevels.
 *
 * Returns: (transfer none): a #GListModel of #AstalWlrToplevel
 */
GListModel *
unity_app_entry_get_toplevels (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), NULL);
  return self->toplevels;
}

/**
 * unity_app_entry_get_pinned:
 * @self: a #UnityAppEntry
 *
 * Gets whether the app is pinned to the launcher.
 *
 * Returns: %TRUE if the app is pinned
 */
gboolean
unity_app_entry_get_pinned (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), FALSE);
  return self->pinned;
}

/**
 * unity_app_entry_get_running:
 * @self: a #UnityAppEntry
 *
 * Gets whether the app has any open toplevels.
 *
 * Returns: %TRUE if at least one window is open
 */
gboolean
unity_app_entry_get_running (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), FALSE);
  return self->running;
}

/**
 * unity_app_entry_get_activated:
 * @self: a #UnityAppEntry
 *
 * Gets whether any of the app's toplevels is focused.
 *
 * Returns: %TRUE if a window is activated
 */
gboolean
unity_app_entry_get_activated (UnityAppEntry *self)
{
  g_return_val_if_fail (UNITY_IS_APP_ENTRY (self), FALSE);
  return self->activated;
}

/**
 * unity_app_entry_activate_or_launch:
 * @self: a #UnityAppEntry
 *
 * Applies the launcher's click semantics. With no open windows, the app is
 * launched through its #GAppInfo. With a focused window, that window is
 * minimized to hide it. Otherwise the first window is raised.
 */
void
unity_app_entry_activate_or_launch (UnityAppEntry *self)
{
  g_return_if_fail (UNITY_IS_APP_ENTRY (self));

  guint n = g_list_model_get_n_items (self->toplevels);
  if (n == 0)
    {
      if (self->app_info != NULL)
        {
          g_autoptr (GError) error = NULL;
          if (!g_app_info_launch (self->app_info, NULL, NULL, &error))
            g_warning ("UnityAppEntry: launch failed for %s: %s",
                       self->app_id ? self->app_id : "(null)",
                       error ? error->message : "unknown error");
        }
      return;
    }

  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (self->toplevels, i);
      if (astal_wlr_toplevel_get_activated (tl))
        {
          astal_wlr_toplevel_minimize (tl, TRUE);
          return;
        }
    }

  g_autoptr (AstalWlrToplevel) first = g_list_model_get_item (self->toplevels, 0);
  if (first != NULL)
    astal_wlr_toplevel_activate (first);
}

/**
 * unity_app_entry_close_all:
 * @self: a #UnityAppEntry
 *
 * Closes every open window of the app, the launcher's "Quit" action.
 */
void
unity_app_entry_close_all (UnityAppEntry *self)
{
  g_return_if_fail (UNITY_IS_APP_ENTRY (self));

  guint n = g_list_model_get_n_items (self->toplevels);
  g_autoptr (GPtrArray) snapshot = g_ptr_array_new_full (n, g_object_unref);
  for (guint i = 0; i < n; i++)
    g_ptr_array_add (snapshot, g_list_model_get_item (self->toplevels, i));

  for (guint i = 0; i < snapshot->len; i++)
    astal_wlr_toplevel_close (g_ptr_array_index (snapshot, i));
}
