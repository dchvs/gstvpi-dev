# Define source code
gst_lib_sources = [
  'gstcudaallocator.c',
  'gstcudabufferpool.c',
  'gstcudameta.c',
  'gstvpi.c',
  'gstvpibufferpool.c',
  'gstvpifilter.c',
  'gstvpimeta.c'
]

gst_lib_headers = [
  'gstcudaallocator.h',
  'gstcudabufferpool.h',
  'gstcudameta.h',
  'gstvpi.h',
  'gstvpibufferpool.c',
  'gstvpifilter.h',
  'gstvpimeta.h'
]

# Evaluation option
if get_option('eval').enabled()
  gst_lib_sources += ['eval.c']
  gst_lib_headers += ['eval.h']
endif

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
