#ifndef __GST_PUBLISH_BIN_H__
#define __GST_PUBLISH_BIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PUBLISH_BIN gst_publish_bin_get_type ()
G_DECLARE_FINAL_TYPE (GstPublishBin, gst_publish_bin, GST, PUBLISH_BIN, GstBin)

struct GstPublishBinClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif
