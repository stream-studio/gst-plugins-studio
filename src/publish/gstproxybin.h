#ifndef __GST_PROXY_BIN_H__
#define __GST_PROXY_BIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PROXY_BIN gst_proxy_bin_get_type ()
G_DECLARE_FINAL_TYPE (GstProxyBin, gst_proxy_bin, GST, PROXY_BIN, GstBin)

struct GstProxyBinClass {
  GstBinClass parent_class;
};

G_END_DECLS

#endif
