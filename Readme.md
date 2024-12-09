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

## Dynamic tee usage 


```
    gboolean result;
    g_signal_emit_by_name(receiver_entry->parent->tee, "start", receiver_bin, &result);
```

```
    gboolean result;
    g_signal_emit_by_name(receiver_entry->parent->tee, "stop", receiver_bin, &result);
```


## Preview Sink usage

Preview sink allows you to preview video stream on webrtc server

```
    GST_PLUGIN_PATH=$(pwd)/src gst-launch-1.0 videotestsrc is-live=TRUE ! x264enc key-int-max=50 ! h264parse ! previewsink name=p audiotestsrc is-live=TRUE ! opusenc ! p.
```
# Debian package generation


```
dh_auto_configure --buildsystem=meson
pkg-buildpackage -rfakeroot -us -uc -b
```



 GST_PLUGIN_PATH=$(pwd)/src gst-launch-1.0 videotestsrc is-live=TRUE ! x264enc key-int-max=50 ! h264parse ! previewsink name=p audiotestsrc is-live=TRUE ! opusenc ! p. 


 ## MAC OS Instructions 

brew install icu4c@74

Follow these instructions : 
``` 
    brew info icu4c@74
```

```
==> icu4c@74: stable 74.2 (bottled) [keg-only]
C/C++ and Java libraries for Unicode and globalization
https://icu.unicode.org/home
Deprecated! It will be disabled on 2025-05-01.
Installed
/opt/homebrew/Cellar/icu4c@74/74.2 (271 files, 77.9MB)
  Poured from bottle using the formulae.brew.sh API on 2024-12-09 at 14:20:31
From: https://github.com/Homebrew/homebrew-core/blob/HEAD/Formula/i/icu4c@74.rb
License: ICU
==> Caveats
icu4c@74 is keg-only, which means it was not symlinked into /opt/homebrew,
because this is an alternate version of another formula.

If you need to have icu4c@74 first in your PATH, run:
  echo 'export PATH="/opt/homebrew/opt/icu4c@74/bin:$PATH"' >> /Users/ludovic/.zshrc
  echo 'export PATH="/opt/homebrew/opt/icu4c@74/sbin:$PATH"' >> /Users/ludovic/.zshrc

For compilers to find icu4c@74 you may need to set:
  export LDFLAGS="-L/opt/homebrew/opt/icu4c@74/lib"
  export CPPFLAGS="-I/opt/homebrew/opt/icu4c@74/include"

For pkg-config to find icu4c@74 you may need to set:
  export PKG_CONFIG_PATH="/opt/homebrew/opt/icu4c@74/lib/pkgconfig"
```

```
 ln -s /opt/homebrew/opt/icu4c@74/lib/libicuuc.74.dylib /opt/homebrew/opt/icu4c/lib/libicuuc.74.dylib    
 ln -s /opt/homebrew/opt/icu4c@74/lib/libicudata.74.dylib /opt/homebrew/opt/icu4c/lib/libicudata.74.dylib
```