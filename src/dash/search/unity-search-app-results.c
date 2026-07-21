#include "dash/search/unity-search-app-results.h"

#include <astal-apps.h>

#include "components/unity-dash-tile.h"
#include "unity-launcher-defs.h"

#define APP_COLS   5
#define TILE_BAND  52

/**
 * UnitySearchAppResults:
 *
 * The application matches on the dash's search page: a fuzzy-queried grid of
 * app tiles under an "Applications" group, with a Show More toggle that reveals
 * the overflow rows. The first match is selected so Enter launches it.
 *
 * It emits UnitySearchAppResults::activated when a tile is launched, so the
 * search page can close the dash.
 */
struct _UnitySearchAppResults
{
  AdwBin           parent_instance;

  AstalAppsApps   *catalog;
  GSettings       *settings;

  GtkToggleButton *show_more;
  GtkFlowBox      *flow_first;
  GtkFlowBox      *flow_rest;
  GtkRevealer     *revealer;
};

G_DEFINE_FINAL_TYPE (UnitySearchAppResults, unity_search_app_results, ADW_TYPE_BIN)

enum { SIG_ACTIVATED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void
on_tile_activated (UnityDashTile *tile, gpointer user_data)
{
  (void) tile;
  g_signal_emit (UNITY_SEARCH_APP_RESULTS (user_data), signals[SIG_ACTIVATED], 0);
}

static GtkFlowBoxChild *
selected_child (UnitySearchAppResults *self)
{
  g_autoptr (GList) sel = gtk_flow_box_get_selected_children (self->flow_first);
  return sel != NULL ? sel->data : NULL;
}

static void
on_child_activated (GtkFlowBox *box, GtkFlowBoxChild *child, gpointer user_data)
{
  (void) box; (void) user_data;
  GtkWidget *tile = gtk_flow_box_child_get_child (child);
  if (tile != NULL)
    gtk_widget_activate (tile);
}

static void
flow_clear (GtkFlowBox *flow)
{
  GtkWidget *c;
  while ((c = gtk_widget_get_first_child (GTK_WIDGET (flow))) != NULL)
    gtk_flow_box_remove (flow, c);
}

static void
on_show_more_toggled (GtkToggleButton *button, gpointer user_data)
{
  UnitySearchAppResults *self = user_data;
  gboolean more = gtk_toggle_button_get_active (button);
  gtk_revealer_set_reveal_child (self->revealer, more);
  gtk_button_set_label (GTK_BUTTON (button), more ? "Show Less" : "Show More");
}

/**
 * unity_search_app_results_fill:
 * @self: a UnitySearchAppResults
 * @query: the text to match.
 *
 * Fuzzy-queries the catalog and fills the grid, selecting the first match. The
 * widget hides itself when nothing matches.
 *
 * Returns: the number of matches.
 */
guint
unity_search_app_results_fill (UnitySearchAppResults *self, const gchar *query)
{
  g_return_val_if_fail (UNITY_IS_SEARCH_APP_RESULTS (self), 0);

  flow_clear (self->flow_first);
  flow_clear (self->flow_rest);
  gtk_toggle_button_set_active (self->show_more, FALSE);
  gtk_revealer_set_reveal_child (self->revealer, FALSE);

  gint side = g_settings_get_int (self->settings, UNITY_LAUNCHER_KEY_DASH_ICON_SIZE)
              + TILE_BAND;

  GList *apps = astal_apps_apps_fuzzy_query (self->catalog, query);
  guint i = 0;
  for (GList *l = apps; l != NULL; l = l->next, i++)
    {
      GtkWidget *tile = unity_dash_tile_new (l->data);
      gtk_widget_set_size_request (tile, side, side);
      gtk_widget_set_focusable (tile, FALSE);
      g_signal_connect (tile, "activated", G_CALLBACK (on_tile_activated), self);
      g_settings_bind (self->settings, UNITY_LAUNCHER_KEY_DASH_ICON_SIZE,
                       tile, "icon-size", G_SETTINGS_BIND_GET);
      gtk_flow_box_append (i < APP_COLS ? self->flow_first : self->flow_rest, tile);
    }
  g_list_free (apps);

  gtk_widget_set_visible (GTK_WIDGET (self), i > 0);
  gtk_widget_set_visible (GTK_WIDGET (self->show_more), i > APP_COLS);
  if (i > 0)
    gtk_flow_box_select_child (
      self->flow_first, gtk_flow_box_get_child_at_index (self->flow_first, 0));
  return i;
}

/**
 * unity_search_app_results_clear:
 * @self: a UnitySearchAppResults
 *
 * Empties the grid and hides the widget.
 */
void
unity_search_app_results_clear (UnitySearchAppResults *self)
{
  g_return_if_fail (UNITY_IS_SEARCH_APP_RESULTS (self));
  flow_clear (self->flow_first);
  flow_clear (self->flow_rest);
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

/**
 * unity_search_app_results_activate_selected:
 * @self: a UnitySearchAppResults
 *
 * Launches the selected match, falling back to the first. For Enter from the
 * search entry.
 */
void
unity_search_app_results_activate_selected (UnitySearchAppResults *self)
{
  g_return_if_fail (UNITY_IS_SEARCH_APP_RESULTS (self));
  GtkFlowBoxChild *child = selected_child (self);
  if (child == NULL)
    child = gtk_flow_box_get_child_at_index (self->flow_first, 0);
  if (child != NULL)
    gtk_widget_activate (gtk_flow_box_child_get_child (child));
}

/**
 * unity_search_app_results_focus:
 * @self: a UnitySearchAppResults
 *
 * Moves keyboard focus into the grid. For Down from the search entry.
 */
void
unity_search_app_results_focus (UnitySearchAppResults *self)
{
  g_return_if_fail (UNITY_IS_SEARCH_APP_RESULTS (self));
  gtk_widget_child_focus (GTK_WIDGET (self->flow_first), GTK_DIR_DOWN);
}

/**
 * unity_search_app_results_new:
 *
 * Creates the application-matches widget for the search page.
 *
 * Returns: (transfer full): a new UnitySearchAppResults.
 */
GtkWidget *
unity_search_app_results_new (void)
{
  return g_object_new (UNITY_TYPE_SEARCH_APP_RESULTS, NULL);
}

static void
unity_search_app_results_dispose (GObject *object)
{
  UnitySearchAppResults *self = UNITY_SEARCH_APP_RESULTS (object);
  g_clear_object (&self->catalog);
  g_clear_object (&self->settings);
  gtk_widget_dispose_template (GTK_WIDGET (object), UNITY_TYPE_SEARCH_APP_RESULTS);
  G_OBJECT_CLASS (unity_search_app_results_parent_class)->dispose (object);
}

static void
unity_search_app_results_class_init (UnitySearchAppResultsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = unity_search_app_results_dispose;

  signals[SIG_ACTIVATED] = g_signal_new (
    "activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (
    widget_class, "/org/unity/launcher/dash/search/unity-search-app-results.ui");
  gtk_widget_class_bind_template_child (widget_class, UnitySearchAppResults, show_more);
  gtk_widget_class_bind_template_child (widget_class, UnitySearchAppResults, flow_first);
  gtk_widget_class_bind_template_child (widget_class, UnitySearchAppResults, flow_rest);
  gtk_widget_class_bind_template_child (widget_class, UnitySearchAppResults, revealer);
}

static void
unity_search_app_results_init (UnitySearchAppResults *self)
{
  self->catalog  = astal_apps_apps_new ();
  self->settings = g_settings_new (UNITY_LAUNCHER_SCHEMA);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->flow_first, "child-activated", G_CALLBACK (on_child_activated), self);
  g_signal_connect (self->flow_rest,  "child-activated", G_CALLBACK (on_child_activated), self);
  g_signal_connect (self->show_more,  "toggled",         G_CALLBACK (on_show_more_toggled), self);
}
