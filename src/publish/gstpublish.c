#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include "gstdynamictee.h"
#include "gstrecordsink.h"
#include "gststreamsink.h"

gboolean publish_plugin_init(GstPlugin *plugin)
{
    gst_element_register(plugin, "dynamictee",
                              GST_RANK_NONE,
                              GST_TYPE_DYNAMIC_TEE);

    gst_element_register(plugin, "recordsink",
                              GST_RANK_NONE,
                              GST_TYPE_RECORD_SINK);

    gst_element_register(plugin, "streamsink",
                              GST_RANK_NONE,
                              GST_TYPE_STREAM_SINK);                              
    return TRUE;
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "gstpublish"
#endif

/* gstreamer looks for this structure to register trackers
 *
 * exchange the string 'Template tracker' with your tracker description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    publish,
    "publish",
    publish_plugin_init,
    PACKAGE_VERSION,
    "LGPL",
    "StreamStudio",
    "https://stream.studio/"
)
