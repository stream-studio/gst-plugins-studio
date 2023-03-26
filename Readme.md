![Logo Stream Studio](docs/assets/banner.png)

# Gstreamer Plugin Template


## Create a custom bin

On the project dir simply start

```
./scripts/create_bin.sh SuperPlugin
```

Your custom bin source code is now available in src/superplugin/gstsuperplugin.c


## Tutorial create Transform Plugin Gstreamer

```
    ./scripts/create_plugin.sh opencv
```

```
    ./scripts/create_transform.sh opencv OpenCVTransform
```

Add in meson.build your new plugin source files 
( You also need gstbase dep )

```
opencv_sources = [
'opencv/gstopencv.c',
'opencv/gstopencvtransform.c',
]

gstbase_dep = dependency('gstreamer-base-1.0')

opencv = library('opencv',
    opencv_sources,
    dependencies : [gst_dep, gstbase_dep],
    c_args: plugin_c_args,
    install : true,
    install_dir : plugins_install_dir,
)
```
Then in your plugin file gstopencv.c add include and register plugin

```
#include "gstopencvtransform.h"
```


```
gboolean opencv_plugin_init(GstPlugin *plugin)
{

    gst_element_register(plugin, "opencvtransform",
                              GST_RANK_NONE,
                              GST_TYPE_OPEN_CV_TRANSFORM);
    return TRUE;
}
```


Then Build your project 
```
mkdir builddir
cd builddir
meson ..
```

Test your plugin

```
    GST_PLUGIN_PATH=$(pwd)/src gst-launch-1.0 gltestsrc is-live=TRUE ! dewarp a=0.328 b=0.339 fx=0.02 fy=0.04 scale=0.343 x=1.003 y=0.999 ! glimagesink
```




