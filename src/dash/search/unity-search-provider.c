#include "dash/search/unity-search-provider.h"

#include <gio/gdesktopappinfo.h>

#define SEARCH_PROVIDER_IFACE "org.gnome.Shell.SearchProvider2"
#define PROVIDER_GROUP        "Shell Search Provider"

struct _UnitySearchProvider
{
  GObject          parent_instance;

  gchar           *bus_name;
  gchar           *object_path;
  GDesktopAppInfo *app_info;
  GDBusProxy      *proxy;
};

/**
 * UnitySearchProvider:
 *
 * One installed SearchProvider2, such as Calculator, Files or Settings.
 *
 * A UnitySearchProvider wraps a GDBusProxy to the provider's bus name and
 * object path. It also holds the provider application's GDesktopAppInfo, used
 * for the section header shown above its results.
 */
G_DEFINE_FINAL_TYPE (UnitySearchProvider, unity_search_provider, G_TYPE_OBJECT)

static void
on_proxy_ready (GObject *source, GAsyncResult *res, gpointer user_data)
{
  (void) source;
  UnitySearchProvider *self = user_data;
  g_autoptr (GError) error = NULL;
  GDBusProxy *proxy = g_dbus_proxy_new_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("search provider %s: proxy failed: %s",
                 self->bus_name, error ? error->message : "?");
      g_object_unref (self);
      return;
    }
  self->proxy = proxy;
  g_object_unref (self);
}

static UnitySearchProvider *
provider_new (const gchar *bus_name, const gchar *object_path, const gchar *desktop_id)
{
  UnitySearchProvider *self = g_object_new (UNITY_TYPE_SEARCH_PROVIDER, NULL);
  self->bus_name    = g_strdup (bus_name);
  self->object_path = g_strdup (object_path);
  self->app_info    = g_desktop_app_info_new (desktop_id);

  g_dbus_proxy_new_for_bus (
    G_BUS_TYPE_SESSION,
    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
    NULL, bus_name, object_path, SEARCH_PROVIDER_IFACE,
    NULL, on_proxy_ready, g_object_ref (self));

  return self;
}

static void
discover_in_dir (const gchar *data_dir, GHashTable *seen, GList **out)
{
  g_autofree gchar *dir = g_build_filename (data_dir, "gnome-shell", "search-providers", NULL);
  g_autoptr (GDir) d = g_dir_open (dir, 0, NULL);
  if (d == NULL)
    return;

  const gchar *name;
  while ((name = g_dir_read_name (d)) != NULL)
    {
      if (!g_str_has_suffix (name, ".ini"))
        continue;
      if (!g_hash_table_add (seen, g_strdup (name)))
        continue;

      g_autofree gchar *path = g_build_filename (dir, name, NULL);
      g_autoptr (GKeyFile) kf = g_key_file_new ();
      if (!g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL))
        continue;
      if (!g_key_file_has_group (kf, PROVIDER_GROUP))
        continue;
      if (g_key_file_get_boolean (kf, PROVIDER_GROUP, "DefaultDisabled", NULL))
        continue;

      g_autofree gchar *version = g_key_file_get_string (kf, PROVIDER_GROUP, "Version", NULL);
      if (g_strcmp0 (version, "2") != 0)
        continue;

      g_autofree gchar *bus  = g_key_file_get_string (kf, PROVIDER_GROUP, "BusName", NULL);
      g_autofree gchar *obj  = g_key_file_get_string (kf, PROVIDER_GROUP, "ObjectPath", NULL);
      g_autofree gchar *desk = g_key_file_get_string (kf, PROVIDER_GROUP, "DesktopId", NULL);
      if (bus == NULL || obj == NULL || desk == NULL)
        continue;

      *out = g_list_prepend (*out, provider_new (bus, obj, desk));
    }
}

/**
 * unity_search_provider_discover:
 *
 * Discovers all Version=2 search providers installed on the system.
 *
 * The search-providers directories under the user data dir and $XDG_DATA_DIRS
 * are scanned. A provider in the user directory shadows a system provider with
 * the same filename.
 *
 * Returns: (transfer full) (element-type UnitySearchProvider): a list of
 *   providers. Unref each element and free the list.
 */
GList *
unity_search_provider_discover (void)
{
  GList *out = NULL;
  g_autoptr (GHashTable) seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  discover_in_dir (g_get_user_data_dir (), seen, &out);
  for (const gchar *const *p = g_get_system_data_dirs (); p && *p; p++)
    discover_in_dir (*p, seen, &out);

  return out;
}

/**
 * unity_search_provider_get_name:
 * @self: a UnitySearchProvider.
 *
 * Gets the display name of the provider application.
 *
 * Returns: (transfer none) (nullable): the display name, or %NULL.
 */
const gchar *
unity_search_provider_get_name (UnitySearchProvider *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_PROVIDER (self), NULL);
  return self->app_info ? g_app_info_get_display_name (G_APP_INFO (self->app_info)) : NULL;
}

/**
 * unity_search_provider_get_gicon:
 * @self: a UnitySearchProvider.
 *
 * Gets the icon of the provider application.
 *
 * Returns: (transfer none) (nullable): the icon, or %NULL.
 */
GIcon *
unity_search_provider_get_gicon (UnitySearchProvider *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_PROVIDER (self), NULL);
  return self->app_info ? g_app_info_get_icon (G_APP_INFO (self->app_info)) : NULL;
}

typedef struct
{
  GStrv terms;
  guint limit;
} QueryData;

static void
query_data_free (gpointer p)
{
  QueryData *q = p;
  g_strfreev (q->terms);
  g_free (q);
}

static GIcon *
gicon_from_meta (GVariant *icon, GVariant *gicon)
{
  if (gicon != NULL)
    return g_icon_new_for_string (g_variant_get_string (gicon, NULL), NULL);
  if (icon != NULL)
    return g_icon_deserialize (icon);
  return NULL;
}

static void
on_get_metas (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask                    *task = user_data;
  UnitySearchProvider *self = g_task_get_source_object (task);
  QueryData                *q    = g_task_get_task_data (task);
  g_autoptr (GError)        error = NULL;

  g_autoptr (GVariant) reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (reply == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  GPtrArray *results = g_ptr_array_new_with_free_func (g_object_unref);

  g_autoptr (GVariant) metas = g_variant_get_child_value (reply, 0);
  GVariantIter outer;
  g_variant_iter_init (&outer, metas);
  GVariant *meta;
  while ((meta = g_variant_iter_next_value (&outer)))
    {
      g_autoptr (GVariant) v_id    = g_variant_lookup_value (meta, "id",          NULL);
      g_autoptr (GVariant) v_name  = g_variant_lookup_value (meta, "name",        NULL);
      g_autoptr (GVariant) v_desc  = g_variant_lookup_value (meta, "description", NULL);
      g_autoptr (GVariant) v_icon  = g_variant_lookup_value (meta, "icon",        NULL);
      g_autoptr (GVariant) v_gicon = g_variant_lookup_value (meta, "gicon",       NULL);

      if (v_id != NULL && v_name != NULL)
        {
          g_autoptr (GIcon) gicon = gicon_from_meta (v_icon, v_gicon);
          g_ptr_array_add (results, unity_search_result_new (
            self,
            g_variant_get_string (v_id, NULL),
            g_variant_get_string (v_name, NULL),
            v_desc ? g_variant_get_string (v_desc, NULL) : NULL,
            gicon,
            (const gchar *const *) q->terms));
        }
      g_variant_unref (meta);
    }

  g_task_return_pointer (task, results, (GDestroyNotify) g_ptr_array_unref);
  g_object_unref (task);
}

static void
on_get_initial (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask                    *task = user_data;
  UnitySearchProvider *self = g_task_get_source_object (task);
  QueryData                *q    = g_task_get_task_data (task);
  g_autoptr (GError)        error = NULL;

  g_autoptr (GVariant) reply = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (reply == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  g_autoptr (GVariant) v_ids = g_variant_get_child_value (reply, 0);
  gsize n_ids = 0;
  const gchar **ids = g_variant_get_strv (v_ids, &n_ids);

  if (n_ids == 0)
    {
      g_task_return_pointer (task, g_ptr_array_new_with_free_func (g_object_unref),
                             (GDestroyNotify) g_ptr_array_unref);
      g_object_unref (task);
      g_free (ids);
      return;
    }

  GVariantBuilder capped;
  g_variant_builder_init (&capped, G_VARIANT_TYPE ("as"));
  for (gsize i = 0; i < n_ids && i < q->limit; i++)
    g_variant_builder_add (&capped, "s", ids[i]);
  g_free (ids);

  g_dbus_proxy_call (self->proxy, "GetResultMetas",
                     g_variant_new ("(as)", &capped),
                     G_DBUS_CALL_FLAGS_NONE, -1,
                     g_task_get_cancellable (task), on_get_metas, task);
}

/**
 * unity_search_provider_query_async:
 * @self: a UnitySearchProvider.
 * @terms: the search terms.
 * @limit: the maximum number of results to return.
 * @cancellable: (nullable): a GCancellable, or %NULL.
 * @callback: the callback to invoke when the query completes.
 * @user_data: data to pass to @callback.
 *
 * Queries the provider asynchronously.
 *
 * The call chains GetInitialResultSet into GetResultMetas over D-Bus. When the
 * proxy is not connected yet the query completes with an empty result set and
 * no error.
 */
void
unity_search_provider_query_async (UnitySearchProvider *self,
                                         const gchar *const       *terms,
                                         guint                     limit,
                                         GCancellable             *cancellable,
                                         GAsyncReadyCallback       callback,
                                         gpointer                  user_data)
{
  g_return_if_fail (UNITY_IS_SEARCH_PROVIDER (self));

  GTask *task = g_task_new (self, cancellable, callback, user_data);
  QueryData *q = g_new0 (QueryData, 1);
  q->terms = g_strdupv ((gchar **) terms);
  q->limit = limit;
  g_task_set_task_data (task, q, query_data_free);

  if (self->proxy == NULL)
    {
      g_task_return_pointer (task, g_ptr_array_new_with_free_func (g_object_unref),
                             (GDestroyNotify) g_ptr_array_unref);
      g_object_unref (task);
      return;
    }

  GVariantBuilder tb;
  g_variant_builder_init (&tb, G_VARIANT_TYPE ("as"));
  for (const gchar *const *t = terms; t && *t; t++)
    g_variant_builder_add (&tb, "s", *t);

  g_dbus_proxy_call (self->proxy, "GetInitialResultSet",
                     g_variant_new ("(as)", &tb),
                     G_DBUS_CALL_FLAGS_NONE, -1,
                     cancellable, on_get_initial, task);
}

/**
 * unity_search_provider_query_finish:
 * @self: a UnitySearchProvider.
 * @result: the GAsyncResult passed to the callback.
 * @error: (nullable): return location for an error, or %NULL.
 *
 * Finishes an asynchronous query started with
 * unity_search_provider_query_async().
 *
 * Returns: (transfer full) (element-type UnitySearchResult): the results, which
 *   may be empty, or %NULL on error.
 */
GPtrArray *
unity_search_provider_query_finish (UnitySearchProvider *self,
                                          GAsyncResult *result, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);
  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * unity_search_provider_activate_result:
 * @self: a UnitySearchProvider.
 * @id: the identifier of the result to activate.
 * @terms: the terms that produced the result.
 * @timestamp: an activation timestamp.
 *
 * Activates a result by calling ActivateResult on the provider.
 */
void
unity_search_provider_activate_result (UnitySearchProvider *self,
                                             const gchar              *id,
                                             const gchar *const       *terms,
                                             guint32                   timestamp)
{
  g_return_if_fail (UNITY_IS_SEARCH_PROVIDER (self));
  if (self->proxy == NULL)
    return;

  GVariantBuilder tb;
  g_variant_builder_init (&tb, G_VARIANT_TYPE ("as"));
  for (const gchar *const *t = terms; t && *t; t++)
    g_variant_builder_add (&tb, "s", *t);

  g_dbus_proxy_call (self->proxy, "ActivateResult",
                     g_variant_new ("(sasu)", id, &tb, timestamp),
                     G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
unity_search_provider_dispose (GObject *object)
{
  UnitySearchProvider *self = UNITY_SEARCH_PROVIDER (object);
  g_clear_object (&self->proxy);
  g_clear_object (&self->app_info);
  G_OBJECT_CLASS (unity_search_provider_parent_class)->dispose (object);
}

static void
unity_search_provider_finalize (GObject *object)
{
  UnitySearchProvider *self = UNITY_SEARCH_PROVIDER (object);
  g_free (self->bus_name);
  g_free (self->object_path);
  G_OBJECT_CLASS (unity_search_provider_parent_class)->finalize (object);
}

static void
unity_search_provider_class_init (UnitySearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose  = unity_search_provider_dispose;
  object_class->finalize = unity_search_provider_finalize;
}

static void
unity_search_provider_init (UnitySearchProvider *self)
{
  (void) self;
}
