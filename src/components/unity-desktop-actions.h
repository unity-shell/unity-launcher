#pragma once

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

G_BEGIN_DECLS

void unity_desktop_actions_append (GMenu           *menu,
                                   GDesktopAppInfo *info,
                                   const gchar     *action_name);

G_END_DECLS
