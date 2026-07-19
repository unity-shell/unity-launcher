#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define UNITY_TYPE_DASH_SEARCH (unity_dash_search_get_type ())

G_DECLARE_FINAL_TYPE (UnityDashSearch, unity_dash_search,
                      UNITY, DASH_SEARCH, AdwBin)

GtkWidget *unity_dash_search_new (void);

void unity_dash_search_run (UnityDashSearch *self, const gchar *query);

void unity_dash_search_activate_selected (UnityDashSearch *self);

void unity_dash_search_focus_results (UnityDashSearch *self);

void unity_dash_search_reset (UnityDashSearch *self);

G_END_DECLS
