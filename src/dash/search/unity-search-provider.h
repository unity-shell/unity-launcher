#pragma once

#include <gtk/gtk.h>

#include "dash/search/unity-search-result.h"

G_BEGIN_DECLS

#define UNITY_TYPE_SEARCH_PROVIDER (unity_search_provider_get_type ())

G_DECLARE_FINAL_TYPE (UnitySearchProvider, unity_search_provider,
                      UNITY, SEARCH_PROVIDER, GObject)

GList *unity_search_provider_discover (void);

const gchar *unity_search_provider_get_name  (UnitySearchProvider *self);
GIcon       *unity_search_provider_get_gicon (UnitySearchProvider *self);

void       unity_search_provider_query_async  (UnitySearchProvider *self,
                                                     const gchar *const       *terms,
                                                     guint                     limit,
                                                     GCancellable             *cancellable,
                                                     GAsyncReadyCallback       callback,
                                                     gpointer                  user_data);
GPtrArray *unity_search_provider_query_finish (UnitySearchProvider *self,
                                                     GAsyncResult             *result,
                                                     GError                  **error);

void       unity_search_provider_activate_result (UnitySearchProvider *self,
                                                        const gchar              *id,
                                                        const gchar *const       *terms,
                                                        guint32                   timestamp);

G_END_DECLS
