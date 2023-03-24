#ifndef __GST_DEWARP_H__
#define __GST_DEWARP_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DEWARP gst_dewarp_get_type ()
G_DECLARE_FINAL_TYPE (GstDewarp, gst_dewarp, GST, DEWARP, GstBin)

struct GstDewarpClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif
