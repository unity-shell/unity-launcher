#include "dash/unity-dash-search-controller.h"

#include <gdk/gdkkeysyms.h>

#include "dash/unity-dash-search.h"

#define SEARCH_DEBOUNCE_MS 150

#define PAGE_APPS   "apps"
#define PAGE_SEARCH "search"

struct _UnityDashSearchController
{
  GObject         parent_instance;

  GtkSearchEntry  *entry;
  AdwViewStack    *stack;
  UnityDashSearch *search_page;

  guint           debounce_id;
};

/**
 * UnityDashSearchController:
 *
 * Drives the dash's search interaction.
 *
 * It debounces the search entry into queries on the search page, switches the
 * view stack between the apps and search pages as the query changes, and routes
 * Enter to launch the highlighted match and Down to move focus into the results.
 * It owns no widgets. The window passes in the ones it already holds.
 */
G_DEFINE_FINAL_TYPE (UnityDashSearchController, unity_dash_search_controller, G_TYPE_OBJECT)

static gboolean
do_search (gpointer user_data)
{
  UnityDashSearchController *self = user_data;
  self->debounce_id = 0;
  unity_dash_search_run (self->search_page,
                             gtk_editable_get_text (GTK_EDITABLE (self->entry)));
  return G_SOURCE_REMOVE;
}

static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
  UnityDashSearchController *self = user_data;
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  gboolean empty = (text == NULL || *text == '\0');

  g_clear_handle_id (&self->debounce_id, g_source_remove);

  if (empty)
    {
      unity_dash_search_reset (self->search_page);
      adw_view_stack_set_visible_child_name (self->stack, PAGE_APPS);
      return;
    }

  adw_view_stack_set_visible_child_name (self->stack, PAGE_SEARCH);
  self->debounce_id = g_timeout_add (SEARCH_DEBOUNCE_MS, do_search, self);
}

static void
on_entry_activate (GtkSearchEntry *entry, gpointer user_data)
{
  (void) entry;
  UnityDashSearchController *self = user_data;
  if (g_strcmp0 (adw_view_stack_get_visible_child_name (self->stack), PAGE_SEARCH) == 0)
    unity_dash_search_activate_selected (self->search_page);
}

static gboolean
on_entry_key (GtkEventControllerKey *key, guint keyval, guint keycode,
              GdkModifierType state, gpointer user_data)
{
  (void) key; (void) keycode; (void) state;
  UnityDashSearchController *self = user_data;

  if (keyval == GDK_KEY_Down &&
      g_strcmp0 (adw_view_stack_get_visible_child_name (self->stack), PAGE_SEARCH) == 0)
    {
      unity_dash_search_focus_results (self->search_page);
      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

/**
 * unity_dash_search_controller_reset:
 * @self: a UnityDashSearchController
 *
 * Returns to the resting state. Cancels any pending query, clears the results
 * and the entry, and shows the apps page.
 */
void
unity_dash_search_controller_reset (UnityDashSearchController *self)
{
  g_return_if_fail (UNITY_IS_DASH_SEARCH_CONTROLLER (self));

  g_clear_handle_id (&self->debounce_id, g_source_remove);
  unity_dash_search_reset (self->search_page);
  adw_view_stack_set_visible_child_name (self->stack, PAGE_APPS);
  gtk_editable_set_text (GTK_EDITABLE (self->entry), "");
}

/**
 * unity_dash_search_controller_new:
 * @entry: the search entry to drive queries from.
 * @stack: the view stack switched between the apps and search pages.
 * @search_page: the search results page.
 *
 * Creates a controller wiring @entry to @stack and the search page. The widgets
 * are borrowed and stay owned by the caller.
 *
 * Returns: (transfer full): a new UnityDashSearchController
 */
UnityDashSearchController *
unity_dash_search_controller_new (GtkSearchEntry *entry, AdwViewStack *stack,
                                  UnityDashSearch *search_page)
{
  UnityDashSearchController *self = g_object_new (UNITY_TYPE_DASH_SEARCH_CONTROLLER, NULL);
  self->entry       = entry;
  self->stack       = stack;
  self->search_page = search_page;

  g_signal_connect_object (entry, "search-changed", G_CALLBACK (on_search_changed), self, 0);
  g_signal_connect_object (entry, "activate", G_CALLBACK (on_entry_activate), self, 0);

  GtkEventController *entry_key = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (entry_key, GTK_PHASE_CAPTURE);
  g_signal_connect_object (entry_key, "key-pressed", G_CALLBACK (on_entry_key), self, 0);
  gtk_widget_add_controller (GTK_WIDGET (entry), entry_key);

  return self;
}

static void
unity_dash_search_controller_dispose (GObject *object)
{
  UnityDashSearchController *self = UNITY_DASH_SEARCH_CONTROLLER (object);
  g_clear_handle_id (&self->debounce_id, g_source_remove);
  G_OBJECT_CLASS (unity_dash_search_controller_parent_class)->dispose (object);
}

static void
unity_dash_search_controller_class_init (UnityDashSearchControllerClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = unity_dash_search_controller_dispose;
}

static void
unity_dash_search_controller_init (UnityDashSearchController *self)
{
  (void) self;
}
