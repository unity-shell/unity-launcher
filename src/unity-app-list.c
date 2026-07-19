#include "unity-app-list.h"

#include <string.h>

#include <astal-apps.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#include <astal-wlr.h>

struct _UnityAppList
{
  GObject         parent_instance;

  AstalAppsApps  *catalog;
  GListModel     *toplevels;
  GStrv           pinned;

  GPtrArray      *entries;
  GHashTable     *cache;
  GHashTable     *id_canonical;
};

static void list_model_iface_init (GListModelInterface *iface);

/**
 * UnityAppList:
 *
 * A #GListModel of #UnityAppEntry merging pinned apps with running windows.
 *
 * The list combines the pinned .desktop ids with the running windows from the
 * toplevel service into one deduped, ordered model. Pinned apps come first, in
 * pin order, then running but unpinned apps in compositor order. Each entry's
 * toplevels are filtered live by app-id, backed by a #GtkFilterListModel over
 * the shared toplevel service.
 *
 * A raw app-id (a pinned .desktop id, or a compositor app-id or wm class from a
 * window) is resolved to a canonical .desktop id through AstalApps, so "Code",
 * "code.desktop" and a StartupWMClass all collapse to one entry. Resolutions are
 * memoised and dropped when AstalApps reloads its catalog. Each canonical id
 * maps to one cached #UnityAppEntry, kept alive across rebuilds so pointer
 * identity stays stable for the diff.
 */
G_DEFINE_FINAL_TYPE_WITH_CODE (UnityAppList, unity_app_list, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GType    list_model_get_item_type (GListModel *m) { (void) m; return UNITY_TYPE_APP_ENTRY; }
static guint    list_model_get_n_items   (GListModel *m) { return UNITY_APP_LIST (m)->entries->len; }
static gpointer list_model_get_item      (GListModel *m, guint pos)
{
  UnityAppList *self = UNITY_APP_LIST (m);
  if (pos >= self->entries->len)
    return NULL;
  return g_object_ref (g_ptr_array_index (self->entries, pos));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = list_model_get_item_type;
  iface->get_n_items   = list_model_get_n_items;
  iface->get_item      = list_model_get_item;
}

static gchar *
canonicalize_token (const gchar *token)
{
  gchar *out = g_ascii_strdown (token, -1);
  g_strdelimit (out, " ", '-');
  return out;
}

static gchar *
resolve_canonical_for_token (UnityAppList *self, const gchar *token)
{
  if (token == NULL || *token == '\0')
    return NULL;

  GList            *list = astal_apps_apps_get_list (self->catalog);
  g_autofree gchar *with_suffix = g_strconcat (token, ".desktop", NULL);
  gchar            *result = NULL;

  for (GList *l = list; l != NULL && result == NULL; l = l->next)
    {
      const gchar *entry = astal_apps_application_get_entry (l->data);
      if (entry != NULL &&
          (g_ascii_strcasecmp (entry, token) == 0 ||
           g_ascii_strcasecmp (entry, with_suffix) == 0))
        result = g_strdup (entry);
    }
  for (GList *l = list; l != NULL && result == NULL; l = l->next)
    {
      const gchar *wm = astal_apps_application_get_wm_class (l->data);
      if (wm != NULL && *wm != '\0' && g_ascii_strcasecmp (wm, token) == 0)
        result = g_strdup (astal_apps_application_get_entry (l->data));
    }
  g_list_free (list);
  if (result != NULL)
    return result;

  GList *fuzzy = astal_apps_apps_fuzzy_query (self->catalog, token);
  if (fuzzy != NULL)
    result = g_strdup (astal_apps_application_get_entry (fuzzy->data));
  g_list_free (fuzzy);
  return result;
}

static const gchar *
canonical_id_for (UnityAppList *self, const gchar *raw_id)
{
  if (raw_id == NULL || *raw_id == '\0')
    return NULL;

  const gchar *cached = g_hash_table_lookup (self->id_canonical, raw_id);
  if (cached != NULL)
    return cached;

  gchar *canonical = NULL;
  g_auto (GStrv) tokens = g_strsplit_set (raw_id, " \t", -1);
  for (gchar **t = tokens; *t != NULL && canonical == NULL; t++)
    if (**t != '\0')
      canonical = resolve_canonical_for_token (self, *t);
  if (canonical == NULL)
    canonical = canonicalize_token (raw_id);

  g_hash_table_insert (self->id_canonical, g_strdup (raw_id), canonical);
  return canonical;
}

typedef struct
{
  UnityAppList *list;
  gchar             *canonical;
} AppIdMatch;

static void
app_id_match_free (gpointer p)
{
  AppIdMatch *m = p;
  g_free (m->canonical);
  g_free (m);
}

static gboolean
match_app_id (gpointer item, gpointer user_data)
{
  AstalWlrToplevel *tl   = item;
  AppIdMatch         *m    = user_data;
  const gchar        *tlid = astal_wlr_toplevel_get_app_id (tl);

  if (tlid == NULL || m->canonical == NULL)
    return FALSE;
  return g_strcmp0 (canonical_id_for (m->list, tlid), m->canonical) == 0;
}

static GListModel *
build_per_app_filter (UnityAppList *self, const gchar *canonical)
{
  AppIdMatch *m = g_new0 (AppIdMatch, 1);
  m->list      = self;
  m->canonical = g_strdup (canonical);

  GtkCustomFilter *filter = gtk_custom_filter_new (match_app_id, m, app_id_match_free);
  GListModel      *base   = self->toplevels ? g_object_ref (self->toplevels) : NULL;
  return G_LIST_MODEL (gtk_filter_list_model_new (base, GTK_FILTER (filter)));
}

static UnityAppEntry *
ensure_entry (UnityAppList *self, const gchar *canonical)
{
  if (canonical == NULL)
    return NULL;

  UnityAppEntry *entry = g_hash_table_lookup (self->cache, canonical);
  if (entry != NULL)
    return entry;

  g_autoptr (GDesktopAppInfo) dinfo = g_desktop_app_info_new (canonical);
  g_autoptr (GListModel) toplevels  = build_per_app_filter (self, canonical);
  entry = _unity_app_entry_new (canonical, dinfo ? G_APP_INFO (dinfo) : NULL, toplevels);

  g_hash_table_insert (self->cache, g_strdup (canonical), entry);
  return entry;
}

static void
append_unique_entry (UnityAppList *self, const gchar *raw_id, gboolean pinned,
                     GHashTable *seen, GPtrArray *out)
{
  const gchar *canonical = canonical_id_for (self, raw_id);
  if (canonical == NULL || g_hash_table_contains (seen, canonical))
    return;
  UnityAppEntry *entry = ensure_entry (self, canonical);
  if (entry == NULL)
    return;
  g_hash_table_add (seen, (gpointer) canonical);
  _unity_app_entry_set_pinned (entry, pinned);
  g_ptr_array_add (out, entry);
}

static GPtrArray *
compute_desired_order (UnityAppList *self)
{
  GPtrArray *out = g_ptr_array_new ();
  g_autoptr (GHashTable) seen = g_hash_table_new (g_str_hash, g_str_equal);

  if (self->pinned != NULL)
    for (gchar **id = self->pinned; *id != NULL; id++)
      append_unique_entry (self, *id, TRUE, seen, out);

  if (self->toplevels != NULL)
    {
      guint n = g_list_model_get_n_items (self->toplevels);
      for (guint i = 0; i < n; i++)
        {
          g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (self->toplevels, i);
          append_unique_entry (self, astal_wlr_toplevel_get_app_id (tl), FALSE, seen, out);
        }
    }

  return out;
}

static void
sync_to_desired (UnityAppList *self)
{
  g_autoptr (GPtrArray) desired = compute_desired_order (self);
  GPtrArray            *current = self->entries;

  g_autoptr (GHashTable) desired_set = g_hash_table_new (NULL, NULL);
  for (guint i = 0; i < desired->len; i++)
    g_hash_table_add (desired_set, g_ptr_array_index (desired, i));

  for (guint i = current->len; i-- > 0;)
    if (!g_hash_table_contains (desired_set, g_ptr_array_index (current, i)))
      {
        g_ptr_array_remove_index (current, i);
        g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
      }

  for (guint i = 0; i < desired->len; i++)
    {
      gpointer want = g_ptr_array_index (desired, i);
      if (i < current->len && g_ptr_array_index (current, i) == want)
        continue;

      gboolean moved = FALSE;
      for (guint j = i + 1; j < current->len; j++)
        if (g_ptr_array_index (current, j) == want)
          {
            g_ptr_array_remove_index (current, j);
            g_list_model_items_changed (G_LIST_MODEL (self), j, 1, 0);
            g_ptr_array_insert (current, i, want);
            g_list_model_items_changed (G_LIST_MODEL (self), i, 0, 1);
            moved = TRUE;
            break;
          }
      if (moved)
        continue;

      g_ptr_array_insert (current, i, want);
      g_list_model_items_changed (G_LIST_MODEL (self), i, 0, 1);
    }
}

static void
invalidate_app_filters (UnityAppList *self)
{
  GHashTableIter iter;
  gpointer       entry_p;

  g_hash_table_iter_init (&iter, self->cache);
  while (g_hash_table_iter_next (&iter, NULL, &entry_p))
    {
      GListModel *toplevels = unity_app_entry_get_toplevels (entry_p);
      if (!GTK_IS_FILTER_LIST_MODEL (toplevels))
        continue;
      GtkFilter *filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (toplevels));
      if (filter != NULL)
        gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
    }
}

static void
on_toplevel_app_id_notify (AstalWlrToplevel *toplevel, GParamSpec *pspec,
                           UnityAppList *self)
{
  (void) toplevel; (void) pspec;
  sync_to_desired (self);
  invalidate_app_filters (self);
}

static void
hook_toplevel (UnityAppList *self, AstalWlrToplevel *tl)
{
  g_signal_connect_object (tl, "notify::app-id",
                           G_CALLBACK (on_toplevel_app_id_notify), self, 0);
}

static void
on_toplevels_items_changed (GListModel *model, guint position, guint removed,
                            guint added, UnityAppList *self)
{
  (void) removed;

  for (guint i = 0; i < added; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (model, position + i);
      if (tl != NULL)
        hook_toplevel (self, tl);
    }
  sync_to_desired (self);
}

static void
on_catalog_changed (AstalAppsApps *catalog, GParamSpec *pspec, UnityAppList *self)
{
  (void) catalog; (void) pspec;
  g_hash_table_remove_all (self->id_canonical);
  g_hash_table_remove_all (self->cache);
  sync_to_desired (self);
}

static void
unity_app_list_dispose (GObject *object)
{
  UnityAppList *self = UNITY_APP_LIST (object);

  g_clear_pointer (&self->entries,      g_ptr_array_unref);
  g_clear_pointer (&self->cache,        g_hash_table_unref);
  g_clear_pointer (&self->id_canonical, g_hash_table_unref);
  g_clear_object  (&self->catalog);
  g_clear_object  (&self->toplevels);

  G_OBJECT_CLASS (unity_app_list_parent_class)->dispose (object);
}

static void
unity_app_list_finalize (GObject *object)
{
  UnityAppList *self = UNITY_APP_LIST (object);

  g_clear_pointer (&self->pinned, g_strfreev);

  G_OBJECT_CLASS (unity_app_list_parent_class)->finalize (object);
}

static void
unity_app_list_class_init (UnityAppListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose  = unity_app_list_dispose;
  object_class->finalize = unity_app_list_finalize;
}

static void
unity_app_list_init (UnityAppList *self)
{
  self->entries      = g_ptr_array_new ();
  self->cache        = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->id_canonical = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  self->catalog = astal_apps_apps_new ();
  g_signal_connect_object (self->catalog, "notify::list",
                           G_CALLBACK (on_catalog_changed), self, 0);

  self->toplevels = g_object_ref (G_LIST_MODEL (astal_wlr_toplevels_get_default ()));
  guint n = g_list_model_get_n_items (self->toplevels);
  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (self->toplevels, i);
      hook_toplevel (self, tl);
    }
  g_signal_connect_object (self->toplevels, "items-changed",
                           G_CALLBACK (on_toplevels_items_changed), self, 0);

  sync_to_desired (self);
}

/**
 * unity_app_list_new:
 *
 * Creates a new app list tracking pinned apps and running windows.
 *
 * Returns: (transfer full): a new #UnityAppList
 */
UnityAppList *
unity_app_list_new (void)
{
  return g_object_new (UNITY_TYPE_APP_LIST, NULL);
}

/**
 * unity_app_list_set_pinned_app_ids:
 * @self: a #UnityAppList
 * @app_ids: (nullable) (array zero-terminated=1): the pinned .desktop ids, in order
 *
 * Sets the pinned apps and resyncs the list.
 */
void
unity_app_list_set_pinned_app_ids (UnityAppList *self, const gchar *const *app_ids)
{
  g_return_if_fail (UNITY_IS_APP_LIST (self));

  g_strfreev (self->pinned);
  self->pinned = app_ids ? g_strdupv ((gchar **) app_ids) : NULL;
  sync_to_desired (self);
}

/**
 * unity_app_list_get_entry:
 * @self: a #UnityAppList
 * @app_id: a raw app-id, canonicalised internally
 *
 * Looks up the cached entry for an app-id.
 *
 * Returns: (transfer none) (nullable): the #UnityAppEntry, or %NULL if absent
 */
UnityAppEntry *
unity_app_list_get_entry (UnityAppList *self, const gchar *app_id)
{
  g_return_val_if_fail (UNITY_IS_APP_LIST (self), NULL);
  if (app_id == NULL || *app_id == '\0')
    return NULL;

  const gchar *canonical = canonical_id_for (self, app_id);
  return canonical ? g_hash_table_lookup (self->cache, canonical) : NULL;
}
