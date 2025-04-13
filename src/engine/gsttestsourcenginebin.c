// src/engine/gsttestsourcenginebin.c
#include "gsttestsourcenginebin.h"
#include "gstenginebinbase_private.h" // Access private data if needed (like target pads)
#include <gst/gstinfo.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GST_DEBUG_CATEGORY_STATIC(gst_test_src_engine_bin_debug);
#define GST_CAT_DEFAULT gst_test_src_engine_bin_debug

// Define the type derived from GstEngineBinBase
G_DEFINE_TYPE(GstTestSrcEngineBin, gst_test_src_engine_bin, GST_TYPE_ENGINE_BIN_BASE);

// Implement the virtual methods from the base class
static gboolean gst_test_src_engine_bin_create_sources(GstEngineBinBase *base);
static gboolean gst_test_src_engine_bin_link_sources(GstEngineBinBase *base);

static void gst_test_src_engine_bin_init(GstTestSrcEngineBin *self)
{
  // Initialization specific to this subclass (if any)
  GST_INFO_OBJECT(self, "Initializing GstTestSrcEngineBin");
  // Base class init is called automatically first

  // Now, call the virtual method implementations for this class to create sources
  // We do this here because source creation might depend on base class setup
  GstEngineBinBaseClass *base_klass = GST_ENGINE_BIN_BASE_GET_CLASS(self);
  if (base_klass->create_sources) {
      if (!base_klass->create_sources(GST_ENGINE_BIN_BASE(self))) {
          GST_ERROR_OBJECT(self, "Failed to create test sources during init");
          // Handle error appropriately - perhaps set a flag
      }
  }

   // Linking happens after all elements are added and possibly configured
   // The base class init should have added its elements.
   // Now link the sources created above.
   if (base_klass->link_sources) {
       if (!base_klass->link_sources(GST_ENGINE_BIN_BASE(self))) {
           GST_ERROR_OBJECT(self, "Failed to link test sources during init");
           // Handle error appropriately
       }
   }

}

static void gst_test_src_engine_bin_class_init(GstTestSrcEngineBinClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstEngineBinBaseClass *base_class = GST_ENGINE_BIN_BASE_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_test_src_engine_bin_debug, "testsourcenginebin", 0, "Test Source Engine Bin");

  // Override the virtual methods with our implementations
  base_class->create_sources = gst_test_src_engine_bin_create_sources;
  base_class->link_sources = gst_test_src_engine_bin_link_sources;
  base_class->create_sink_pads = NULL; // This class doesn't use sink pads

  // No properties or signals specific to this subclass yet

  gst_element_class_set_static_metadata(element_class,
                                        "Test Source Engine Bin", // Name
                                        "Source/VideoAudio/Bin/Encoder/Filter/Muxer", // Classification
                                        "Engine bin using videotestsrc and audiotestsrc", // Description
                                        "Ludovic Bouguerra <ludovic.bouguerra@kalyzee.com>"); // Author
}

static gboolean gst_test_src_engine_bin_create_sources(GstEngineBinBase *base)
{
  GstTestSrcEngineBin *self = GST_TEST_SRC_ENGINE_BIN(base);
  GstBin *bin = GST_BIN(self);

  GST_INFO_OBJECT(self, "Creating test sources");
  self->vsource = gst_element_factory_make("videotestsrc", "vsource_test");
  self->asource = gst_element_factory_make("audiotestsrc", "asource_test");

  if (!self->vsource || !self->asource) {
    GST_ERROR_OBJECT(self, "Failed to create test sources v='%s' a='%s'",
                     self->vsource?"OK":"FAIL", self->asource?"OK":"FAIL" );
    // Clean up partially created elements
    if (self->vsource) gst_object_unref(self->vsource); self->vsource = NULL;
    if (self->asource) gst_object_unref(self->asource); self->asource = NULL;
    return FALSE;
  }

  g_object_set(self->asource, "is-live", TRUE, NULL);
  g_object_set(self->vsource, "is-live", TRUE, "pattern", 18 /* GST_VIDEO_TEST_SRC_BALL */, NULL);

  // Add the sources to the bin
  gst_bin_add_many(bin, self->vsource, self->asource, NULL);
  GST_DEBUG_OBJECT(self,"Added test sources to bin");

  return TRUE;
}

static gboolean gst_test_src_engine_bin_link_sources(GstEngineBinBase *base)
{
  GstTestSrcEngineBin *self = GST_TEST_SRC_ENGINE_BIN(base);
  GstEngineBinBase *base_self = GST_ENGINE_BIN_BASE(self); // Access base members
  GstEngineBinBasePrivate *priv = base_self->priv;

  // Ensure sources and target pads/elements in base class exist
  if (!self->vsource || !self->asource || !base_self->video_encoder || !priv->audio_target_pad) {
      GST_ERROR_OBJECT(self, "Cannot link sources: one or more elements/pads missing.");
      return FALSE;
  }

  GST_DEBUG_OBJECT(self, "Linking test sources");

  // Link video source to video encoder sink (raw video input)
  GstPad *venc_sink = gst_element_get_static_pad(base_self->video_encoder, "sink");
  if (!venc_sink) {
       GST_ERROR_OBJECT(self, "Failed to get video encoder sink pad");
       return FALSE;
  }
  if (!gst_element_link(self->vsource, base_self->video_encoder)) { // Links src to sink
      GST_ERROR_OBJECT(self, "Failed to link video test source to video encoder");
      gst_object_unref(venc_sink);
      return FALSE;
  }
  gst_object_unref(venc_sink);
  GST_DEBUG_OBJECT(self,"Linked video source to encoder");


  // Link audio source to audio tee sink
  GstPad *asrc_pad = gst_element_get_static_pad(self->asource, "src");
   if (!asrc_pad) {
        GST_ERROR_OBJECT(self, "Failed to get audio source pad");
        return FALSE;
   }

  if (gst_pad_link(asrc_pad, priv->audio_target_pad) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT(self, "Failed to link audio test source src to audio tee sink");
      gst_object_unref(asrc_pad);
      // priv->audio_target_pad is owned by the base class, don't unref here
      return FALSE;
  }
  gst_object_unref(asrc_pad);
  GST_DEBUG_OBJECT(self,"Linked audio source to tee");

  GST_INFO_OBJECT(self, "Successfully linked test sources");
  return TRUE;
} 