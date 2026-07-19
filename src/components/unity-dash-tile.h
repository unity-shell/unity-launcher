#pragma once

#include <astal-apps.h>

#include "components/unity-tile.h"

G_BEGIN_DECLS

#define UNITY_TYPE_DASH_TILE (unity_dash_tile_get_type ())

G_DECLARE_FINAL_TYPE (UnityDashTile, unity_dash_tile,
                      UNITY, DASH_TILE, UnityTile)

GtkWidget *unity_dash_tile_new (AstalAppsApplication *app);

G_END_DECLS
