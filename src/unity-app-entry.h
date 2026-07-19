#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define UNITY_TYPE_APP_ENTRY (unity_app_entry_get_type ())

G_DECLARE_FINAL_TYPE (UnityAppEntry, unity_app_entry,
                      UNITY, APP_ENTRY, GObject)

const gchar *unity_app_entry_get_app_id    (UnityAppEntry *self);
GAppInfo    *unity_app_entry_get_app_info  (UnityAppEntry *self);
GListModel  *unity_app_entry_get_toplevels (UnityAppEntry *self);
gboolean     unity_app_entry_get_pinned    (UnityAppEntry *self);
gboolean     unity_app_entry_get_running   (UnityAppEntry *self);
gboolean     unity_app_entry_get_activated (UnityAppEntry *self);

void         unity_app_entry_activate_or_launch (UnityAppEntry *self);
void         unity_app_entry_close_all          (UnityAppEntry *self);

UnityAppEntry *_unity_app_entry_new        (const gchar *app_id,
                                                       GAppInfo    *app_info,
                                                       GListModel  *toplevels);
void                _unity_app_entry_set_pinned (UnityAppEntry *self,
                                                       gboolean            pinned);

G_END_DECLS
