#include "components/unity-desktop-actions.h"

/**
 * unity_desktop_actions_append:
 * @menu: the menu to extend.
 * @info: the application whose desktop actions are listed.
 * @action_name: the action each item targets, for example "tile.launch-action".
 *
 * Appends a menu section listing the .desktop actions of @info, each item
 * targeting @action_name with the action id as its target value. Does nothing
 * if @info has no actions.
 */
void
unity_desktop_actions_append (GMenu           *menu,
                              GDesktopAppInfo *info,
                              const gchar     *action_name)
{
  if (info == NULL)
    return;

  const gchar *const *actions = g_desktop_app_info_list_actions (info);
  if (actions == NULL || actions[0] == NULL)
    return;

  g_autoptr (GMenu) section = g_menu_new ();
  for (guint i = 0; actions[i] != NULL; i++)
    {
      g_autofree gchar     *name = g_desktop_app_info_get_action_name (info, actions[i]);
      g_autoptr (GMenuItem) item = g_menu_item_new (name, NULL);
      g_menu_item_set_action_and_target_value (item, action_name,
                                               g_variant_new_string (actions[i]));
      g_menu_append_item (section, item);
    }
  g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
}
