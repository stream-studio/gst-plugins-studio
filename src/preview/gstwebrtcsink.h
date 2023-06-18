#ifndef __GST_WEBRTC_SINK_H__
#define __GST_WEBRTC_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_WEBRTC_SINK gst_webrtc_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstWebrtcSink, gst_webrtc_sink, GST, WEBRTC_SINK, GstBin)

struct GstWebrtcSinkClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif
