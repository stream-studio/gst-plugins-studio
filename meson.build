project('gstreamer-plugin-studio', 'c', version : '1', license : 'LGPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_dep = dependency('gstreamer-1.0')

subdir('src')
subdir('examples')
subdir('tests')