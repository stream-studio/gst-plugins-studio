plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', '1.0')
cdata.set_quoted('PACKAGE', 'gst-plugins')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', '1.20')
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer template Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://stream.studio')

configure_file(output : 'config.h',
               configuration : cdata)



cameragl_sources = [
    'cameragl/gstcameragl.c',
    'cameragl/gstdewarp.c'
]

cameragl = library('gstcameragl',
    cameragl_sources,
    dependencies : [gst_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)



publish_sources = [
    'publish/gstpublish.c', 
    'publish/gstdynamictee.c', 
    'publish/gstrecordsink.c', 
    'publish/gststreamsink.c',
]

publish = library('gstpublish',
    publish_sources,
    dependencies : [gst_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)

soup_dep = dependency('libsoup-2.4')
json_dep = dependency('json-glib-1.0')
webrtc_dep = dependency('gstreamer-webrtc-1.0')
sdp_dep = dependency('gstreamer-sdp-1.0')

preview_sources = [
    'preview/gstwebrtcsink.c',
    'preview/gstpreviewsink.c',
    'preview/gstpreview.c',
]

preview = library('gstpreview',
    preview_sources,
    dependencies : [gst_dep, soup_dep, json_dep, webrtc_dep, sdp_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)

engine_sources = [
    'engine/gstenginebin.c',
    'engine/gstengine.c',
]

engine = library('gstengine',
    engine_sources,
    dependencies : [gst_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)
