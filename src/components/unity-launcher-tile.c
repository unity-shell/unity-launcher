#include "components/unity-launcher-tile.h"

#include <adwaita.h>
#include <astal-wlr.h>
#include <gdk/wayland/gdkwayland.h>
#include <gio/gdesktopappinfo.h>
#include <graphene.h>

#include "unity-launcher-defs.h"
#include "components/unity-desktop-actions.h"

#define SPREAD_APP_ACTION UNITY_LAUNCHER_ACTION_SPREAD_APP

#define FOOTPRINT_PADDING     4

#define DND_OPACITY_ANIM_KEY  "dnd-opacity-anim"
#define DND_FADE_OUT_MS       150
#define DND_FADE_IN_MS        200

struct _UnityLauncherTile
{
  UnityTile      parent_instance;

  UnityAppEntry *entry;
  GtkWidget          *dot;
};

typedef enum
{
  PROP_ENTRY = 1,
} UnityLauncherTileProperty;
static GParamSpec *properties[PROP_ENTRY + 1];

/**
 * UnityLauncherTile:
 *
 * A launcher dock tile bound to a #UnityAppEntry.
 *
 * Built on the shared #UnityTile base, an app tile shows the app icon with a
 * running dot. A primary click launches, raises or minimizes the app, or
 * spreads its windows when two or more are open. A pinned tile acts as a drag
 * source for reordering. Its context menu adds pin and quit items to the base's
 * Open and .desktop actions.
 */
G_DEFINE_FINAL_TYPE (UnityLauncherTile, unity_launcher_tile, UNITY_TYPE_TILE)

static void
sync_image (UnityLauncherTile *self)
{
  GAppInfo *info = self->entry ? unity_app_entry_get_app_info (self->entry) : NULL;

  unity_tile_set_gicon (UNITY_TILE (self), info ? g_app_info_get_icon (info) : NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self),
                               info ? g_app_info_get_display_name (info) : NULL);
}

static void
sync_footprint (UnityLauncherTile *self)
{
  gint side = unity_tile_get_icon_size (UNITY_TILE (self)) + FOOTPRINT_PADDING;
  gtk_widget_set_size_request (
    GTK_WIDGET (unity_tile_get_box (UNITY_TILE (self))), side, side);
}

static void
sync_running (UnityLauncherTile *self)
{
  gboolean running = self->entry && unity_app_entry_get_running (self->entry);

  gtk_widget_set_opacity (self->dot, running ? 1.0 : 0.0);
  unity_tile_set_running (UNITY_TILE (self), running);
}

static void
sync_active (UnityLauncherTile *self)
{
  unity_tile_set_active (UNITY_TILE (self),
                               self->entry && unity_app_entry_get_activated (self->entry));
}

static void
push_rectangle_hints (UnityLauncherTile *self)
{
  if (self->entry == NULL || !gtk_widget_get_mapped (GTK_WIDGET (self)))
    return;

  GListModel *toplevels = unity_app_entry_get_toplevels (self->entry);
  guint n = toplevels ? g_list_model_get_n_items (toplevels) : 0;
  if (n == 0)
    return;

  GtkWidget  *widget = GTK_WIDGET (self);
  GtkRoot    *root   = gtk_widget_get_root (widget);
  if (root == NULL || !GTK_IS_NATIVE (root))
    return;
  GdkSurface *gsurface = gtk_native_get_surface (GTK_NATIVE (root));
  if (gsurface == NULL || !GDK_IS_WAYLAND_SURFACE (gsurface))
    return;
  struct wl_surface *wsurface = gdk_wayland_surface_get_wl_surface (gsurface);
  if (wsurface == NULL)
    return;

  int w = gtk_widget_get_width  (widget);
  int h = gtk_widget_get_height (widget);
  if (w <= 0 || h <= 0)
    return;

  graphene_point_t origin = GRAPHENE_POINT_INIT (0.f, 0.f);
  graphene_point_t mapped;
  if (!gtk_widget_compute_point (widget, GTK_WIDGET (root), &origin, &mapped))
    return;

  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (toplevels, i);
      if (tl != NULL)
        astal_wlr_toplevel_set_rectangle (tl, wsurface,
                                          (int) mapped.x, (int) mapped.y, w, h);
    }
}

static void
on_tile_map (GtkWidget *widget, gpointer data)
{
  (void) data;
  push_rectangle_hints (UNITY_LAUNCHER_TILE (widget));
}

static void
on_toplevels_items_changed (GListModel *model, guint position, guint removed,
                            guint added, gpointer data)
{
  (void) model; (void) position; (void) removed;
  if (added > 0)
    push_rectangle_hints (UNITY_LAUNCHER_TILE (data));
}

static void
spread_app_windows (UnityLauncherTile *self)
{
  GListModel *toplevels = unity_app_entry_get_toplevels (self->entry);
  guint       n         = toplevels ? g_list_model_get_n_items (toplevels) : 0;

  g_autoptr (GPtrArray) ids = g_ptr_array_new_with_free_func (g_free);
  g_autoptr (GHashTable) seen = g_hash_table_new (g_str_hash, g_str_equal);
  for (guint i = 0; i < n; i++)
    {
      g_autoptr (AstalWlrToplevel) tl = g_list_model_get_item (toplevels, i);
      const gchar *aid = tl ? astal_wlr_toplevel_get_app_id (tl) : NULL;
      if (aid == NULL || *aid == '\0' || g_hash_table_contains (seen, aid))
        continue;

      gchar *dup = g_strdup (aid);
      g_ptr_array_add (ids, dup);
      g_hash_table_add (seen, dup);
    }
  g_ptr_array_add (ids, NULL);

  gtk_widget_activate_action (GTK_WIDGET (self), SPREAD_APP_ACTION, "^as",
                              (const gchar *const *) ids->pdata);
}

static void
on_self_clicked (GtkButton *button, gpointer user_data)
{
  (void) user_data;
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (button);
  if (self->entry == NULL)
    return;

  push_rectangle_hints (self);

  GListModel *toplevels = unity_app_entry_get_toplevels (self->entry);
  if (toplevels != NULL && g_list_model_get_n_items (toplevels) >= 2)
    spread_app_windows (self);
  else
    unity_app_entry_activate_or_launch (self->entry);
}

static void
launch_action_activated (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (user_data);
  if (self->entry == NULL)
    return;
  GAppInfo *info = unity_app_entry_get_app_info (self->entry);
  if (info == NULL || !G_IS_DESKTOP_APP_INFO (info))
    return;
  g_desktop_app_info_launch_action (G_DESKTOP_APP_INFO (info),
                                    g_variant_get_string (param, NULL), NULL);
}

static void
launch_default_activated (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action; (void) param;
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (user_data);
  if (self->entry == NULL)
    return;
  GAppInfo *info = unity_app_entry_get_app_info (self->entry);
  if (info == NULL)
    return;

  g_autoptr (GError) error = NULL;
  if (!g_app_info_launch (info, NULL, NULL, &error))
    g_warning ("UnityLauncherTile: launch failed: %s",
               error ? error->message : "unknown error");
}

static void
install_tile_actions (UnityLauncherTile *self)
{
  static const GActionEntry entries[] = {
    { "launch",        launch_default_activated, NULL, NULL, NULL, { 0, 0, 0 } },
    { "launch-action", launch_action_activated,  "s",  NULL, NULL, { 0, 0, 0 } },
  };

  g_autoptr (GSimpleActionGroup) group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "tile", G_ACTION_GROUP (group));
}

static void
unity_launcher_tile_populate_menu (UnityTile *tile, GMenu *menu)
{
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (tile);
  GAppInfo          *info = self->entry ? unity_app_entry_get_app_info (self->entry) : NULL;

  {
    g_autoptr (GMenu) section = g_menu_new ();
    g_menu_append (section, "Open", "tile.launch");
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  }

  if (info != NULL && G_IS_DESKTOP_APP_INFO (info))
    unity_desktop_actions_append (
      menu, G_DESKTOP_APP_INFO (info), "tile.launch-action");

  if (self->entry == NULL)
    return;
  const gchar *app_id = unity_app_entry_get_app_id (self->entry);
  if (app_id == NULL || *app_id == '\0')
    return;

  GVariant *target = g_variant_new_string (app_id);

  {
    gboolean pinned = unity_app_entry_get_pinned (self->entry);
    g_autoptr (GMenu)     section = g_menu_new ();
    g_autoptr (GMenuItem) item    = g_menu_item_new (
      pinned ? "Unpin from Launcher" : "Pin to Launcher", NULL);
    g_menu_item_set_action_and_target_value (item, UNITY_LAUNCHER_ACTION_PIN_TOGGLE, target);
    g_menu_append_item (section, item);
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
  }

  if (unity_app_entry_get_running (self->entry))
    {
      g_autoptr (GMenu)     section = g_menu_new ();
      g_autoptr (GMenuItem) item    = g_menu_item_new ("Quit", NULL);
      g_menu_item_set_action_and_target_value (item, UNITY_LAUNCHER_ACTION_QUIT, target);
      g_menu_append_item (section, item);
      g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
    }
}

static GdkContentProvider *
on_drag_prepare (GtkDragSource *source, gdouble x, gdouble y, gpointer user_data)
{
  (void) source; (void) x; (void) y;
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (user_data);
  if (self->entry == NULL || !unity_app_entry_get_pinned (self->entry))
    return NULL;
  const gchar *app_id = unity_app_entry_get_app_id (self->entry);
  if (app_id == NULL || *app_id == '\0')
    return NULL;
  return gdk_content_provider_new_typed (G_TYPE_STRING, app_id);
}

static void
fade_opacity (GtkWidget *widget, gdouble from, gdouble to, guint ms)
{
  AdwAnimation *old = g_object_get_data (G_OBJECT (widget), DND_OPACITY_ANIM_KEY);
  if (old != NULL)
    adw_animation_reset (old);

  AdwAnimationTarget *tgt = adw_property_animation_target_new (G_OBJECT (widget), "opacity");
  AdwAnimation *anim = adw_timed_animation_new (widget, from, to, ms, tgt);
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (anim), ADW_EASE_OUT_QUAD);
  g_object_set_data_full (G_OBJECT (widget), DND_OPACITY_ANIM_KEY, anim, g_object_unref);
  adw_animation_play (anim);
}

static GdkPaintable *
build_drag_paintable (UnityLauncherTile *self)
{
  if (self->entry == NULL)
    return NULL;
  GAppInfo *info = unity_app_entry_get_app_info (self->entry);
  GIcon    *icon = info ? g_app_info_get_icon (info) : NULL;
  if (icon == NULL)
    return NULL;

  gint          size  = unity_tile_get_icon_size (UNITY_TILE (self));
  gint          scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
  GtkIconTheme *theme = gtk_icon_theme_get_for_display (gtk_widget_get_display (GTK_WIDGET (self)));

  GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon (
    theme, icon, size, scale, GTK_TEXT_DIR_NONE, GTK_ICON_LOOKUP_FORCE_REGULAR);
  return paintable ? GDK_PAINTABLE (paintable) : NULL;
}

static void
on_drag_begin (GtkDragSource *source, GdkDrag *drag, gpointer user_data)
{
  (void) drag;
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (user_data);
  gint size = unity_tile_get_icon_size (UNITY_TILE (self));

  g_autoptr (GdkPaintable) paintable = build_drag_paintable (self);
  if (paintable != NULL)
    gtk_drag_source_set_icon (source, paintable, size / 2, size / 2);

  fade_opacity (GTK_WIDGET (self), 1.0, 0.0, DND_FADE_OUT_MS);
}

static void
on_drag_end (GtkDragSource *source, GdkDrag *drag, gboolean delete_data, gpointer user_data)
{
  (void) source; (void) drag;
  GtkWidget *widget = GTK_WIDGET (user_data);

  if (delete_data)
    {
      g_object_set_data (G_OBJECT (widget), DND_OPACITY_ANIM_KEY, NULL);
      gtk_widget_set_opacity (widget, 1.0);
    }
  else
    {
      fade_opacity (widget, 0.0, 1.0, DND_FADE_IN_MS);
    }
}

static void
group_with_click_gesture (GtkWidget *widget, GtkGesture *drag_gesture)
{
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (widget);
  guint n = g_list_model_get_n_items (controllers);
  for (guint i = 0; i < n; i++)
    {
      g_autoptr (GtkEventController) ctrl = g_list_model_get_item (controllers, i);
      if (GTK_IS_GESTURE_CLICK (ctrl))
        {
          gtk_gesture_group (drag_gesture, GTK_GESTURE (ctrl));
          break;
        }
    }
}

static void
install_dnd (UnityLauncherTile *self)
{
  GtkDragSource *drag_source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (drag_source, GDK_ACTION_MOVE);
  g_signal_connect_object (drag_source, "prepare",    G_CALLBACK (on_drag_prepare), self, 0);
  g_signal_connect_object (drag_source, "drag-begin", G_CALLBACK (on_drag_begin),   self, 0);
  g_signal_connect_object (drag_source, "drag-end",   G_CALLBACK (on_drag_end),     self, 0);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag_source));
  group_with_click_gesture (GTK_WIDGET (self), GTK_GESTURE (drag_source));
}

static void
on_entry_app_info_notify (UnityAppEntry *entry, GParamSpec *pspec, UnityLauncherTile *self)
{
  (void) entry; (void) pspec;
  sync_image (self);
}

static void
on_entry_running_notify (UnityAppEntry *entry, GParamSpec *pspec, UnityLauncherTile *self)
{
  (void) entry; (void) pspec;
  sync_running (self);
}

static void
on_entry_activated_notify (UnityAppEntry *entry, GParamSpec *pspec, UnityLauncherTile *self)
{
  (void) entry; (void) pspec;
  sync_active (self);
}

static void
on_icon_size_notify (GObject *object, GParamSpec *pspec, gpointer user_data)
{
  (void) object; (void) pspec;
  sync_footprint (user_data);
}

static void
construct_entry_bindings (UnityLauncherTile *self, UnityAppEntry *entry)
{
  if (entry == NULL)
    return;

  self->entry = g_object_ref (entry);

  g_signal_connect_object (entry, "notify::app-info",
                           G_CALLBACK (on_entry_app_info_notify),  self, 0);
  g_signal_connect_object (entry, "notify::running",
                           G_CALLBACK (on_entry_running_notify),   self, 0);
  g_signal_connect_object (entry, "notify::activated",
                           G_CALLBACK (on_entry_activated_notify), self, 0);

  GListModel *toplevels = unity_app_entry_get_toplevels (entry);
  g_signal_connect_object (toplevels, "items-changed",
                           G_CALLBACK (on_toplevels_items_changed), self, 0);

  sync_image   (self);
  sync_running (self);
  sync_active  (self);
}

/**
 * unity_launcher_tile_new:
 * @entry: the #UnityAppEntry the tile represents
 *
 * Creates a new app tile bound to @entry.
 *
 * Returns: (transfer full): a new #UnityLauncherTile, as a #GtkWidget
 */
GtkWidget *
unity_launcher_tile_new (UnityAppEntry *entry)
{
  return g_object_new (UNITY_TYPE_LAUNCHER_TILE, "entry", entry, NULL);
}

/**
 * unity_launcher_tile_get_app_id:
 * @self: a #UnityLauncherTile
 *
 * Gets the application id of the tile's entry.
 *
 * Returns: (nullable): the app id, or %NULL
 */
const gchar *
unity_launcher_tile_get_app_id (UnityLauncherTile *self)
{
  g_return_val_if_fail (UNITY_IS_LAUNCHER_TILE (self), NULL);
  return self->entry ? unity_app_entry_get_app_id (self->entry) : NULL;
}

/**
 * unity_launcher_tile_get_pinned:
 * @self: a #UnityLauncherTile
 *
 * Gets whether the tile's entry is pinned to the launcher.
 *
 * Returns: %TRUE if the entry is pinned
 */
gboolean
unity_launcher_tile_get_pinned (UnityLauncherTile *self)
{
  g_return_val_if_fail (UNITY_IS_LAUNCHER_TILE (self), FALSE);
  return self->entry ? unity_app_entry_get_pinned (self->entry) : FALSE;
}

static void
unity_launcher_tile_dispose (GObject *object)
{
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (object);
  g_clear_object (&self->entry);
  G_OBJECT_CLASS (unity_launcher_tile_parent_class)->dispose (object);
}

static void
unity_launcher_tile_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (object);
  switch ((UnityLauncherTileProperty) id)
    {
    case PROP_ENTRY: g_value_set_object (value, self->entry); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
unity_launcher_tile_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
  UnityLauncherTile *self = UNITY_LAUNCHER_TILE (object);
  switch ((UnityLauncherTileProperty) id)
    {
    case PROP_ENTRY: construct_entry_bindings (self, g_value_get_object (value)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
unity_launcher_tile_class_init (UnityLauncherTileClass *klass)
{
  GObjectClass    *object_class = G_OBJECT_CLASS (klass);
  UnityTileClass *tile_class = UNITY_TILE_CLASS (klass);

  object_class->dispose      = unity_launcher_tile_dispose;
  object_class->get_property = unity_launcher_tile_get_property;
  object_class->set_property = unity_launcher_tile_set_property;

  tile_class->populate_menu = unity_launcher_tile_populate_menu;

  properties[PROP_ENTRY] = g_param_spec_object (
    "entry", NULL, NULL, UNITY_TYPE_APP_ENTRY,
    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "tile");
}

static void
unity_launcher_tile_init (UnityLauncherTile *self)
{
  self->dot = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (self->dot, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (self->dot, "running-dot");
  gtk_box_append (unity_tile_get_box (UNITY_TILE (self)), self->dot);

  unity_tile_set_menu_position (UNITY_TILE (self), GTK_POS_RIGHT);

  sync_footprint (self);
  install_tile_actions (self);
  install_dnd (self);

  g_signal_connect (self, "clicked", G_CALLBACK (on_self_clicked), NULL);
  g_signal_connect (self, "notify::icon-size", G_CALLBACK (on_icon_size_notify), self);
  g_signal_connect (self, "map", G_CALLBACK (on_tile_map), NULL);
}
