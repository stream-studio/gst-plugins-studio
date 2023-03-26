if get_option('tests').disabled()
	subdir_done()
endif

gst_check_dep = dependency('gstreamer-check-1.0', required : get_option('tests'))

if get_option('tests').disabled() and gst_check_dep.found() 
	subdir_done()
endif

pluginsdirs = []
if gst_dep.type_name() == 'pkgconfig'
  pluginsdirs = ['/usr/local/lib/gstreamer-1.0/', gst_dep.get_pkgconfig_variable('pluginsdir')]
  gst_plugin_scanner_dir = gst_dep.get_pkgconfig_variable('pluginscannerdir')
else
  gst_plugin_scanner_dir = gst_proj.get_variable('gst_scanner_dir')
endif
gst_plugin_scanner_path = join_paths(gst_plugin_scanner_dir, 'gst-plugin-scanner')


env = environment()
env.set('GST_PLUGIN_PATH_1_0', meson.build_root()+'/src', pluginsdirs)
env.set('GST_PLUGIN_SYSTEM_PATH_1_0', '')
env.set('GST_STATE_IGNORE_ELEMENTS', '')
env.set('GST_PLUGIN_SCANNER_1_0', gst_plugin_scanner_path)

testrecordsink = executable('testrecordsink', 'publish/recordsink.c', dependencies: [gst_dep, gst_check_dep])
test('test recordsink', testrecordsink, env : env)

teststreamsink = executable('teststreamsink', 'publish/streamsink.c', dependencies: [gst_dep, gst_check_dep])
test('test streamsink', teststreamsink, env : env)

testdewarp = executable('dewarp', 'cameragl/dewarp.c', dependencies: [gst_dep, gst_check_dep])
test('test dewarp', testdewarp, env : env)

