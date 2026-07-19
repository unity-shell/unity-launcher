#include "components/unity-tile.h"

#define ICON_SIZE_MIN     16
#define ICON_SIZE_MAX     256
#define ICON_SIZE_DEFAULT 48

typedef struct
{
  GtkBox         *box;
  GtkImage       *image;
  GtkPopoverMenu *popover;
  gint            icon_size;
  GtkPositionType menu_position;
} UnityTilePrivate;

/**
 * UnityTile:
 *
 * Abstract scaffolding for a launcher or dash tile.
 *
 * A #UnityTile is a flat button holding a centred icon plus whatever the
 * subclass adds to the box (a running dot, a label). The icon is sized by the
 * #UnityTile:icon-size property. A secondary click raises a context menu that
 * the subclass fills through the populate_menu vfunc.
 *
 * The base knows no app model. Subclasses bind their own data source (a
 * UnityAppEntry, an AstalApps.Application) and drive the tile with
 * unity_tile_set_gicon(), unity_tile_set_running() and unity_tile_set_active().
 */
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (UnityTile, unity_tile, GTK_TYPE_BUTTON)

typedef enum
{
  PROP_ICON_SIZE = 1,
} UnityTileProperty;
static GParamSpec *properties[PROP_ICON_SIZE + 1];

static void
present_menu (UnityTile *self)
{
  UnityTilePrivate       *priv = unity_tile_get_instance_private (self);
  UnityTileClass         *klass = UNITY_TILE_GET_CLASS (self);

  if (klass->populate_menu == NULL)
    return;

  g_autoptr (GMenu) menu = g_menu_new ();
  klass->populate_menu (self, menu);
  if (g_menu_model_get_n_items (G_MENU_MODEL (menu)) == 0)
    return;

  if (priv->popover == NULL)
    {
      priv->popover = GTK_POPOVER_MENU (gtk_popover_menu_new_from_model (NULL));
      gtk_widget_set_parent (GTK_WIDGET (priv->popover), GTK_WIDGET (self));
      gtk_popover_set_has_arrow (GTK_POPOVER (priv->popover), FALSE);
      gtk_popover_set_position (GTK_POPOVER (priv->popover), priv->menu_position);
      gtk_widget_add_css_class (GTK_WIDGET (priv->popover), "body");
    }
  gtk_popover_menu_set_menu_model (priv->popover, G_MENU_MODEL (menu));

  GdkRectangle rect = {
    0, 0, gtk_widget_get_width (GTK_WIDGET (self)), gtk_widget_get_height (GTK_WIDGET (self)),
  };
  gtk_popover_set_pointing_to (GTK_POPOVER (priv->popover), &rect);
  gtk_popover_popup (GTK_POPOVER (priv->popover));
}

static void
on_secondary_pressed (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
                      gpointer user_data)
{
  (void) n_press; (void) x; (void) y;
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  present_menu (UNITY_TILE (user_data));
}

/**
 * unity_tile_get_box:
 * @self: a #UnityTile
 *
 * Gets the tile's content box, the vertical box holding the icon. Subclasses
 * append their own widgets to it.
 *
 * Returns: (transfer none): the tile's #GtkBox
 */
GtkBox *
unity_tile_get_box (UnityTile *self)
{
  UnityTilePrivate *priv;
  g_return_val_if_fail (UNITY_IS_TILE (self), NULL);
  priv = unity_tile_get_instance_private (self);
  return priv->box;
}

/**
 * unity_tile_get_icon_size:
 * @self: a #UnityTile
 *
 * Gets the pixel size of the tile icon.
 *
 * Returns: the icon size, in pixels
 */
gint
unity_tile_get_icon_size (UnityTile *self)
{
  UnityTilePrivate *priv;
  g_return_val_if_fail (UNITY_IS_TILE (self), ICON_SIZE_DEFAULT);
  priv = unity_tile_get_instance_private (self);
  return priv->icon_size;
}

/**
 * unity_tile_set_gicon:
 * @self: a #UnityTile
 * @icon: (nullable): the icon to show, or %NULL for a fallback
 *
 * Sets the tile icon. When @icon is %NULL a generic executable icon is shown.
 */
void
unity_tile_set_gicon (UnityTile *self, GIcon *icon)
{
  g_return_if_fail (UNITY_IS_TILE (self));
  UnityTilePrivate *priv = unity_tile_get_instance_private (self);
  if (icon != NULL)
    gtk_image_set_from_gicon (priv->image, icon);
  else
    gtk_image_set_from_icon_name (priv->image, "application-x-executable-symbolic");
}

/**
 * unity_tile_set_running:
 * @self: a #UnityTile
 * @running: whether the tile's app is running
 *
 * Toggles the "running" style class on the tile.
 */
void
unity_tile_set_running (UnityTile *self, gboolean running)
{
  g_return_if_fail (UNITY_IS_TILE (self));
  if (running) gtk_widget_add_css_class    (GTK_WIDGET (self), "running");
  else         gtk_widget_remove_css_class (GTK_WIDGET (self), "running");
}

/**
 * unity_tile_set_active:
 * @self: a #UnityTile
 * @active: whether the tile's app is the active window
 *
 * Toggles the "active" style class on the tile.
 */
void
unity_tile_set_active (UnityTile *self, gboolean active)
{
  g_return_if_fail (UNITY_IS_TILE (self));
  if (active) gtk_widget_add_css_class    (GTK_WIDGET (self), "active");
  else        gtk_widget_remove_css_class (GTK_WIDGET (self), "active");
}

/**
 * unity_tile_set_menu_position:
 * @self: a #UnityTile
 * @position: where the context menu should open relative to the tile
 *
 * Sets the side on which the secondary-click context menu opens.
 */
void
unity_tile_set_menu_position (UnityTile *self, GtkPositionType position)
{
  g_return_if_fail (UNITY_IS_TILE (self));
  UnityTilePrivate *priv = unity_tile_get_instance_private (self);
  priv->menu_position = position;
  if (priv->popover != NULL)
    gtk_popover_set_position (GTK_POPOVER (priv->popover), position);
}

static void
unity_tile_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  UnityTilePrivate *priv = unity_tile_get_instance_private (UNITY_TILE (object));
  switch ((UnityTileProperty) id)
    {
    case PROP_ICON_SIZE: g_value_set_int (value, priv->icon_size); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
unity_tile_set_property (GObject *object, guint id, const GValue *value, GParamSpec *pspec)
{
  UnityTilePrivate *priv = unity_tile_get_instance_private (UNITY_TILE (object));
  switch ((UnityTileProperty) id)
    {
    case PROP_ICON_SIZE:
      {
        gint n = CLAMP (g_value_get_int (value), ICON_SIZE_MIN, ICON_SIZE_MAX);
        if (priv->icon_size != n)
          {
            priv->icon_size = n;
            gtk_image_set_pixel_size (priv->image, n);
            g_object_notify_by_pspec (object, pspec);
          }
        break;
      }
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
unity_tile_dispose (GObject *object)
{
  UnityTilePrivate *priv = unity_tile_get_instance_private (UNITY_TILE (object));

  if (priv->popover != NULL)
    {
      GtkWidget *popover = GTK_WIDGET (priv->popover);
      priv->popover = NULL;
      gtk_widget_unparent (popover);
    }

  G_OBJECT_CLASS (unity_tile_parent_class)->dispose (object);
}

static void
unity_tile_class_init (UnityTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = unity_tile_dispose;
  object_class->get_property = unity_tile_get_property;
  object_class->set_property = unity_tile_set_property;

  properties[PROP_ICON_SIZE] = g_param_spec_int (
    "icon-size", NULL, NULL, ICON_SIZE_MIN, ICON_SIZE_MAX, ICON_SIZE_DEFAULT,
    G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
unity_tile_init (UnityTile *self)
{
  UnityTilePrivate *priv = unity_tile_get_instance_private (self);

  priv->icon_size     = ICON_SIZE_DEFAULT;
  priv->menu_position = GTK_POS_BOTTOM;

  gtk_widget_add_css_class (GTK_WIDGET (self), "flat");

  priv->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 1));
  gtk_widget_set_halign (GTK_WIDGET (priv->box), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (priv->box), GTK_ALIGN_CENTER);

  priv->image = GTK_IMAGE (gtk_image_new ());
  gtk_widget_set_halign (GTK_WIDGET (priv->image), GTK_ALIGN_CENTER);
  gtk_image_set_pixel_size (priv->image, priv->icon_size);
  gtk_widget_add_css_class (GTK_WIDGET (priv->image), "icon-dropshadow");
  gtk_widget_add_css_class (GTK_WIDGET (priv->image), "lowres-icon");
  gtk_box_append (priv->box, GTK_WIDGET (priv->image));

  gtk_button_set_child (GTK_BUTTON (self), GTK_WIDGET (priv->box));

  GtkGesture *gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (on_secondary_pressed), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}
