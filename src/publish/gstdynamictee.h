#ifndef __GST_DYNAMIC_TEE_H__
#define __GST_DYNAMIC_TEE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DYNAMIC_TEE gst_dynamic_tee_get_type ()
G_DECLARE_FINAL_TYPE (GstDynamicTee, gst_dynamic_tee, GST, DYNAMIC_TEE, GstBin)

struct GstDynamicTeeClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif