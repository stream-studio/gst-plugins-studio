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
    'publish/gststreamsink.c'
]

publish = library('gstpublish',
    publish_sources,
    dependencies : [gst_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)
