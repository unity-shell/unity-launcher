#pragma once

#include <astal-4.h>

G_BEGIN_DECLS

#define UNITY_TYPE_DASH (unity_dash_get_type ())

G_DECLARE_FINAL_TYPE (UnityDash, unity_dash, UNITY, DASH, AstalWindow)

GtkWidget *unity_dash_new   (GtkApplication *app);

void       unity_dash_reset (UnityDash *self);

void       unity_dash_close (UnityDash *self);

void       unity_dash_toggle (UnityDash *self);

G_END_DECLS
