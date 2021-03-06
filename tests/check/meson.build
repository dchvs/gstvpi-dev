# Name, condition when to skip the test, extra dependencies and extra files
gst_tests = [
  ['elements/vpiboxfilter', false, [],  [] ],
  ['elements/vpidownload', false, [],  [] ],
  ['elements/vpigaussianfilter', false, [],  [] ],
  ['elements/vpiharrisdetector', false, [],  [] ],
  ['elements/vpiklttracker', false, [],  [] ],
  ['elements/vpiundistort', false, [],  [] ],
  ['elements/vpiupload', false, [],  [] ],
  ['elements/vpivideoconvert', false, [],  [] ],
  ['elements/vpivideoscale', false, [],  [] ],
  ['elements/vpiwarp', false, [],  [] ],
  ['libs/vpifilter', false, [],  [] ]
]

# Add C Definitions for tests
test_defines = [
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
  '-DGST_CHECK_TEST_ENVIRONMENT_BEACON="GST_PLUGIN_LOADING_WHITELIST"',
  '-DGST_USE_UNSTABLE_API',
]

# Define plugins path
plugins_dir = gst_dep.get_pkgconfig_variable('pluginsdir')

# Create environment object to store information about the environment
# variables set during tests.
# Define constant enviroment variable
env = environment()
env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
env.set('CK_DEFAULT_TIMEOUT', '120')
env.set('GST_PLUGIN_LOADING_WHITELIST', 'gstreamer', 'gst-plugins-base',
        'gst-vpi@' + meson.build_root())
env.set('GST_PLUGIN_PATH_1_0', [meson.build_root()] + [plugins_dir])

# Build and run tests
foreach t : gst_tests
  fname = '@0@.c'.format(t.get(0))
  test_name = t.get(0).underscorify()
  extra_sources = t.get(3, [ ])
  extra_deps = t.get(2, [ ])
  skip_test = t.get(1, false)
  if not skip_test
    exe = executable(test_name, fname, extra_sources,
      include_directories : [configinc],
      c_args : ['-DHAVE_CONFIG_H=1' ] + test_defines,
      dependencies : test_deps + extra_deps,
    )

    # Define enviroment variable
    env.set('GST_REGISTRY', '@0@/@1@.registry'.format(meson.current_build_dir(), test_name))

    # Run tests
    test(test_name, exe, env: env, timeout : 60)
  endif
endforeach
