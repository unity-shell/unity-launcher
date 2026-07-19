#include "dash/unity-dash-grid-layout.h"

#define GRID_GAP 18

struct _UnityDashGridLayout
{
  GtkLayoutManager parent_instance;
};

/**
 * UnityDashGridLayout:
 *
 * A layout manager arranging children in a grid of equal square cells.
 *
 * The cell side is content-driven, the bounding square of the largest child, so
 * there is no magic pixel constant. As many columns fit the allocated width as
 * possible, and the square side is grown to fill the row exactly.
 */
G_DEFINE_FINAL_TYPE (UnityDashGridLayout, unity_dash_grid_layout, GTK_TYPE_LAYOUT_MANAGER)

static gint
cell_min (GtkWidget *widget)
{
  gint side = 1;
  for (GtkWidget *c = gtk_widget_get_first_child (widget);
       c != NULL; c = gtk_widget_get_next_sibling (c))
    {
      if (!gtk_widget_should_layout (c))
        continue;
      gint wnat, hnat;
      gtk_widget_measure (c, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &wnat, NULL, NULL);
      gtk_widget_measure (c, GTK_ORIENTATION_VERTICAL,   -1, NULL, &hnat, NULL, NULL);
      side = MAX (side, MAX (wnat, hnat));
    }
  return side;
}

static guint
count_children (GtkWidget *widget)
{
  guint n = 0;
  for (GtkWidget *c = gtk_widget_get_first_child (widget);
       c != NULL; c = gtk_widget_get_next_sibling (c))
    if (gtk_widget_should_layout (c))
      n++;
  return n;
}

static void
resolve (gint width, gint cell, guint n, gint *out_cols, gint *out_side)
{
  gint cols = MAX (1, (width + GRID_GAP) / (cell + GRID_GAP));
  if (n > 0)
    cols = MIN (cols, (gint) n);

  *out_cols = cols;
  *out_side = (width - (cols - 1) * GRID_GAP) / cols;
}

static GtkSizeRequestMode
unity_dash_grid_layout_get_request_mode (GtkLayoutManager *manager, GtkWidget *widget)
{
  (void) manager; (void) widget;
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
unity_dash_grid_layout_measure (GtkLayoutManager *manager, GtkWidget *widget,
                                 GtkOrientation orientation, gint for_size,
                                 gint *minimum, gint *natural,
                                 gint *minimum_baseline, gint *natural_baseline)
{
  (void) manager;
  gint cell = cell_min (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = cell;
      *natural = cell;
    }
  else
    {
      guint n     = count_children (widget);
      gint  width = (for_size > 0) ? for_size : cell;
      gint  cols, side;
      resolve (width, cell, n, &cols, &side);

      gint rows = MAX (1, (gint) (n + cols - 1) / cols);
      gint height = rows * side + (rows - 1) * GRID_GAP;
      *minimum = height;
      *natural = height;
    }

  if (minimum_baseline) *minimum_baseline = -1;
  if (natural_baseline) *natural_baseline = -1;
}

static void
unity_dash_grid_layout_allocate (GtkLayoutManager *manager, GtkWidget *widget,
                                  gint width, gint height, gint baseline)
{
  (void) manager; (void) height; (void) baseline;
  gint cols, side;
  resolve (width, cell_min (widget), count_children (widget), &cols, &side);

  gint i = 0;
  for (GtkWidget *c = gtk_widget_get_first_child (widget);
       c != NULL; c = gtk_widget_get_next_sibling (c))
    {
      if (!gtk_widget_should_layout (c))
        continue;

      gint col = i % cols;
      gint row = i / cols;
      GtkAllocation a = {
        .x = col * (side + GRID_GAP),
        .y = row * (side + GRID_GAP),
        .width = side,
        .height = side,
      };
      gtk_widget_size_allocate (c, &a, -1);
      i++;
    }
}

static void
unity_dash_grid_layout_class_init (UnityDashGridLayoutClass *klass)
{
  GtkLayoutManagerClass *lm = GTK_LAYOUT_MANAGER_CLASS (klass);
  lm->get_request_mode = unity_dash_grid_layout_get_request_mode;
  lm->measure          = unity_dash_grid_layout_measure;
  lm->allocate         = unity_dash_grid_layout_allocate;
}

static void
unity_dash_grid_layout_init (UnityDashGridLayout *self)
{
  (void) self;
}

/**
 * unity_dash_grid_layout_new:
 *
 * Creates a new square-cell grid layout manager.
 *
 * Returns: (transfer full): a new UnityDashGridLayout
 */
GtkLayoutManager *
unity_dash_grid_layout_new (void)
{
  return g_object_new (UNITY_TYPE_DASH_GRID_LAYOUT, NULL);
}
