#pragma once

#define UNITY_LAUNCHER_SCHEMA                   "org.unity.launcher"

#define UNITY_LAUNCHER_KEY_PINNED_APPS          "pinned-apps"
#define UNITY_LAUNCHER_KEY_LAUNCHER_ICON_SIZE   "launcher-icon-size"
#define UNITY_LAUNCHER_KEY_DASH_ICON_SIZE       "dash-icon-size"
#define UNITY_LAUNCHER_KEY_DASH_MAXIMIZED       "dash-maximized"

#define UNITY_LAUNCHER_CHANGED_PINNED_APPS "changed::" UNITY_LAUNCHER_KEY_PINNED_APPS

#define UNITY_LAUNCHER_ACTION_GROUP             "launcher"
#define UNITY_LAUNCHER_ACTION_PIN_TOGGLE UNITY_LAUNCHER_ACTION_GROUP ".pin-toggle"
#define UNITY_LAUNCHER_ACTION_QUIT       UNITY_LAUNCHER_ACTION_GROUP ".quit"
#define UNITY_LAUNCHER_ACTION_REORDER    UNITY_LAUNCHER_ACTION_GROUP ".reorder-pinned"

#define UNITY_LAUNCHER_ACTION_SPREAD_APP "wayfire.spread-app"
