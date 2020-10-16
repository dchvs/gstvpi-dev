# Define source code
gst_plugin_sources = [
  'gstvpi.c',
  'gstvpidownload.c',
  'gstvpigaussianfilter.c',
  'gstvpiundistort.c',
  'gstvpiupload.c'
]

gst_plugin_headers = [
  'gstvpidownload.h',
  'gstvpigaussianfilter.h',
  'gstvpiundistort.h',
  'gstvpiupload.h'
]

# Build plugin library
gst_plugin = library('gstvpi',
  gst_plugin_sources,
  c_args : c_args,
  include_directories : [configinc, gstvpiplugin_inc_dir],
  install : true,
  install_dir : plugins_install_dir,
  dependencies : [plugin_deps, gstvpifilter_dep] ,
)
