# Define source code
gst_plugin_sources = [
  'gstvpi.c',
  'gstvpiboxfilter.c',
  'gstvpidownload.c',
  'gstvpigaussianfilter.c',
  'gstvpiharrisdetector.c',
  'gstvpiklttracker.c',
  'gstvpioverlay.c',
  'gstvpiundistort.c',
  'gstvpiupload.c',
  'gstvpivideoconvert.c',
  'gstvpiwarp.c'
]

gst_plugin_headers = [
  'gstvpiboxfilter.h',
  'gstvpidownload.h',
  'gstvpigaussianfilter.h',
  'gstvpiharrisdetector.h',
  'gstvpiklttracker.h',
  'gstvpioverlay.h',
  'gstvpiundistort.h',
  'gstvpiupload.h',
  'gstvpivideoconvert.h',
  'gstvpiwarp.h'
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
