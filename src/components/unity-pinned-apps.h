#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

void unity_pinned_apps_toggle (GSettings *settings, const gchar *app_id);

G_END_DECLS
