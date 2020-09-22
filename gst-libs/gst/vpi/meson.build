# Define source code
gst_lib_sources = [
  'gstvpifilter.c',
]

gst_lib_headers = [
  'gstvpifilter.h',
]

# Build library
gstvpifilter_lib = library('gstvpifilter-@0@'.format(api_version),
  gst_lib_sources,
  c_args : c_args,
  version : api_version,
  include_directories : configinc,
  install : true,
  dependencies : [plugin_deps],
)

plugin_dir = meson.current_build_dir()

pkgconfig.generate(gstvpifilter_lib, install_dir : plugins_pkgconfig_install_dir)

gstvpifilter_dep = declare_dependency(link_with : gstvpifilter_lib)
