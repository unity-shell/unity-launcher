#pragma once

#include <gtk/gtk.h>

#include "dash/search/unity-search-provider.h"

G_BEGIN_DECLS

#define UNITY_TYPE_SEARCH (unity_search_get_type ())

G_DECLARE_FINAL_TYPE (UnitySearch, unity_search, UNITY, SEARCH, GObject)

UnitySearch *unity_search_get_default (void);

void unity_search_query (UnitySearch *self, const gchar *query, guint limit);

G_END_DECLS
