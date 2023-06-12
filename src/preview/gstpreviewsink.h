#ifndef __GST_PREVIEW_SINK_H__
#define __GST_PREVIEW_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PREVIEW_SINK gst_preview_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstPreviewSink, gst_preview_sink, GST, PREVIEW_SINK, GstBin)

struct GstPreviewSinkClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif
