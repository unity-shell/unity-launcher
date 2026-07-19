#include "dash/search/unity-search-result.h"

#include "dash/search/unity-search-provider.h"

struct _UnitySearchResult
{
  GObject                   parent_instance;

  UnitySearchProvider *provider;
  gchar                    *id;
  gchar                    *name;
  gchar                    *description;
  GIcon                    *gicon;
  GStrv                     terms;
};

/**
 * UnitySearchResult:
 *
 * One result from a SearchProvider2 GetResultMetas entry.
 *
 * A UnitySearchResult holds the id, display name, optional description and
 * optional icon of a single match. It keeps a reference to the
 * #UnitySearchProvider that produced it, along with the query terms, so the
 * result can be activated later.
 */
G_DEFINE_FINAL_TYPE (UnitySearchResult, unity_search_result, G_TYPE_OBJECT)

/**
 * unity_search_result_new:
 * @provider: the provider that produced the result.
 * @id: the provider-local result identifier.
 * @name: the display name.
 * @description: (nullable): a longer description, or %NULL.
 * @gicon: (nullable): an icon for the result, or %NULL.
 * @terms: the query terms that produced the result.
 *
 * Creates a new search result.
 *
 * Returns: (transfer full): a new UnitySearchResult.
 */
UnitySearchResult *
unity_search_result_new (UnitySearchProvider *provider,
                               const gchar              *id,
                               const gchar              *name,
                               const gchar              *description,
                               GIcon                    *gicon,
                               const gchar *const       *terms)
{
  UnitySearchResult *self = g_object_new (UNITY_TYPE_SEARCH_RESULT, NULL);
  self->provider    = provider ? g_object_ref (provider) : NULL;
  self->id          = g_strdup (id);
  self->name        = g_strdup (name);
  self->description = g_strdup (description);
  self->gicon       = gicon ? g_object_ref (gicon) : NULL;
  self->terms       = g_strdupv ((gchar **) terms);
  return self;
}

/**
 * unity_search_result_get_id:
 * @self: a UnitySearchResult.
 *
 * Gets the provider-local identifier of the result.
 *
 * Returns: (transfer none): the result id.
 */
const gchar *
unity_search_result_get_id (UnitySearchResult *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_RESULT (self), NULL);
  return self->id;
}

/**
 * unity_search_result_get_name:
 * @self: a UnitySearchResult.
 *
 * Gets the display name of the result.
 *
 * Returns: (transfer none): the display name.
 */
const gchar *
unity_search_result_get_name (UnitySearchResult *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_RESULT (self), NULL);
  return self->name;
}

/**
 * unity_search_result_get_description:
 * @self: a UnitySearchResult.
 *
 * Gets the longer description of the result.
 *
 * Returns: (transfer none) (nullable): the description, or %NULL.
 */
const gchar *
unity_search_result_get_description (UnitySearchResult *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_RESULT (self), NULL);
  return self->description;
}

/**
 * unity_search_result_get_gicon:
 * @self: a UnitySearchResult.
 *
 * Gets the icon of the result.
 *
 * Returns: (transfer none) (nullable): the icon, or %NULL.
 */
GIcon *
unity_search_result_get_gicon (UnitySearchResult *self)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_RESULT (self), NULL);
  return self->gicon;
}

/**
 * unity_search_result_activate:
 * @self: a UnitySearchResult.
 * @timestamp: an activation timestamp.
 *
 * Activates the result through its provider, replaying the query terms that
 * produced it.
 */
void
unity_search_result_activate (UnitySearchResult *self, guint32 timestamp)
{
  g_return_if_fail (UNITY_IS_SEARCH_RESULT (self));
  if (self->provider != NULL)
    unity_search_provider_activate_result (
      self->provider, self->id, (const gchar *const *) self->terms, timestamp);
}

static void
unity_search_result_dispose (GObject *object)
{
  UnitySearchResult *self = UNITY_SEARCH_RESULT (object);
  g_clear_object (&self->provider);
  g_clear_object (&self->gicon);
  G_OBJECT_CLASS (unity_search_result_parent_class)->dispose (object);
}

static void
unity_search_result_finalize (GObject *object)
{
  UnitySearchResult *self = UNITY_SEARCH_RESULT (object);
  g_free (self->id);
  g_free (self->name);
  g_free (self->description);
  g_strfreev (self->terms);
  G_OBJECT_CLASS (unity_search_result_parent_class)->finalize (object);
}

static void
unity_search_result_class_init (UnitySearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose  = unity_search_result_dispose;
  object_class->finalize = unity_search_result_finalize;
}

static void
unity_search_result_init (UnitySearchResult *self)
{
  (void) self;
}
