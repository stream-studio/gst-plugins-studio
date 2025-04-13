// src/engine/gsth264appsrcenginebin.c
#include "gsth264appsrcenginebin.h"
#include "gstenginebinbase_private.h" // To access priv->audio_target_pad
#include <gst/gstinfo.h>
#include <gst/video/video.h> // For H264 caps
#include <gst/audio/audio.h> // For Raw Audio caps

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GST_DEBUG_CATEGORY_STATIC(gst_h264_app_src_engine_bin_debug);
#define GST_CAT_DEFAULT gst_h264_app_src_engine_bin_debug

G_DEFINE_TYPE(GstH264AppSrcEngineBin, gst_h264_app_src_engine_bin, GST_TYPE_ENGINE_BIN_BASE);

static gboolean gst_h264_app_src_engine_bin_create_sources(GstEngineBinBase *base);
static gboolean gst_h264_app_src_engine_bin_link_sources(GstEngineBinBase *base);

static void gst_h264_app_src_engine_bin_init(GstH264AppSrcEngineBin *self)
{
  GST_INFO_OBJECT(self, "Initializing GstH264AppSrcEngineBin");

  GstEngineBinBaseClass *base_klass = GST_ENGINE_BIN_BASE_GET_CLASS(self);
  if (base_klass->create_sources) {
      if (!base_klass->create_sources(GST_ENGINE_BIN_BASE(self))) {
          GST_ERROR_OBJECT(self, "Failed to create H264/Raw app sources during init");
      }
  }
   if (base_klass->link_sources) {
       if (!base_klass->link_sources(GST_ENGINE_BIN_BASE(self))) {
           GST_ERROR_OBJECT(self, "Failed to link H264/Raw app sources during init");
       }
   }
}

static void gst_h264_app_src_engine_bin_class_init(GstH264AppSrcEngineBinClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstEngineBinBaseClass *base_class = GST_ENGINE_BIN_BASE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_h264_app_src_engine_bin_debug, "h264appsourcenginebin", 0, "App Source Engine Bin (H264 Video)");

  base_class->create_sources = gst_h264_app_src_engine_bin_create_sources;
  base_class->link_sources = gst_h264_app_src_engine_bin_link_sources;
  base_class->create_sink_pads = NULL;

  gst_element_class_set_static_metadata(element_class,
                                        "AppSrc Engine Bin (H264 Video)",
                                        "Source/VideoAudio/Bin/Encoder/Filter/Muxer",
                                        "Engine bin using appsrc for H.264 video and raw audio input",
                                        "Ludovic Bouguerra <ludovic.bouguerra@kalyzee.com>");
}

static gboolean gst_h264_app_src_engine_bin_create_sources(GstEngineBinBase *base)
{
  GstH264AppSrcEngineBin *self = GST_H264_APP_SRC_ENGINE_BIN(base);
  GstBin *bin = GST_BIN(self);

  GST_INFO_OBJECT(self, "Creating app sources for H264 video and Raw audio input");
  self->vappsrc = GST_APP_SRC(gst_element_factory_make("appsrc", "vsource_h264_app"));
  self->aappsrc = GST_APP_SRC(gst_element_factory_make("appsrc", "asource_raw_app"));
  self->h264parser = gst_element_factory_make("h264parse", "h264parse_app");


  if (!self->vappsrc || !self->aappsrc || !self->h264parser) {
    GST_ERROR_OBJECT(self, "Failed to create elements vappsrc='%s' aappsrc='%s' parser='%s'",
                     self->vappsrc?"OK":"FAIL",
                     self->aappsrc?"OK":"FAIL",
                     self->h264parser?"OK":"FAIL");
    if (self->vappsrc) gst_object_unref(self->vappsrc); self->vappsrc = NULL;
    if (self->aappsrc) gst_object_unref(self->aappsrc); self->aappsrc = NULL;
    if (self->h264parser) gst_object_unref(self->h264parser); self->h264parser = NULL;
    return FALSE;
  }

  // Configure appsrc properties
  // H264 video caps (byte-stream is common for appsrc)
  GstCaps *vcaps = gst_caps_new_simple("video/x-h264",
                                       "stream-format", G_TYPE_STRING, "byte-stream",
                                       "alignment", G_TYPE_STRING, "au", // Access Unit alignment recommended
                                       NULL);
  // Raw audio caps (same as GstAppSrcEngineBin)
  GstCaps *acaps = gst_caps_new_simple("audio/x-raw",
                                       "format", G_TYPE_STRING, "S16LE",
                                       "layout", G_TYPE_STRING, "interleaved",
                                       "rate", G_TYPE_INT, 44100,
                                       "channels", G_TYPE_INT, 2,
                                       NULL);

  g_object_set(G_OBJECT(self->vappsrc),
               "caps", vcaps,
               "stream-type", GST_APP_STREAM_TYPE_STREAM,
               "format", GST_FORMAT_TIME,
               "is-live", TRUE,
               "do-timestamp", TRUE,
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
  GST_DEBUG_OBJECT(self, "Configured appsrc elements (H264 video, raw audio)");

  // Add the sources and parser to the bin
  // We don't add the base->video_encoder as it's bypassed
  gst_bin_add_many(bin, GST_ELEMENT(self->vappsrc), GST_ELEMENT(self->aappsrc), self->h264parser, NULL);

  GST_DEBUG_OBJECT(self, "Added app sources and parser to bin");

  // Remove the unused video encoder from the base class bin to avoid issues
  GstEngineBinBase *base_self = GST_ENGINE_BIN_BASE(self);
  if (base_self->video_encoder) {
      GST_DEBUG_OBJECT(self, "Removing unused base video encoder %" GST_PTR_FORMAT, base_self->video_encoder);
      gst_element_set_state(base_self->video_encoder, GST_STATE_NULL); // Ensure it's in NULL state
      if (!gst_bin_remove(bin, base_self->video_encoder)) {
          GST_WARNING_OBJECT(self, "Could not remove base video encoder from bin");
          // Not fatal, but indicates potential issue or double-removal
      }
      // We don't unref here, gst_bin_remove does it if it was the sole owner.
      // The base class still holds a pointer, but it's effectively removed from the pipeline.
      // We could set base_self->video_encoder = NULL, but the base might still use it.
  }


  return TRUE;
}

static gboolean gst_h264_app_src_engine_bin_link_sources(GstEngineBinBase *base)
{
  GstH264AppSrcEngineBin *self = GST_H264_APP_SRC_ENGINE_BIN(base);
  GstEngineBinBase *base_self = GST_ENGINE_BIN_BASE(self);
  GstEngineBinBasePrivate *priv = base_self->priv;

  if (!self->vappsrc || !self->aappsrc || !self->h264parser || !base_self->venctee || !priv->audio_target_pad) {
      GST_ERROR_OBJECT(self, "Cannot link H264/Raw app sources: one or more elements/pads missing.");
      return FALSE;
  }

  GST_DEBUG_OBJECT(self, "Linking H264 app source -> parser -> venctee");

  // Link H264 appsrc -> h264parser
  if (!gst_element_link(GST_ELEMENT(self->vappsrc), self->h264parser)) {
      GST_ERROR_OBJECT(self, "Failed to link H264 app source to H264 parser");
      return FALSE;
  }
  GST_DEBUG_OBJECT(self, "Linked video appsrc -> h264parser");

  // Link h264parser -> venctee (bypass internal video encoder)
  if (!gst_element_link(self->h264parser, base_self->venctee)) {
      GST_ERROR_OBJECT(self, "Failed to link H264 parser to video tee");
      gst_element_unlink(GST_ELEMENT(self->vappsrc), self->h264parser); // Unlink previous step
      return FALSE;
  }
  GST_DEBUG_OBJECT(self, "Linked h264parser -> venctee");


  // Link raw audio appsrc -> audio tee sink (same as raw appsrc bin)
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

  GST_INFO_OBJECT(self, "Successfully linked H264/Raw app sources");
  return TRUE;
}

// --- Public Methods ---

GstAppSrc* gst_h264_app_src_engine_bin_get_video_appsrc(GstH264AppSrcEngineBin *self)
{
    g_return_val_if_fail(GST_IS_H264_APP_SRC_ENGINE_BIN(self), NULL);
    return self->vappsrc ? GST_APP_SRC(gst_object_ref(self->vappsrc)) : NULL;
}

GstAppSrc* gst_h264_app_src_engine_bin_get_audio_appsrc(GstH264AppSrcEngineBin *self)
{
    g_return_val_if_fail(GST_IS_H264_APP_SRC_ENGINE_BIN(self), NULL);
    return self->aappsrc ? GST_APP_SRC(gst_object_ref(self->aappsrc)) : NULL;
} 