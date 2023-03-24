#ifndef __GST_STREAM_SINK_H__
#define __GST_STREAM_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_STREAM_SINK gst_stream_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstStreamSink, gst_stream_sink, GST, STREAM_SINK, GstBin)

struct GstStreamSinkClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif