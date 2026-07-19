#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UNITY_TYPE_TILE (unity_tile_get_type ())

G_DECLARE_DERIVABLE_TYPE (UnityTile, unity_tile, UNITY, TILE, GtkButton)

struct _UnityTileClass
{
  GtkButtonClass parent_class;

  void (*populate_menu) (UnityTile *self, GMenu *menu);
};

GtkBox   *unity_tile_get_box          (UnityTile *self);
gint      unity_tile_get_icon_size    (UnityTile *self);

void      unity_tile_set_gicon        (UnityTile *self, GIcon *icon);
void      unity_tile_set_running      (UnityTile *self, gboolean running);
void      unity_tile_set_active       (UnityTile *self, gboolean active);
void      unity_tile_set_menu_position (UnityTile *self, GtkPositionType position);

G_END_DECLS
