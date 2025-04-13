// src/engine/gstappsrcenginebin.h
#ifndef __GST_APP_SRC_ENGINE_BIN_H__
#define __GST_APP_SRC_ENGINE_BIN_H__

#include "gstenginebinbase.h"
#include <gst/app/gstappsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_APP_SRC_ENGINE_BIN (gst_app_src_engine_bin_get_type())
#define GST_APP_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_APP_SRC_ENGINE_BIN, GstAppSrcEngineBin))
#define GST_APP_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_APP_SRC_ENGINE_BIN, GstAppSrcEngineBinClass))
#define GST_IS_APP_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_APP_SRC_ENGINE_BIN))
#define GST_IS_APP_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_APP_SRC_ENGINE_BIN))
#define GST_APP_SRC_ENGINE_BIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_APP_SRC_ENGINE_BIN, GstAppSrcEngineBinClass))

typedef struct _GstAppSrcEngineBin GstAppSrcEngineBin;
typedef struct _GstAppSrcEngineBinClass GstAppSrcEngineBinClass;

struct _GstAppSrcEngineBin
{
  GstEngineBinBase parent_instance;

  GstAppSrc *vappsrc; // Video app source (expects raw video)
  GstAppSrc *aappsrc; // Audio app source (expects raw audio)
};

struct _GstAppSrcEngineBinClass
{
  GstEngineBinBaseClass parent_class;

  gpointer padding[4];
};

GType gst_app_src_engine_bin_get_type(void);

// Public methods to access appsrc elements if needed
GstAppSrc* gst_app_src_engine_bin_get_video_appsrc(GstAppSrcEngineBin *self);
GstAppSrc* gst_app_src_engine_bin_get_audio_appsrc(GstAppSrcEngineBin *self);

G_END_DECLS

#endif /* __GST_APP_SRC_ENGINE_BIN_H__ */ 