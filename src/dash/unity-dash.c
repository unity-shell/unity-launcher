#include "dash/unity-dash.h"

#include "dash/unity-dash-apps.h"
#include "dash/unity-dash-search.h"
#include "dash/unity-dash-search-controller.h"
#include "components/unity-dismiss.h"
#include "unity-launcher-defs.h"

#define POPOVER_NUM 2
#define POPOVER_DEN 3

struct _UnityDash
{
  AstalWindow                  parent_instance;

  GSettings                   *settings;
  UnityDashSearchController *search;

  AdwBin          *area;
  AdwToolbarView  *panel;
  GtkSearchEntry  *entry;
  AdwViewStack    *stack;
  UnityDashApps   *apps_page;
  UnityDashSearch *search_page;

  gboolean         fullscreen;
  gboolean         suppress_close;  /* guards the hide/re-present in the maximize toggle */
};

/**
 * UnityDash:
 *
 * The application grid window.
 *
 * A layer-shell overlay that fills the work area so a click outside the panel
 * dismisses it. A GtkPopover cannot span the output or take a fraction of it.
 * The floating panel is a header bar (centred search and window controls) over
 * an AdwViewStack of two page widgets: apps for browsing and search for results.
 * The search page shows an inline placeholder when a query matches nothing.
 *
 * It opens either as a popover (two thirds of the monitor at the top-left
 * corner) or fullscreen. The mode persists through the launcher's
 * dash-maximized setting. The grid dismisses on Escape, on a click or a
 * focus loss outside the panel, or when a page launches something.
 */
G_DEFINE_FINAL_TYPE (UnityDash, unity_dash, ASTAL_TYPE_WINDOW)

static void
apply_layout (UnityDash *self)
{
  GtkWidget *panel = GTK_WIDGET (self->panel);

  if (self->fullscreen)
    {
      gtk_widget_add_css_class (panel, "fullscreen");
      gtk_widget_set_halign  (panel, GTK_ALIGN_FILL);
      gtk_widget_set_valign  (panel, GTK_ALIGN_FILL);
      gtk_widget_set_hexpand (panel, TRUE);
      gtk_widget_set_vexpand (panel, TRUE);
      gtk_widget_set_size_request (panel, -1, -1);
      return;
    }

  gtk_widget_remove_css_class (panel, "fullscreen");
  gtk_widget_set_halign  (panel, GTK_ALIGN_START);
  gtk_widget_set_valign  (panel, GTK_ALIGN_START);
  gtk_widget_set_hexpand (panel, FALSE);
  gtk_widget_set_vexpand (panel, FALSE);

  GdkRectangle geo = { 0, 0, 0, 0 };
  GdkMonitor  *monitor = astal_window_get_current_monitor (ASTAL_WINDOW (self));
  if (monitor != NULL)
    gdk_monitor_get_geometry (monitor, &geo);
  if (geo.width > 0 && geo.height > 0)
    gtk_widget_set_size_request (panel,
                                 geo.width  * POPOVER_NUM / POPOVER_DEN,
                                 geo.height * POPOVER_NUM / POPOVER_DEN);
}

/* Mirror the fullscreen state onto the real window's maximized state so the
 * header bar's native maximize button shows the right icon (maximize vs restore).
 * Only effective while the window is unmapped: gtk_window_maximize() then sets
 * priv->maximized directly and it sticks — the layer-shell surface never gets a
 * competing maximized state back from the compositor. */
static void
sync_window_maximized (UnityDash *self)
{
  if (self->fullscreen)
    gtk_window_maximize (GTK_WINDOW (self));
  else
    gtk_window_unmaximize (GTK_WINDOW (self));
}

/* Show the window, syncing the maximized state first while it is still unmapped
 * so the native maximize button appears with the correct icon. */
static void
present_dash (UnityDash *self)
{
  sync_window_maximized (self);
  gtk_window_present (GTK_WINDOW (self));
}

/* Overrides of GtkWindow's built-in window.* actions, which the AdwHeaderBar
 * native controls target. Installed in class_init, so the stock close, minimize
 * and maximize buttons drive the dash's own behaviour. */
static void
on_close_action (GtkWidget *widget, const char *name, GVariant *param)
{
  (void) name; (void) param;
  unity_dash_close (UNITY_DASH (widget));
}

static void
on_minimize_action (GtkWidget *widget, const char *name, GVariant *param)
{
  (void) name; (void) param;
  gtk_widget_set_visible (widget, FALSE);
}

static void
on_toggle_maximized_action (GtkWidget *widget, const char *name, GVariant *param)
{
  (void) name; (void) param;
  UnityDash *self = UNITY_DASH (widget);

  self->fullscreen = !self->fullscreen;
  apply_layout (self);
  g_settings_set_boolean (self->settings, UNITY_LAUNCHER_KEY_DASH_MAXIMIZED,
                          self->fullscreen);

  /* The native maximize icon only tracks priv->maximized, which is settable
   * only while unmapped. Briefly hide and re-present so the button rebuilds with
   * the right icon; suppress_close stops the hide's focus-leave from dismissing. */
  self->suppress_close = TRUE;
  gtk_widget_set_visible (widget, FALSE);
  present_dash (self);
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
  self->suppress_close = FALSE;
}

static void
on_page_activated (GtkWidget *page, gpointer user_data)
{
  (void) page;
  unity_dash_close (UNITY_DASH (user_data));
}

static void
on_grid_map (GtkWidget *widget, gpointer user_data)
{
  (void) user_data;
  apply_layout (UNITY_DASH (widget));
}

static void
on_dismiss (gpointer user_data)
{
  unity_dash_close (UNITY_DASH (user_data));
}

/**
 * unity_dash_new:
 * @app: the application the window belongs to.
 *
 * Creates a new application grid window.
 *
 * Returns: (transfer full): a new UnityDash
 */
GtkWidget *
unity_dash_new (GtkApplication *app)
{
  return g_object_new (UNITY_TYPE_DASH, "application", app, NULL);
}

/**
 * unity_dash_reset:
 * @self: a UnityDash
 *
 * Resets the grid for a fresh open. Applies the remembered popover or
 * fullscreen mode, clears the query and results back to the apps page, and
 * focuses the search entry.
 */
void
unity_dash_reset (UnityDash *self)
{
  g_return_if_fail (UNITY_IS_DASH (self));

  self->fullscreen = g_settings_get_boolean (
    self->settings, UNITY_LAUNCHER_KEY_DASH_MAXIMIZED);
  apply_layout (self);

  unity_dash_search_controller_reset (self->search);
  unity_dash_apps_reset (self->apps_page);
  gtk_widget_grab_focus (GTK_WIDGET (self->entry));
}

/**
 * unity_dash_close:
 * @self: a UnityDash
 *
 * Hides the window and discards its state, so the next open starts fresh. Does
 * nothing if the window is already hidden.
 */
void
unity_dash_close (UnityDash *self)
{
  g_return_if_fail (UNITY_IS_DASH (self));
  if (self->suppress_close || !gtk_widget_get_visible (GTK_WIDGET (self)))
    return;
  unity_dash_reset (self);
  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

/**
 * unity_dash_toggle:
 * @self: a UnityDash
 *
 * Toggles the grid for the Super key or the launcher icon. A visible grid is
 * minimized, keeping its state. A hidden grid is presented, showing whatever
 * state is held: fresh if the last hide was a close, restored if it was a
 * minimize.
 */
void
unity_dash_toggle (UnityDash *self)
{
  g_return_if_fail (UNITY_IS_DASH (self));

  if (gtk_widget_get_visible (GTK_WIDGET (self)))
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
  else
    present_dash (self);
}

static void
unity_dash_dispose (GObject *object)
{
  UnityDash *self = UNITY_DASH (object);
  g_clear_object (&self->search);
  g_clear_object (&self->settings);
  G_OBJECT_CLASS (unity_dash_parent_class)->dispose (object);
  gtk_widget_dispose_template (GTK_WIDGET (object), UNITY_TYPE_DASH);
}

static void
unity_dash_class_init (UnityDashClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  G_OBJECT_CLASS (klass)->dispose = unity_dash_dispose;

  g_type_ensure (UNITY_TYPE_DASH_APPS);
  g_type_ensure (UNITY_TYPE_DASH_SEARCH);

  gtk_widget_class_set_template_from_resource (
    widget_class, "/org/unity/launcher/dash/unity-dash.ui");
  gtk_widget_class_bind_template_child (widget_class, UnityDash, area);
  gtk_widget_class_bind_template_child (widget_class, UnityDash, panel);
  gtk_widget_class_bind_template_child (widget_class, UnityDash, entry);
  gtk_widget_class_bind_template_child (widget_class, UnityDash, stack);
  gtk_widget_class_bind_template_child (widget_class, UnityDash, apps_page);
  gtk_widget_class_bind_template_child (widget_class, UnityDash, search_page);
  gtk_widget_class_set_css_name (widget_class, "unity-dash");

  /* Override GtkWindow's built-in window controls so the header bar's native
   * close, minimize and maximize buttons drive the dash instead. */
  gtk_widget_class_install_action (widget_class, "window.close",            NULL, on_close_action);
  gtk_widget_class_install_action (widget_class, "window.minimize",         NULL, on_minimize_action);
  gtk_widget_class_install_action (widget_class, "window.toggle-maximized", NULL, on_toggle_maximized_action);
}

static void
unity_dash_init (UnityDash *self)
{
  self->settings = g_settings_new (UNITY_LAUNCHER_SCHEMA);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_search_entry_set_key_capture_widget (self->entry, GTK_WIDGET (self));

  self->search = unity_dash_search_controller_new (
    self->entry, self->stack, self->search_page);

  g_signal_connect (self, "map", G_CALLBACK (on_grid_map), NULL);

  g_signal_connect (self->apps_page,   "activated", G_CALLBACK (on_page_activated), self);
  g_signal_connect (self->search_page, "activated", G_CALLBACK (on_page_activated), self);

  unity_dismiss_attach (GTK_WIDGET (self), GTK_WIDGET (self->area),
                        GTK_WIDGET (self->panel), on_dismiss, self);

  unity_dash_reset (self);
}
