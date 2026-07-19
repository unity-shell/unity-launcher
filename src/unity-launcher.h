#pragma once

#include <astal-4.h>

G_BEGIN_DECLS

#define UNITY_TYPE_LAUNCHER (unity_launcher_get_type ())

G_DECLARE_FINAL_TYPE (UnityLauncher, unity_launcher,
                      UNITY, LAUNCHER, AstalWindow)

UnityLauncher *unity_launcher_new (GtkApplication *app);

G_END_DECLS
