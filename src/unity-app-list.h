#pragma once

#include <gio/gio.h>

#include "unity-app-entry.h"

G_BEGIN_DECLS

#define UNITY_TYPE_APP_LIST (unity_app_list_get_type ())

G_DECLARE_FINAL_TYPE (UnityAppList, unity_app_list, UNITY, APP_LIST, GObject)

UnityAppList  *unity_app_list_new              (void);

void                unity_app_list_set_pinned_app_ids (UnityAppList *self,
                                                             const gchar *const *app_ids);

UnityAppEntry *unity_app_list_get_entry        (UnityAppList *self,
                                                           const gchar       *app_id);

G_END_DECLS
