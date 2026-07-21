#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * UnityDismissFunc:
 * @user_data: the data passed to unity_dismiss_attach()
 *
 * Callback invoked when the surface should dismiss.
 */
typedef void (*UnityDismissFunc) (gpointer user_data);

void unity_dismiss_attach (GtkWidget        *surface,
                           GtkWidget        *area,
                           GtkWidget        *content,
                           UnityDismissFunc  on_minimize,
                           UnityDismissFunc  on_close,
                           gpointer          user_data);

G_END_DECLS
