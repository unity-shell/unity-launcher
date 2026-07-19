unity_launcher_deps = [
  dep_gtk,
  dep_adw,
  dep_gio,
  dep_gio_unix,
  dep_graphene,
  dep_wlclient,
  dep_astal4,
  dep_astalapps,
  dep_astalwlr,
]

unity_launcher_sources = files(
  'unity-launcher.c',
  'unity-app-entry.c',
  'unity-app-list.c',
)

subdir('components')
subdir('dash')

unity_launcher_resources = gnome.compile_resources(
  'unity-launcher-resources',
  'unity-launcher.gresource.xml',
  c_name: 'unity_launcher',
)

unity_launcher_lib = library(
  'unity-launcher',
  unity_launcher_sources,
  unity_launcher_resources,
  dependencies: unity_launcher_deps,
  include_directories: include_directories('.'),
  version: meson.project_version(),
  soversion: '0',
  install: true,
)

install_headers(
  'unity-launcher.h',
  'unity-launcher-defs.h',
  'dash/unity-dash.h',
  subdir: 'unity-launcher',
)

unity_launcher_dep = declare_dependency(
  link_with: unity_launcher_lib,
  include_directories: include_directories('.'),
  dependencies: unity_launcher_deps,
)

pkg = import('pkgconfig')
pkg.generate(
  unity_launcher_lib,
  name: 'unity-launcher',
  description: 'Unity Launcher and Dash',
  subdirs: 'unity-launcher',
  requires: [dep_gtk, dep_astal4],
)
