#ifndef __GST_RECORD_SINK_H__
#define __GST_RECORD_SINK_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_RECORD_SINK gst_record_sink_get_type ()
G_DECLARE_FINAL_TYPE (GstRecordSink, gst_record_sink, GST, RECORD_SINK, GstBin)

struct GstRecordSinkClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif