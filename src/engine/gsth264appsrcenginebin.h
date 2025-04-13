// src/engine/gsth264appsrcenginebin.h
#ifndef __GST_H264_APP_SRC_ENGINE_BIN_H__
#define __GST_H264_APP_SRC_ENGINE_BIN_H__

#include "gstenginebinbase.h"
#include <gst/app/gstappsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_H264_APP_SRC_ENGINE_BIN (gst_h264_app_src_engine_bin_get_type())
#define GST_H264_APP_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_H264_APP_SRC_ENGINE_BIN, GstH264AppSrcEngineBin))
#define GST_H264_APP_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_H264_APP_SRC_ENGINE_BIN, GstH264AppSrcEngineBinClass))
#define GST_IS_H264_APP_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_H264_APP_SRC_ENGINE_BIN))
#define GST_IS_H264_APP_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_H264_APP_SRC_ENGINE_BIN))
#define GST_H264_APP_SRC_ENGINE_BIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_H264_APP_SRC_ENGINE_BIN, GstH264AppSrcEngineBinClass))

typedef struct _GstH264AppSrcEngineBin GstH264AppSrcEngineBin;
typedef struct _GstH264AppSrcEngineBinClass GstH264AppSrcEngineBinClass;

struct _GstH264AppSrcEngineBin
{
  GstEngineBinBase parent_instance;

  GstAppSrc *vappsrc; // Video app source (expects H.264 bitstream)
  GstAppSrc *aappsrc; // Audio app source (expects raw audio)
  GstElement *h264parser; // Parser needed before tee
};

struct _GstH264AppSrcEngineBinClass
{
  GstEngineBinBaseClass parent_class;

  gpointer padding[4];
};

GType gst_h264_app_src_engine_bin_get_type(void);

// Public methods to access appsrc elements
GstAppSrc* gst_h264_app_src_engine_bin_get_video_appsrc(GstH264AppSrcEngineBin *self);
GstAppSrc* gst_h264_app_src_engine_bin_get_audio_appsrc(GstH264AppSrcEngineBin *self);

G_END_DECLS

#endif /* __GST_H264_APP_SRC_ENGINE_BIN_H__ */ 