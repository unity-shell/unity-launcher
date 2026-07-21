#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define UNITY_TYPE_SEARCH_APP_RESULTS (unity_search_app_results_get_type ())

G_DECLARE_FINAL_TYPE (UnitySearchAppResults, unity_search_app_results,
                      UNITY, SEARCH_APP_RESULTS, AdwBin)

GtkWidget *unity_search_app_results_new               (void);

guint      unity_search_app_results_fill              (UnitySearchAppResults *self,
                                                       const gchar           *query);
void       unity_search_app_results_clear             (UnitySearchAppResults *self);
void       unity_search_app_results_activate_selected (UnitySearchAppResults *self);
void       unity_search_app_results_focus             (UnitySearchAppResults *self);

G_END_DECLS
