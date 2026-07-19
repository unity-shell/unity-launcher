#include "components/unity-pinned-apps.h"

#include "unity-launcher-defs.h"

/**
 * unity_pinned_apps_toggle:
 * @settings: the launcher GSettings.
 * @app_id: the desktop id to toggle.
 *
 * Toggles @app_id in the launcher's pinned-apps list and writes it back to
 * @settings. The id is removed if already present, otherwise it is appended.
 */
void
unity_pinned_apps_toggle (GSettings *settings, const gchar *app_id)
{
  if (app_id == NULL || *app_id == '\0')
    return;

  g_auto (GStrv)        ids  = g_settings_get_strv (settings, UNITY_LAUNCHER_KEY_PINNED_APPS);
  g_autoptr (GPtrArray) next = g_ptr_array_new_with_free_func (g_free);

  gboolean was_pinned = FALSE;
  for (gchar **p = ids; p && *p; p++)
    {
      if (g_strcmp0 (*p, app_id) == 0) { was_pinned = TRUE; continue; }
      g_ptr_array_add (next, g_strdup (*p));
    }
  if (!was_pinned)
    g_ptr_array_add (next, g_strdup (app_id));
  g_ptr_array_add (next, NULL);

  g_settings_set_strv (settings, UNITY_LAUNCHER_KEY_PINNED_APPS,
                       (const gchar *const *) next->pdata);
}
