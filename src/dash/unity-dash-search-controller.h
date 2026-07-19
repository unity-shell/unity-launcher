#pragma once

#include <adwaita.h>

#include "dash/unity-dash-search.h"

G_BEGIN_DECLS

#define UNITY_TYPE_DASH_SEARCH_CONTROLLER (unity_dash_search_controller_get_type ())

G_DECLARE_FINAL_TYPE (UnityDashSearchController, unity_dash_search_controller,
                      UNITY, DASH_SEARCH_CONTROLLER, GObject)

UnityDashSearchController *unity_dash_search_controller_new (GtkSearchEntry  *entry,
                                                            AdwViewStack    *stack,
                                                            UnityDashSearch *search_page);

void unity_dash_search_controller_reset (UnityDashSearchController *self);

G_END_DECLS
