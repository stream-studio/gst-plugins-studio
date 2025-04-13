#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include "gstenginebinbase.h"
#include "gstenginebin.h"
#include "gsttestsourcenginebin.h"
#include "gstappsrcenginebin.h"
#include "gsth264appsrcenginebin.h"

gboolean engine_plugin_init(GstPlugin *plugin)
{
    if (!gst_element_register(plugin, "testsourcenginebin",
                              GST_RANK_NONE,
                              GST_TYPE_TEST_SRC_ENGINE_BIN)) return FALSE;

    if (!gst_element_register(plugin, "appsrcenginebin",
                              GST_RANK_NONE,
                              GST_TYPE_APP_SRC_ENGINE_BIN)) return FALSE;

    if (!gst_element_register(plugin, "h264appsrcenginebin",
                              GST_RANK_NONE,
                              GST_TYPE_H264_APP_SRC_ENGINE_BIN)) return FALSE;

    if (!gst_element_register(plugin, "enginebin",
                              GST_RANK_NONE,
                              GST_TYPE_ENGINE_BIN)) return FALSE;                              

    return TRUE;
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "gstengine"
#endif

/* gstreamer looks for this structure to register trackers
 *
 * exchange the string 'Template tracker' with your tracker description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    engine,
    "engine",
    engine_plugin_init,
    PACKAGE_VERSION,
    "LGPL",
    "StreamStudio",
    "https://stream.studio/"
)
