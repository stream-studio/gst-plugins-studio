// src/engine/gstappsrcenginebin.c
#include "gstappsrcenginebin.h"
#include "gstenginebinbase_private.h"
#include <gst/gstinfo.h>
#include <gst/video/video.h> // For caps
#include <gst/audio/audio.h> // For caps

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GST_DEBUG_CATEGORY_STATIC(gst_app_src_engine_bin_debug);
#define GST_CAT_DEFAULT gst_app_src_engine_bin_debug

G_DEFINE_TYPE(GstAppSrcEngineBin, gst_app_src_engine_bin, GST_TYPE_ENGINE_BIN_BASE);

static gboolean gst_app_src_engine_bin_create_sources(GstEngineBinBase *base);
static gboolean gst_app_src_engine_bin_link_sources(GstEngineBinBase *base);

static void gst_app_src_engine_bin_init(GstAppSrcEngineBin *self)
{
  GST_INFO_OBJECT(self, "Initializing GstAppSrcEngineBin");

  GstEngineBinBaseClass *base_klass = GST_ENGINE_BIN_BASE_GET_CLASS(self);
  if (base_klass->create_sources) {
      if (!base_klass->create_sources(GST_ENGINE_BIN_BASE(self))) {
          GST_ERROR_OBJECT(self, "Failed to create app sources during init");
      }
  }
   if (base_klass->link_sources) {
       if (!base_klass->link_sources(GST_ENGINE_BIN_BASE(self))) {
           GST_ERROR_OBJECT(self, "Failed to link app sources during init");
       }
   }
}

static void gst_app_src_engine_bin_class_init(GstAppSrcEngineBinClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstEngineBinBaseClass *base_class = GST_ENGINE_BIN_BASE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_app_src_engine_bin_debug, "appsourcenginebin", 0, "App Source Engine Bin (Raw)");

  base_class->create_sources = gst_app_src_engine_bin_create_sources;
  base_class->link_sources = gst_app_src_engine_bin_link_sources;
  base_class->create_sink_pads = NULL;

  gst_element_class_set_static_metadata(element_class,
                                        "AppSrc Engine Bin (Raw)",
                                        "Source/VideoAudio/Bin/Encoder/Filter/Muxer",
                                        "Engine bin using appsrc for raw video and audio input",
                                        "Ludovic Bouguerra <ludovic.bouguerra@kalyzee.com>");
}

static gboolean gst_app_src_engine_bin_create_sources(GstEngineBinBase *base)
{
  GstAppSrcEngineBin *self = GST_APP_SRC_ENGINE_BIN(base);
  GstBin *bin = GST_BIN(self);

  GST_INFO_OBJECT(self, "Creating app sources for raw input");
  self->vappsrc = GST_APP_SRC(gst_element_factory_make("appsrc", "vsource_app"));
  self->aappsrc = GST_APP_SRC(gst_element_factory_make("appsrc", "asource_app"));

  if (!self->vappsrc || !self->aappsrc) {
    GST_ERROR_OBJECT(self, "Failed to create app sources v='%s' a='%s'",
                     self->vappsrc?"OK":"FAIL", self->aappsrc?"OK":"FAIL" );
    if (self->vappsrc) gst_object_unref(self->vappsrc); self->vappsrc = NULL;
    if (self->aappsrc) gst_object_unref(self->aappsrc); self->aappsrc = NULL;
    return FALSE;
  }

  // Configure appsrc properties (crucial!)
  // Example: Set caps for raw video and audio. These should match the data you push.
  GstCaps *vcaps = gst_caps_new_simple("video/x-raw",
                                       "format", G_TYPE_STRING, "I420",
                                       "width", G_TYPE_INT, 640, // Example values
                                       "height", G_TYPE_INT, 480,
                                       "framerate", GST_TYPE_FRACTION, 30, 1,
                                       NULL);
  GstCaps *acaps = gst_caps_new_simple("audio/x-raw",
                                       "format", G_TYPE_STRING, "S16LE", // Example
                                       "layout", G_TYPE_STRING, "interleaved",
                                       "rate", G_TYPE_INT, 44100,
                                       "channels", G_TYPE_INT, 2,
                                       NULL);

  g_object_set(G_OBJECT(self->vappsrc),
               "caps", vcaps,
               "stream-type", GST_APP_STREAM_TYPE_STREAM, // Choose based on usage
               "format", GST_FORMAT_TIME,
               "is-live", TRUE,
               "do-timestamp", TRUE, // If pushing buffers without timestamps
               NULL);
  g_object_set(G_OBJECT(self->aappsrc),
               "caps", acaps,
               "stream-type", GST_APP_STREAM_TYPE_STREAM,
               "format", GST_FORMAT_TIME,
               "is-live", TRUE,
                "do-timestamp", TRUE,
               NULL);

  gst_caps_unref(vcaps);
  gst_caps_unref(acaps);
  GST_DEBUG_OBJECT(self, "Configured appsrc elements");

  gst_bin_add_many(bin, GST_ELEMENT(self->vappsrc), GST_ELEMENT(self->aappsrc), NULL);

  GST_DEBUG_OBJECT(self, "Added app sources to bin");

  return TRUE;
}

static gboolean gst_app_src_engine_bin_link_sources(GstEngineBinBase *base)
{
  GstAppSrcEngineBin *self = GST_APP_SRC_ENGINE_BIN(base);
  GstEngineBinBase *base_self = GST_ENGINE_BIN_BASE(self);
  GstEngineBinBasePrivate *priv = base_self->priv;

  if (!self->vappsrc || !self->aappsrc || !base_self->video_encoder || !priv->audio_target_pad) {
      GST_ERROR_OBJECT(self, "Cannot link app sources: one or more elements/pads missing.");
      return FALSE;
  }

  GST_DEBUG_OBJECT(self, "Linking app sources");

  // Link raw video appsrc to video encoder sink
  if (!gst_element_link(GST_ELEMENT(self->vappsrc), base_self->video_encoder)) {
      GST_ERROR_OBJECT(self, "Failed to link video app source to video encoder");
      return FALSE;
  }
  GST_DEBUG_OBJECT(self, "Linked video appsrc to encoder");

  // Link raw audio appsrc to audio tee sink
  GstPad *aappsrc_pad = gst_element_get_static_pad(GST_ELEMENT(self->aappsrc), "src");
  if (!aappsrc_pad) {
        GST_ERROR_OBJECT(self, "Failed to get audio appsrc pad");
        return FALSE;
   }
  if (gst_pad_link(aappsrc_pad, priv->audio_target_pad) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT(self, "Failed to link audio app source src to audio tee sink");
      gst_object_unref(aappsrc_pad);
      return FALSE;
  }
  gst_object_unref(aappsrc_pad);
  GST_DEBUG_OBJECT(self, "Linked audio appsrc to tee");

  GST_INFO_OBJECT(self, "Successfully linked app sources");
  return TRUE;
}

// --- Public Methods --- 

GstAppSrc* gst_app_src_engine_bin_get_video_appsrc(GstAppSrcEngineBin *self)
{
    g_return_val_if_fail(GST_IS_APP_SRC_ENGINE_BIN(self), NULL);
    // Return a new ref to the appsrc element
    return self->vappsrc ? GST_APP_SRC(gst_object_ref(self->vappsrc)) : NULL;
}

GstAppSrc* gst_app_src_engine_bin_get_audio_appsrc(GstAppSrcEngineBin *self)
{
    g_return_val_if_fail(GST_IS_APP_SRC_ENGINE_BIN(self), NULL);
    return self->aappsrc ? GST_APP_SRC(gst_object_ref(self->aappsrc)) : NULL;
} 