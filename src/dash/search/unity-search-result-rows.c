#include "dash/search/unity-search-result-rows.h"

#include <adwaita.h>

/**
 * UnitySearchResultRows:
 *
 * A provider's search results, rendered like Bazaar's link rows: a title over a
 * two-column grid of activatable rows. Each row shows the result's icon, name
 * and description, plus a Bazaar-style suffix — an external-link cue, preceded
 * by a copy button when the result carries clipboard text (e.g. a calculator
 * answer). Both columns share one width so every provider's rows line up.
 *
 * It emits UnitySearchResultRows::activated when a row is launched, so the
 * search page can close the dash.
 */
struct _UnitySearchResultRows
{
  GtkBox               parent_instance;

  UnitySearchProvider *provider;  /* for the header's launch-in-app action */
  GStrv                terms;     /* the query terms handed to LaunchSearch */
};

G_DEFINE_FINAL_TYPE (UnitySearchResultRows, unity_search_result_rows, GTK_TYPE_BOX)

enum { SIG_ACTIVATED, N_SIGNALS };
static guint signals[N_SIGNALS];

static void
on_row_activated (AdwActionRow *row, gpointer user_data)
{
  UnitySearchResultRows *self   = user_data;
  UnitySearchResult     *result = g_object_get_data (G_OBJECT (row), "result");
  if (result != NULL)
    unity_search_result_activate (result, GDK_CURRENT_TIME);
  g_signal_emit (self, signals[SIG_ACTIVATED], 0);
}

static void
on_copy_clicked (GtkButton *button, gpointer user_data)
{
  (void) user_data;
  const gchar *text = g_object_get_data (G_OBJECT (button), "copy-text");
  if (text == NULL || *text == '\0')
    return;
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), text);
}

/* The row suffix. By default just an external-link cue; results that carry
 * clipboard text also get a flat copy button and a separator ahead of it. */
static GtkWidget *
build_suffix (UnitySearchResultRows *self, const gchar *copy_text)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

  if (copy_text != NULL && *copy_text != '\0')
    {
      GtkWidget *copy = gtk_button_new_from_icon_name ("edit-copy-symbolic");
      gtk_widget_set_tooltip_text (copy, "Copy");
      gtk_button_set_has_frame (GTK_BUTTON (copy), FALSE);
      gtk_widget_set_valign (copy, GTK_ALIGN_CENTER);
      g_object_set_data_full (G_OBJECT (copy), "copy-text", g_strdup (copy_text), g_free);
      g_signal_connect (copy, "clicked", G_CALLBACK (on_copy_clicked), self);

      GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
      gtk_widget_set_margin_top (sep, 6);
      gtk_widget_set_margin_bottom (sep, 6);

      gtk_box_append (GTK_BOX (box), copy);
      gtk_box_append (GTK_BOX (box), sep);
    }

  GtkWidget *ext = gtk_image_new_from_icon_name ("external-link-symbolic");
  gtk_widget_set_margin_start (ext, 8);
  gtk_widget_set_margin_end (ext, 4);
  gtk_box_append (GTK_BOX (box), ext);

  return box;
}

static GtkWidget *
create_row (UnitySearchResultRows *self, UnitySearchResult *r)
{
  AdwActionRow *row = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 unity_search_result_get_name (r));
  const gchar *desc = unity_search_result_get_description (r);
  if (desc != NULL && *desc != '\0')
    {
      adw_action_row_set_subtitle (row, desc);
      adw_action_row_set_subtitle_lines (row, 1);
    }

  GIcon *gicon = unity_search_result_get_gicon (r);
  if (gicon != NULL)
    adw_action_row_add_prefix (row, gtk_image_new_from_gicon (gicon));
  adw_action_row_add_suffix (
    row, build_suffix (self, unity_search_result_get_clipboard_text (r)));

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  g_object_set_data_full (G_OBJECT (row), "result", g_object_ref (r), g_object_unref);
  g_signal_connect (row, "activated", G_CALLBACK (on_row_activated), self);

  return GTK_WIDGET (row);
}

/* A .boxed-list column that holds result rows. */
static GtkListBox *
create_column (void)
{
  GtkListBox *list = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (list, GTK_SELECTION_NONE);
  gtk_widget_add_css_class (GTK_WIDGET (list), "boxed-list");
  gtk_widget_set_valign (GTK_WIDGET (list), GTK_ALIGN_START);
  gtk_widget_set_hexpand (GTK_WIDGET (list), TRUE);
  return list;
}

/* The header-suffix button is the provider's "open in <app>" action: it calls
 * LaunchSearch and closes the dash, mirroring GNOME Shell. */
static void
on_launch_clicked (GtkButton *button, gpointer user_data)
{
  (void) button;
  UnitySearchResultRows *self = user_data;
  if (self->provider != NULL)
    unity_search_provider_launch_search (
      self->provider, (const gchar *const *) self->terms, GDK_CURRENT_TIME);
  g_signal_emit (self, signals[SIG_ACTIVATED], 0);
}

static void
populate (UnitySearchResultRows *self, GPtrArray *results)
{
  /* AdwPreferencesGroup gives the provider title; its header-suffix carries the
   * "Open in <app>" launch action. The group's own list stays empty (and so
   * hidden); the results sit below it. */
  const gchar *name = unity_search_provider_get_name (self->provider);

  AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_preferences_group_set_title (group, name);

  g_autofree gchar *label = g_strdup_printf ("Open in %s", name);
  GtkWidget *launch = gtk_button_new_with_label (label);
  gtk_widget_add_css_class (launch, "flat");
  gtk_widget_add_css_class (launch, "body");
  gtk_widget_set_valign (launch, GTK_ALIGN_CENTER);
  g_signal_connect (launch, "clicked", G_CALLBACK (on_launch_clicked), self);
  adw_preferences_group_set_header_suffix (group, launch);

  gtk_box_append (GTK_BOX (self), GTK_WIDGET (group));

  guint n = results != NULL ? results->len : 0;

  /* One result is a normal full-width row; several split into two columns, the
   * first taking the extra on odd counts (mirroring Bazaar's dual-column list). */
  if (n <= 1)
    {
      GtkListBox *list = create_column ();
      for (guint i = 0; i < n; i++)
        gtk_list_box_append (list, create_row (self, results->pdata[i]));
      gtk_box_append (GTK_BOX (self), GTK_WIDGET (list));
      return;
    }

  guint left = (n + 1) / 2;

  GtkWidget *columns = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_set_homogeneous (GTK_BOX (columns), TRUE);
  gtk_widget_set_hexpand (columns, TRUE);

  GtkListBox *left_list = create_column ();
  for (guint i = 0; i < left; i++)
    gtk_list_box_append (left_list, create_row (self, results->pdata[i]));
  gtk_box_append (GTK_BOX (columns), GTK_WIDGET (left_list));

  GtkListBox *right_list = create_column ();
  for (guint i = left; i < n; i++)
    gtk_list_box_append (right_list, create_row (self, results->pdata[i]));
  gtk_box_append (GTK_BOX (columns), GTK_WIDGET (right_list));

  gtk_box_append (GTK_BOX (self), columns);
}

/**
 * unity_search_result_rows_new:
 * @provider: the provider whose results these are; drives the header's launch.
 * @results: (element-type UnitySearchResult): the results to render.
 *
 * Creates a group of search-result rows headed by the provider's launch action.
 *
 * Returns: (transfer full): a new UnitySearchResultRows.
 */
GtkWidget *
unity_search_result_rows_new (UnitySearchProvider *provider, GPtrArray *results)
{
  UnitySearchResultRows *self = g_object_new (UNITY_TYPE_SEARCH_RESULT_ROWS, NULL);
  self->provider = provider ? g_object_ref (provider) : NULL;
  if (results != NULL && results->len > 0)
    self->terms = g_strdupv (
      (gchar **) unity_search_result_get_terms (results->pdata[0]));
  populate (self, results);
  return GTK_WIDGET (self);
}

static void
unity_search_result_rows_dispose (GObject *object)
{
  UnitySearchResultRows *self = UNITY_SEARCH_RESULT_ROWS (object);
  g_clear_object (&self->provider);
  g_clear_pointer (&self->terms, g_strfreev);
  G_OBJECT_CLASS (unity_search_result_rows_parent_class)->dispose (object);
}

static void
unity_search_result_rows_class_init (UnitySearchResultRowsClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = unity_search_result_rows_dispose;

  signals[SIG_ACTIVATED] = g_signal_new (
    "activated", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
    0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
unity_search_result_rows_init (UnitySearchResultRows *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 6);
}
