project('gstreamer-plugin-studio', 'c', version : '1', license : 'LGPL',
  meson_version : '>= 0.50.0',
  default_options : [
    'warning_level=1',
    'buildtype=debug',
    'c_std=gnu99',
    'b_lundef=false',
    'b_asneeded=true',
    'b_pie=true'
  ]
)

# Add debug flags
add_project_arguments('-g', '-O0', language : 'c')
add_project_arguments('-DDEBUG', language : 'c')

# Add sanitizer flags for memory debugging
#add_project_arguments('-fsanitize=address', language : 'c')
#add_project_arguments('-fsanitize=undefined', language : 'c')
#add_project_arguments('-fno-omit-frame-pointer', language : 'c')

# Add link flags for sanitizers
#add_project_link_arguments('-fsanitize=address', language : 'c')
#add_project_link_arguments('-fsanitize=undefined', language : 'c')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_dep = dependency('gstreamer-1.0')

subdir('src')
subdir('examples')
subdir('tests')