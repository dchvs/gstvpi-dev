# Define source code
gst_lib_sources = [
  'gstvpifilter.c',
]

gst_lib_headers = [
  'gstvpifilter.h',
]

# Build library
gstvpifilter_lib = library('gstvpi-@0@'.format(api_version),
  gst_lib_sources,
  c_args : c_args,
  version : api_version,
  include_directories : configinc,
  install : true,
  dependencies : [plugin_deps],
)

pkgconfig.generate(gstvpifilter_lib, install_dir : pkgconfig_install_dir,
  description : project_description)
gstvpifilter_dep = declare_dependency(link_with : gstvpifilter_lib)