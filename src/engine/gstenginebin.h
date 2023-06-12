#ifndef __GST_ENGINE_BIN_H__
#define __GST_ENGINE_BIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_ENGINE_BIN gst_engine_bin_get_type ()
G_DECLARE_FINAL_TYPE (GstEngineBin, gst_engine_bin, GST, ENGINE_BIN, GstBin)

struct GstEngineBinClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif