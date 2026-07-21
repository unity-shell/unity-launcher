#include "components/unity-dismiss.h"

#include <gdk/gdkkeysyms.h>

typedef struct
{
  UnityDismissFunc cb;
  gpointer         data;
  GtkWidget       *content;
} DismissCtx;

static gboolean
on_key_pressed (GtkEventControllerKey *key, guint keyval, guint keycode,
                GdkModifierType state, gpointer user_data)
{
  (void) key; (void) keycode;
  DismissCtx *ctx = user_data;

  gboolean escape = (keyval == GDK_KEY_Escape);
  gboolean alt_f4 = (keyval == GDK_KEY_F4) && (state & GDK_ALT_MASK);
  gboolean ctrl_w = (keyval == GDK_KEY_w) && (state & GDK_CONTROL_MASK);
  if (escape || alt_f4 || ctrl_w)
    {
      ctx->cb (ctx->data);
      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

/* The window's close-request (WM close, gtk_window_close(), Alt+F4 routed by the
 * compositor). Its default handler destroys the surface, so dismiss and stop it. */
static gboolean
on_close_request (GtkWindow *window, gpointer user_data)
{
  (void) window;
  DismissCtx *ctx = user_data;
  ctx->cb (ctx->data);
  return GDK_EVENT_STOP;
}

static void
on_area_pressed (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
                 gpointer user_data)
{
  (void) n_press;
  DismissCtx *ctx  = user_data;
  GtkWidget  *area = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  graphene_rect_t bounds;
  if (gtk_widget_compute_bounds (ctx->content, area, &bounds) &&
      graphene_rect_contains_point (&bounds, &GRAPHENE_POINT_INIT ((float) x, (float) y)))
    return;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  ctx->cb (ctx->data);
}

static void
on_focus_leave (GtkEventControllerFocus *focus, gpointer user_data)
{
  (void) focus;
  DismissCtx *ctx = user_data;
  ctx->cb (ctx->data);
}

/**
 * unity_dismiss_attach:
 * @surface: the layer-shell surface to make dismissable
 * @area: the full-surface widget that catches clicks outside @content
 * @content: the content widget clicks inside must not dismiss
 * @on_dismiss: callback invoked when the surface should dismiss
 * @user_data: data passed to @on_dismiss
 *
 * Gives a layer-shell surface GtkPopover-like dismissal, which layer-shell does
 * not provide itself. Pressing Escape, Alt+F4 or Ctrl+W, clicking outside
 * @content (caught on the full-surface @area in the capture phase), keyboard
 * focus leaving the surface, or a window close-request each invoke @on_dismiss.
 */
void
unity_dismiss_attach (GtkWidget *surface, GtkWidget *area, GtkWidget *content,
                      UnityDismissFunc on_dismiss, gpointer user_data)
{
  g_return_if_fail (GTK_IS_WIDGET (surface));
  g_return_if_fail (GTK_IS_WIDGET (area));
  g_return_if_fail (GTK_IS_WIDGET (content));
  g_return_if_fail (on_dismiss != NULL);

  DismissCtx *ctx = g_new0 (DismissCtx, 1);
  ctx->cb      = on_dismiss;
  ctx->data    = user_data;
  ctx->content = content;
  g_object_set_data_full (G_OBJECT (surface), "unity-dismiss", ctx, g_free);

  GtkGesture *click = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click), GTK_PHASE_CAPTURE);
  g_signal_connect (click, "pressed", G_CALLBACK (on_area_pressed), ctx);
  gtk_widget_add_controller (area, GTK_EVENT_CONTROLLER (click));

  GtkEventController *key = gtk_event_controller_key_new ();
  gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
  g_signal_connect (key, "key-pressed", G_CALLBACK (on_key_pressed), ctx);
  gtk_widget_add_controller (surface, key);

  GtkEventController *focus = gtk_event_controller_focus_new ();
  g_signal_connect (focus, "leave", G_CALLBACK (on_focus_leave), ctx);
  gtk_widget_add_controller (surface, focus);

  if (GTK_IS_WINDOW (surface))
    g_signal_connect (surface, "close-request", G_CALLBACK (on_close_request), ctx);
}
