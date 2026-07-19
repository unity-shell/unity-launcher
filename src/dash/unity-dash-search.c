#include "dash/unity-dash-search.h"

#include <astal-apps.h>

#include "components/unity-dash-tile.h"
#include "dash/search/unity-search.h"
#include "unity-launcher-defs.h"

#define SEARCH_ROW_COLS  5
#define SEARCH_TILE_BAND 52

struct _UnityDashSearch
{
  AdwBin               parent_instance;

  AstalAppsApps       *catalog;
  GSettings           *settings;
  UnitySearch         *search;

  GtkBox              *groups;
  AdwPreferencesGroup *apps_group;
  GtkToggleButton     *show_more;
  GtkFlowBox          *flow_first;
  GtkFlowBox          *flow_rest;
  GtkRevealer         *revealer;
  AdwStatusPage       *placeholder;

  GPtrArray           *provider_groups;
};

/**
 * UnityDashSearch:
 *
 * The dash's search page.
 *
 * A matching-apps group plus a group per search provider. It owns the search
 * orchestration and the parent drives it. When a query matches nothing it shows
 * an inline "no results" placeholder rather than a separate page.
 *
 * It emits UnityDashSearch::activated when a tile or a result is launched, so
 * the parent should close.
 */
G_DEFINE_FINAL_TYPE (UnityDashSearch, unity_dash_search, ADW_TYPE_BIN)

enum { SIG_ACTIVATED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void
update_placeholder (UnityDashSearch *self)
{
  gboolean nothing = !gtk_widget_get_visible (GTK_WIDGET (self->apps_group)) &&
                     self->provider_groups->len == 0;
  gtk_widget_set_visible (GTK_WIDGET (self->placeholder), nothing);
}

static void
on_tile_activated (UnityDashTile *tile, gpointer user_data)
{
  (void) tile;
  g_signal_emit (UNITY_DASH_SEARCH (user_data), signals[SIG_ACTIVATED], 0);
}

static GtkFlowBoxChild *
selected_child (UnityDashSearch *self)
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

static guint
fill_apps (UnityDashSearch *self, const gchar *query)
{
  flow_clear (self->flow_first);
  flow_clear (self->flow_rest);
  gtk_toggle_button_set_active (self->show_more, FALSE);
  gtk_revealer_set_reveal_child (self->revealer, FALSE);

  gint side = g_settings_get_int (self->settings, UNITY_LAUNCHER_KEY_DASH_ICON_SIZE)
              + SEARCH_TILE_BAND;

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
      gtk_flow_box_append (
        i < SEARCH_ROW_COLS ? self->flow_first : self->flow_rest, tile);
    }
  g_list_free (apps);

  gtk_widget_set_visible (GTK_WIDGET (self->apps_group), i > 0);
  gtk_widget_set_visible (GTK_WIDGET (self->show_more), i > SEARCH_ROW_COLS);
  if (i > 0)
    {
      gtk_flow_box_select_child (
        self->flow_first,
        gtk_flow_box_get_child_at_index (self->flow_first, 0));
    }
  return i;
}

static void
on_show_more_toggled (GtkToggleButton *button, gpointer user_data)
{
  UnityDashSearch *self = user_data;
  gboolean more = gtk_toggle_button_get_active (button);
  gtk_revealer_set_reveal_child (self->revealer, more);
  gtk_button_set_label (GTK_BUTTON (button), more ? "Show Less" : "Show More");
}

static void
on_result_row_activated (AdwActionRow *row, gpointer user_data)
{
  UnityDashSearch *self   = user_data;
  UnitySearchResult  *result = g_object_get_data (G_OBJECT (row), "result");
  if (result != NULL)
    unity_search_result_activate (result, GDK_CURRENT_TIME);
  g_signal_emit (self, signals[SIG_ACTIVATED], 0);
}

static void
on_provider_results (UnitySearch *search, UnitySearchProvider *provider,
                     GPtrArray *results, gpointer user_data)
{
  (void) search;
  UnityDashSearch *self = user_data;

  AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_preferences_group_set_title (group, unity_search_provider_get_name (provider));

  for (guint i = 0; i < results->len; i++)
    {
      UnitySearchResult *r = results->pdata[i];
      AdwActionRow *row = ADW_ACTION_ROW (adw_action_row_new ());
      adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                     unity_search_result_get_name (r));
      const gchar *desc = unity_search_result_get_description (r);
      if (desc != NULL && *desc != '\0')
        adw_action_row_set_subtitle (row, desc);

      GIcon *gicon = unity_search_result_get_gicon (r);
      if (gicon != NULL)
        adw_action_row_add_prefix (row, gtk_image_new_from_gicon (gicon));
      adw_action_row_add_suffix (row, gtk_image_new_from_icon_name ("go-next-symbolic"));

      gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
      g_object_set_data_full (G_OBJECT (row), "result", g_object_ref (r), g_object_unref);
      g_signal_connect (row, "activated", G_CALLBACK (on_result_row_activated), self);

      adw_preferences_group_add (group, GTK_WIDGET (row));
    }

  gtk_box_append (self->groups, GTK_WIDGET (group));
  g_ptr_array_add (self->provider_groups, group);
  update_placeholder (self);
}

static void
clear_provider_groups (UnityDashSearch *self)
{
  for (guint i = 0; i < self->provider_groups->len; i++)
    gtk_box_remove (self->groups, self->provider_groups->pdata[i]);
  g_ptr_array_set_size (self->provider_groups, 0);
}

/**
 * unity_dash_search_run:
 * @self: a UnityDashSearch
 * @query: the text to search for.
 *
 * Runs @query. Matching apps are filled synchronously and search providers are
 * queried asynchronously. The top match is highlighted while the entry keeps
 * focus. The "no results" placeholder shows if nothing matches.
 */
void
unity_dash_search_run (UnityDashSearch *self, const gchar *query)
{
  g_return_if_fail (UNITY_IS_DASH_SEARCH (self));

  fill_apps (self, query);
  clear_provider_groups (self);
  update_placeholder (self);
  unity_search_query (self->search, query, 0);
}

/**
 * unity_dash_search_activate_selected:
 * @self: a UnityDashSearch
 *
 * Launches the highlighted default match. Called for Enter while the entry is
 * focused.
 */
void
unity_dash_search_activate_selected (UnityDashSearch *self)
{
  g_return_if_fail (UNITY_IS_DASH_SEARCH (self));
  GtkFlowBoxChild *child = selected_child (self);
  if (child == NULL)
    child = gtk_flow_box_get_child_at_index (self->flow_first, 0);
  if (child != NULL)
    gtk_widget_activate (gtk_flow_box_child_get_child (child));
}

/**
 * unity_dash_search_focus_results:
 * @self: a UnityDashSearch
 *
 * Moves keyboard focus into the results. Called for Down from the entry, so the
 * arrow keys then navigate the results natively.
 */
void
unity_dash_search_focus_results (UnityDashSearch *self)
{
  g_return_if_fail (UNITY_IS_DASH_SEARCH (self));
  gtk_widget_child_focus (GTK_WIDGET (self->flow_first), GTK_DIR_DOWN);
}

/**
 * unity_dash_search_reset:
 * @self: a UnityDashSearch
 *
 * Cancels any in-flight query and clears the results.
 */
void
unity_dash_search_reset (UnityDashSearch *self)
{
  g_return_if_fail (UNITY_IS_DASH_SEARCH (self));
  unity_search_query (self->search, NULL, 0);
  clear_provider_groups (self);
  flow_clear (self->flow_first);
  flow_clear (self->flow_rest);
  gtk_widget_set_visible (GTK_WIDGET (self->placeholder), FALSE);
}

/**
 * unity_dash_search_new:
 *
 * Creates a new search page for the dash.
 *
 * Returns: (transfer full): a new UnityDashSearch
 */
GtkWidget *
unity_dash_search_new (void)
{
  return g_object_new (UNITY_TYPE_DASH_SEARCH, NULL);
}

static void
unity_dash_search_dispose (GObject *object)
{
  UnityDashSearch *self = UNITY_DASH_SEARCH (object);
  g_clear_object (&self->catalog);
  g_clear_object (&self->settings);
  g_clear_pointer (&self->provider_groups, g_ptr_array_unref);
  gtk_widget_dispose_template (GTK_WIDGET (object), UNITY_TYPE_DASH_SEARCH);
  G_OBJECT_CLASS (unity_dash_search_parent_class)->dispose (object);
}

static void
unity_dash_search_class_init (UnityDashSearchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = unity_dash_search_dispose;

  signals[SIG_ACTIVATED] = g_signal_new (
    "activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (
    widget_class, "/org/unity/launcher/dash/unity-dash-search.ui");
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, groups);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, apps_group);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, show_more);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, flow_first);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, flow_rest);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, revealer);
  gtk_widget_class_bind_template_child (widget_class, UnityDashSearch, placeholder);
}

static void
unity_dash_search_init (UnityDashSearch *self)
{
  self->catalog         = astal_apps_apps_new ();
  self->settings        = g_settings_new (UNITY_LAUNCHER_SCHEMA);
  self->search          = unity_search_get_default ();
  self->provider_groups = g_ptr_array_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect (self->flow_first, "child-activated", G_CALLBACK (on_child_activated), self);
  g_signal_connect (self->flow_rest, "child-activated", G_CALLBACK (on_child_activated), self);

  g_signal_connect (self->show_more, "toggled", G_CALLBACK (on_show_more_toggled), self);
  g_signal_connect_object (self->search, "provider-results",
                           G_CALLBACK (on_provider_results), self, 0);
}
