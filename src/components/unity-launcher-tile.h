#pragma once

#include "components/unity-tile.h"
#include "unity-app-entry.h"

G_BEGIN_DECLS

#define UNITY_TYPE_LAUNCHER_TILE (unity_launcher_tile_get_type ())

G_DECLARE_FINAL_TYPE (UnityLauncherTile, unity_launcher_tile,
                      UNITY, LAUNCHER_TILE, UnityTile)

GtkWidget   *unity_launcher_tile_new        (UnityAppEntry *entry);
const gchar *unity_launcher_tile_get_app_id (UnityLauncherTile *self);
gboolean     unity_launcher_tile_get_pinned (UnityLauncherTile *self);

G_END_DECLS
