install_data(
  'org.unity.launcher.gschema.xml',
  install_dir: datadir / 'glib-2.0' / 'schemas',
)

gnome.compile_schemas(depend_files: files('org.unity.launcher.gschema.xml'))
gnome.post_install(glib_compile_schemas: true)
