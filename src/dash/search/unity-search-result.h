#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UNITY_TYPE_SEARCH_RESULT (unity_search_result_get_type ())

G_DECLARE_FINAL_TYPE (UnitySearchResult, unity_search_result,
                      UNITY, SEARCH_RESULT, GObject)

typedef struct _UnitySearchProvider UnitySearchProvider;

UnitySearchResult *unity_search_result_new (UnitySearchProvider *provider,
                                                       const gchar              *id,
                                                       const gchar              *name,
                                                       const gchar              *description,
                                                       GIcon                    *gicon,
                                                       const gchar *const       *terms);

const gchar *unity_search_result_get_id          (UnitySearchResult *self);
const gchar *unity_search_result_get_name        (UnitySearchResult *self);
const gchar *unity_search_result_get_description  (UnitySearchResult *self);
GIcon       *unity_search_result_get_gicon        (UnitySearchResult *self);

void         unity_search_result_activate         (UnitySearchResult *self,
                                                         guint32                 timestamp);

G_END_DECLS
