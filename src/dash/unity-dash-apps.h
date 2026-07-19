#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define UNITY_TYPE_DASH_APPS (unity_dash_apps_get_type ())

G_DECLARE_FINAL_TYPE (UnityDashApps, unity_dash_apps,
                      UNITY, DASH_APPS, AdwBin)

GtkWidget *unity_dash_apps_new (void);

void unity_dash_apps_reset (UnityDashApps *self);

G_END_DECLS
