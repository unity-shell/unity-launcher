#include "dash/unity-dash-apps.h"

#include <astal-apps.h>

#include "components/unity-dash-tile.h"
#include "dash/unity-dash-grid-layout.h"
#include "unity-launcher-defs.h"

struct _UnityDashApps
{
  AdwBin         parent_instance;

  AstalAppsApps *catalog;
  GSettings     *settings;
  GtkBox        *cells;
};

/**
 * UnityDashApps:
 *
 * The dash's browse page.
 *
 * Shows every installed application as a square grid. It emits
 * UnityDashApps::activated when a tile launches an app, so the popup can
 * close.
 */
G_DEFINE_FINAL_TYPE (UnityDashApps, unity_dash_apps, ADW_TYPE_BIN)

enum { SIG_ACTIVATED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void
on_tile_activated (UnityDashTile *tile, gpointer user_data)
{
  (void) tile;
  g_signal_emit (UNITY_DASH_APPS (user_data), signals[SIG_ACTIVATED], 0);
}

static GtkWidget *
make_tile (UnityDashApps *self, AstalAppsApplication *app)
{
  GtkWidget *tile = unity_dash_tile_new (app);
  g_signal_connect (tile, "activated", G_CALLBACK (on_tile_activated), self);
  g_settings_bind (self->settings, UNITY_LAUNCHER_KEY_DASH_ICON_SIZE,
                   tile, "icon-size", G_SETTINGS_BIND_GET);
  return tile;
}

static gint
cmp_by_name (gconstpointer a, gconstpointer b, gpointer user_data)
{
  (void) user_data;
  const gchar *na = astal_apps_application_get_name ((AstalAppsApplication *) a);
  const gchar *nb = astal_apps_application_get_name ((AstalAppsApplication *) b);
  return g_utf8_collate (na ? na : "", nb ? nb : "");
}

static void
fill (UnityDashApps *self)
{
  GList *apps = g_list_sort_with_data (astal_apps_apps_get_list (self->catalog),
                                       cmp_by_name, NULL);

  GtkWidget *c;
  while ((c = gtk_widget_get_first_child (GTK_WIDGET (self->cells))) != NULL)
    gtk_box_remove (self->cells, c);

  for (GList *l = apps; l != NULL; l = l->next)
    gtk_box_append (self->cells, make_tile (self, l->data));
  g_list_free (apps);
}

static void
on_catalog_changed (AstalAppsApps *catalog, GParamSpec *pspec, gpointer user_data)
{
  (void) catalog; (void) pspec;
  fill (user_data);
}

/**
 * unity_dash_apps_new:
 *
 * Creates a new browse page for the dash.
 *
 * Returns: (transfer full): a new UnityDashApps
 */
GtkWidget *
unity_dash_apps_new (void)
{
  return g_object_new (UNITY_TYPE_DASH_APPS, NULL);
}

/**
 * unity_dash_apps_reset:
 * @self: a UnityDashApps
 *
 * Scrolls the grid back to the top, for a fresh (non-restored) open.
 */
void
unity_dash_apps_reset (UnityDashApps *self)
{
  g_return_if_fail (UNITY_IS_DASH_APPS (self));
  GtkWidget *sw = gtk_widget_get_ancestor (GTK_WIDGET (self->cells), GTK_TYPE_SCROLLED_WINDOW);
  if (sw != NULL)
    {
      GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));
      gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
    }
}

static void
unity_dash_apps_dispose (GObject *object)
{
  UnityDashApps *self = UNITY_DASH_APPS (object);
  g_clear_object (&self->catalog);
  g_clear_object (&self->settings);
  gtk_widget_dispose_template (GTK_WIDGET (object), UNITY_TYPE_DASH_APPS);
  G_OBJECT_CLASS (unity_dash_apps_parent_class)->dispose (object);
}

static void
unity_dash_apps_class_init (UnityDashAppsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = unity_dash_apps_dispose;

  signals[SIG_ACTIVATED] = g_signal_new (
    "activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (
    widget_class, "/org/unity/launcher/dash/unity-dash-apps.ui");
  gtk_widget_class_bind_template_child (widget_class, UnityDashApps, cells);
}

static void
unity_dash_apps_init (UnityDashApps *self)
{
  self->catalog  = astal_apps_apps_new ();
  self->settings = g_settings_new (UNITY_LAUNCHER_SCHEMA);

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_set_layout_manager (GTK_WIDGET (self->cells), unity_dash_grid_layout_new ());

  g_signal_connect_object (self->catalog, "notify::list",
                           G_CALLBACK (on_catalog_changed), self, 0);
  fill (self);
}
