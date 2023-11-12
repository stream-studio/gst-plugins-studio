if get_option('examples').disabled()
	subdir_done()
endif

gtk_dep = dependency('gtk+-3.0', version : '>= 3.0')

example_sources = [
    'example.c', 
]

example = executable('example',
    example_sources,
    dependencies : [gst_dep, gtk_dep],
    install : true,
    install_dir : plugins_install_dir,
)