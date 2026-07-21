#pragma once

#include <gtk/gtk.h>

#include "dash/search/unity-search-provider.h"
#include "dash/search/unity-search-result.h"

G_BEGIN_DECLS

#define UNITY_TYPE_SEARCH_RESULT_ROWS (unity_search_result_rows_get_type ())

G_DECLARE_FINAL_TYPE (UnitySearchResultRows, unity_search_result_rows,
                      UNITY, SEARCH_RESULT_ROWS, GtkBox)

GtkWidget *unity_search_result_rows_new (UnitySearchProvider *provider,
                                         GPtrArray           *results);

G_END_DECLS
