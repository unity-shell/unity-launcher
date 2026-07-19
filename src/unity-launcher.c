#include "unity-launcher.h"

#include <adwaita.h>

#include "unity-app-entry.h"
#include "unity-app-list.h"
#include "components/unity-launcher-tile.h"
#include "unity-launcher-defs.h"
#include "components/unity-pinned-apps.h"


#define DASH_ANIMATION_MS 200
#define DASH_BUTTON_PADDING 6

struct _UnityLauncher
{
  AstalWindow        parent_instance;
  GtkImage          *dash_image;
  GtkBox            *strip;

  GSettings         *settings;
  UnityAppList      *apps;

  GtkWidget         *placeholder;
  AdwAnimation      *ph_anim;
  gint               ph_index;
};

/**
 * UnityLauncher:
 *
 * A layer-shell window anchored to the left edge at full height.
 *
 * The launcher holds a vertical strip of app tiles, pinned apps first then
 * running-but-unpinned apps, with drag-to-reorder within the pinned run. It
 * owns the `launcher` action group (pin-toggle, quit, reorder-pinned) that its
 * tiles target.
 */
G_DEFINE_FINAL_TYPE (UnityLauncher, unity_launcher, ASTAL_TYPE_WINDOW)

static void remove_placeholder (UnityLauncher *self);

static void
commit_pinned (UnityLauncher *self, GPtrArray *items)
{
  g_ptr_array_add (items, NULL);
  g_settings_set_strv (self->settings, UNITY_LAUNCHER_KEY_PINNED_APPS,
                       (const gchar *const *) items->pdata);
}

static void
on_pin_toggle (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  UnityLauncher *self = user_data;
  if (param == NULL)
    return;
  unity_pinned_apps_toggle (self->settings, g_variant_get_string (param, NULL));
}

static void
on_quit (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  UnityLauncher *self = user_data;
  if (param == NULL)
    return;

  UnityAppEntry *entry =
    unity_app_list_get_entry (self->apps, g_variant_get_string (param, NULL));
  if (entry != NULL)
    unity_app_entry_close_all (entry);
}

static void
on_reorder_pinned (GSimpleAction *action, GVariant *param, gpointer user_data)
{
  (void) action;
  UnityLauncher *self = user_data;
  if (param == NULL)
    return;

  const gchar *source = NULL;
  gint32       dest   = 0;
  g_variant_get (param, "(&si)", &source, &dest);
  if (source == NULL || *source == '\0')
    return;

  g_auto (GStrv) ids = g_settings_get_strv (self->settings, UNITY_LAUNCHER_KEY_PINNED_APPS);
  if (ids == NULL)
    return;

  g_autoptr (GPtrArray) next = g_ptr_array_new_with_free_func (g_free);
  gboolean found = FALSE;
  for (gchar **p = ids; *p != NULL; p++)
    {
      if (g_strcmp0 (*p, source) == 0) { found = TRUE; continue; }
      g_ptr_array_add (next, g_strdup (*p));
    }
  if (!found)
    return;

  dest = CLAMP (dest, 0, (gint32) next->len);
  g_ptr_array_insert (next, dest, g_strdup (source));
  commit_pinned (self, next);
}

static void
install_action_group (UnityLauncher *self)
{
  static const GActionEntry entries[] = {
    { "pin-toggle",     on_pin_toggle,     "s",     NULL, NULL, { 0, 0, 0 } },
    { "quit",           on_quit,           "s",     NULL, NULL, { 0, 0, 0 } },
    { "reorder-pinned", on_reorder_pinned, "(si)", NULL, NULL, { 0, 0, 0 } },
  };

  g_autoptr (GSimpleActionGroup) group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  UNITY_LAUNCHER_ACTION_GROUP, G_ACTION_GROUP (group));
}

static void
sync_dash_button (UnityLauncher *self)
{
  gint size = g_settings_get_int (self->settings, UNITY_LAUNCHER_KEY_LAUNCHER_ICON_SIZE);
  gint side = size + DASH_BUTTON_PADDING;
  gtk_widget_set_size_request (GTK_WIDGET (self->dash_image), side, side);
  gtk_image_set_pixel_size (self->dash_image, size / 2);
}

static GtkWidget *
create_launcher_tile (UnityLauncher *self, UnityAppEntry *entry)
{
  GtkWidget *tile = unity_launcher_tile_new (entry);
  g_settings_bind (self->settings, UNITY_LAUNCHER_KEY_LAUNCHER_ICON_SIZE,
                   tile, "icon-size", G_SETTINGS_BIND_GET);
  return tile;
}

static gboolean
is_tile (GtkWidget *w)
{
  return w != NULL && UNITY_IS_LAUNCHER_TILE (w);
}

static GtkWidget *
nth_tile (UnityLauncher *self, gint n)
{
  for (GtkWidget *c = gtk_widget_get_first_child (GTK_WIDGET (self->strip));
       c != NULL; c = gtk_widget_get_next_sibling (c))
    if (is_tile (c) && n-- == 0)
      return c;
  return NULL;
}

static void
on_model_items_changed (GListModel *model, guint position, guint removed,
                        guint added, gpointer user_data)
{
  UnityLauncher *self = user_data;

  remove_placeholder (self);

  GtkWidget *child = nth_tile (self, position);
  for (guint i = 0; i < removed && child != NULL; i++)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);
      gtk_box_remove (self->strip, child);
      child = next;
    }

  GtkWidget *after = position > 0 ? nth_tile (self, (gint) position - 1) : NULL;
  for (guint i = 0; i < added; i++)
    {
      g_autoptr (GObject) entry = g_list_model_get_item (model, position + i);
      GtkWidget *tile = create_launcher_tile (self, UNITY_APP_ENTRY (entry));
      gtk_box_insert_child_after (self->strip, tile, after);
      after = tile;
    }
}

static gint
slot_extent (UnityLauncher *self)
{
  GtkWidget *t = nth_tile (self, 0);
  gint       e = t ? gtk_widget_get_height (t) : 0;
  if (e <= 0)
    e = g_settings_get_int (self->settings, UNITY_LAUNCHER_KEY_LAUNCHER_ICON_SIZE) + 8;
  return e;
}

static void
ph_anim_cb (gdouble value, gpointer data)
{
  GtkWidget *ph    = data;
  gint       slot  = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (ph), "slot"));
  gint       cross = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (ph), "cross"));
  gint       along = (gint) (value * slot + 0.5);

  gtk_widget_set_size_request (ph, cross, along);
  gtk_widget_set_opacity (ph, value);
}

static void
remove_placeholder (UnityLauncher *self)
{
  if (self->ph_anim != NULL)
    {
      adw_animation_reset (self->ph_anim);
      g_clear_object (&self->ph_anim);
    }
  if (self->placeholder != NULL)
    {
      gtk_box_remove (self->strip, self->placeholder);
      self->placeholder = NULL;
    }
  self->ph_index = -1;
}

static void
ph_out_done (AdwAnimation *anim, gpointer data)
{
  (void) anim;
  GtkWidget *ph     = data;
  GtkWidget *parent = gtk_widget_get_parent (ph);
  if (parent != NULL)
    gtk_box_remove (GTK_BOX (parent), ph);
}

static void
fade_out_placeholder (UnityLauncher *self)
{
  GtkWidget *ph = self->placeholder;
  if (self->ph_anim != NULL)
    {
      adw_animation_reset (self->ph_anim);
      g_clear_object (&self->ph_anim);
    }
  self->placeholder = NULL;
  self->ph_index    = -1;
  if (ph == NULL)
    return;

  AdwAnimationTarget *t = adw_callback_animation_target_new (ph_anim_cb, ph, NULL);
  AdwAnimation *a = adw_timed_animation_new (ph, gtk_widget_get_opacity (ph), 0.0,
                                             DASH_ANIMATION_MS, t);
  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (a), ADW_EASE_OUT_QUAD);
  g_signal_connect (a, "done", G_CALLBACK (ph_out_done), ph);
  self->ph_anim = a;
  adw_animation_play (a);
}

static void
show_placeholder (UnityLauncher *self, gint idx)
{
  if (idx == self->ph_index)
    return;

  gboolean first = (self->placeholder == NULL);
  remove_placeholder (self);
  if (idx < 0)
    return;

  GtkWidget *t0    = nth_tile (self, 0);
  gint       cross = t0 ? gtk_widget_get_width (t0) : -1;
  gint       slot  = slot_extent (self);

  GtkWidget *ph = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (ph, "drag-placeholder");
  g_object_set_data (G_OBJECT (ph), "slot",  GINT_TO_POINTER (slot));
  g_object_set_data (G_OBJECT (ph), "cross", GINT_TO_POINTER (cross));

  GtkWidget *after = idx > 0 ? nth_tile (self, idx - 1) : NULL;
  gtk_box_insert_child_after (self->strip, ph, after);
  self->placeholder = ph;
  self->ph_index    = idx;

  if (first)
    {
      ph_anim_cb (0.0, ph);
      AdwAnimationTarget *t = adw_callback_animation_target_new (ph_anim_cb, ph, NULL);
      AdwAnimation *a = adw_timed_animation_new (ph, 0.0, 1.0, DASH_ANIMATION_MS, t);
      adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (a), ADW_EASE_OUT_QUAD);
      self->ph_anim = a;
      adw_animation_play (a);
    }
  else
    {
      ph_anim_cb (1.0, ph);
    }
}

static gint
pinned_index_of (UnityLauncher *self, const gchar *app_id)
{
  gint i = 0;
  for (GtkWidget *c = gtk_widget_get_first_child (GTK_WIDGET (self->strip));
       c != NULL; c = gtk_widget_get_next_sibling (c))
    {
      if (!is_tile (c))
        continue;
      if (!unity_launcher_tile_get_pinned (UNITY_LAUNCHER_TILE (c)))
        break;
      if (g_strcmp0 (unity_launcher_tile_get_app_id (UNITY_LAUNCHER_TILE (c)), app_id) == 0)
        return i;
      i++;
    }
  return -1;
}

static gint
pinned_slot_for_y (UnityLauncher *self, gdouble y)
{
  gint slot = 0;
  for (GtkWidget *c = gtk_widget_get_first_child (GTK_WIDGET (self->strip));
       c != NULL; c = gtk_widget_get_next_sibling (c))
    {
      if (!is_tile (c))
        continue;
      if (!unity_launcher_tile_get_pinned (UNITY_LAUNCHER_TILE (c)))
        break;
      graphene_rect_t b;
      if (!gtk_widget_compute_bounds (c, GTK_WIDGET (self->strip), &b))
        continue;
      if (y > b.origin.y + b.size.height / 2.0)
        slot++;
      else
        break;
    }
  return slot;
}

static gint
slot_to_dest (UnityLauncher *self, gint slot, const gchar *source)
{
  gint src_idx = pinned_index_of (self, source);
  return (src_idx >= 0 && slot > src_idx) ? slot - 1 : slot;
}

static GdkDragAction
on_strip_drag_motion (GtkDropTarget *target, gdouble x, gdouble y, gpointer user_data)
{
  (void) x;
  UnityLauncher *self = user_data;

  if (self->ph_anim != NULL &&
      adw_animation_get_state (self->ph_anim) == ADW_ANIMATION_PLAYING)
    return GDK_ACTION_MOVE;

  const GValue *value  = gtk_drop_target_get_value (target);
  const gchar  *source = (value != NULL && G_VALUE_HOLDS_STRING (value))
                         ? g_value_get_string (value) : NULL;

  if (source == NULL || pinned_index_of (self, source) < 0)
    {
      show_placeholder (self, -1);
      return 0;
    }

  show_placeholder (self, pinned_slot_for_y (self, y));
  return GDK_ACTION_MOVE;
}

static void
on_strip_drag_leave (GtkDropTarget *target, gpointer user_data)
{
  (void) target;
  fade_out_placeholder (user_data);
}

static gboolean
on_strip_drop (GtkDropTarget *target, const GValue *value, gdouble x, gdouble y,
               gpointer user_data)
{
  (void) target; (void) x;
  UnityLauncher *self = user_data;

  const gchar *source = G_VALUE_HOLDS_STRING (value) ? g_value_get_string (value) : NULL;
  if (source == NULL || pinned_index_of (self, source) < 0)
    {
      remove_placeholder (self);
      return FALSE;
    }

  gint slot = self->ph_index >= 0 ? self->ph_index : pinned_slot_for_y (self, y);
  gint dest = slot_to_dest (self, slot, source);

  remove_placeholder (self);
  gtk_widget_activate_action (GTK_WIDGET (self), UNITY_LAUNCHER_ACTION_REORDER,
                              "(si)", source, dest);
  return TRUE;
}

static void
install_drop_target (UnityLauncher *self)
{
  GtkDropTarget *t = gtk_drop_target_new (G_TYPE_STRING, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (t, TRUE);
  g_signal_connect_object (t, "motion", G_CALLBACK (on_strip_drag_motion), self, 0);
  g_signal_connect_object (t, "leave",  G_CALLBACK (on_strip_drag_leave),  self, 0);
  g_signal_connect_object (t, "drop",   G_CALLBACK (on_strip_drop),        self, 0);
  gtk_widget_add_controller (GTK_WIDGET (self->strip), GTK_EVENT_CONTROLLER (t));
}

static void
sync_pinned_apps (UnityLauncher *self)
{
  g_auto (GStrv) ids = g_settings_get_strv (self->settings, UNITY_LAUNCHER_KEY_PINNED_APPS);
  unity_app_list_set_pinned_app_ids (self->apps, (const gchar *const *) ids);
}

static void
unity_launcher_dispose (GObject *object)
{
  UnityLauncher *self = UNITY_LAUNCHER (object);

  if (self->ph_anim != NULL)
    {
      adw_animation_reset (self->ph_anim);
      g_clear_object (&self->ph_anim);
    }
  self->placeholder = NULL;

  g_clear_object (&self->apps);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (unity_launcher_parent_class)->dispose (object);
}

static void
unity_launcher_class_init (UnityLauncherClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = unity_launcher_dispose;

  gtk_widget_class_set_template_from_resource (
    widget_class, "/org/unity/launcher/unity-launcher.ui");
  gtk_widget_class_bind_template_child (widget_class, UnityLauncher, dash_image);
  gtk_widget_class_bind_template_child (widget_class, UnityLauncher, strip);

  gtk_widget_class_set_css_name (widget_class, "unity-launcher");
}

static void
unity_launcher_init (UnityLauncher *self)
{
  self->settings = g_settings_new (UNITY_LAUNCHER_SCHEMA);
  self->ph_index = -1;

  gtk_widget_init_template (GTK_WIDGET (self));

  sync_dash_button (self);
  g_signal_connect_object (self->settings, "changed::" UNITY_LAUNCHER_KEY_LAUNCHER_ICON_SIZE,
                           G_CALLBACK (sync_dash_button), self, G_CONNECT_SWAPPED);

  self->apps = unity_app_list_new ();
  g_signal_connect_object (self->apps, "items-changed",
                           G_CALLBACK (on_model_items_changed), self, 0);
  on_model_items_changed (G_LIST_MODEL (self->apps), 0, 0,
                          g_list_model_get_n_items (G_LIST_MODEL (self->apps)), self);

  install_drop_target (self);
  install_action_group (self);

  g_signal_connect_object (self->settings, UNITY_LAUNCHER_CHANGED_PINNED_APPS,
                           G_CALLBACK (sync_pinned_apps), self, G_CONNECT_SWAPPED);
  sync_pinned_apps (self);
}

static void
ensure_style (void)
{
  static gsize done = 0;

  if (g_once_init_enter (&done))
    {
      g_autoptr (GtkCssProvider) provider = gtk_css_provider_new ();

      gtk_css_provider_load_from_resource (provider, "/org/unity/launcher/launcher.css");
      gtk_style_context_add_provider_for_display (
        gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

      g_once_init_leave (&done, 1);
    }
}

/**
 * unity_launcher_new:
 * @app: the application the window belongs to.
 *
 * Creates a new launcher window and registers the launcher stylesheet, which
 * also styles the application grid.
 *
 * Returns: (transfer full): a new UnityLauncher
 */
UnityLauncher *
unity_launcher_new (GtkApplication *app)
{
  ensure_style ();
  return g_object_new (UNITY_TYPE_LAUNCHER, "application", app, NULL);
}
