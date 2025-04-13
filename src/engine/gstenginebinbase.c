#include "gstenginebinbase.h"
#include <gst/gstinfo.h>
#include <gst/video/video.h> // For video caps details

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GST_DEBUG_CATEGORY_STATIC(gst_engine_bin_base_debug);
#define GST_CAT_DEFAULT gst_engine_bin_base_debug

// Properties
enum
{
  PROP_0,
  PROP_VIDEO_ENCODER,
  PROP_AUDIO_ENCODER,
  PROP_LAST
};

#define DEFAULT_VIDEO_ENCODER "x264enc"
#define DEFAULT_AUDIO_ENCODER "avenc_aac" // Default for publishing
#define PREVIEW_AUDIO_ENCODER "opusenc"   // Always use Opus for preview

// G_DEFINE_ABSTRACT_TYPE requires a private struct
struct _GstEngineBinBasePrivate {
  GstPad *video_target_pad; // Internal pad to link video source to (encoder sink or venctee sink)
  GstPad *audio_target_pad; // Internal pad to link audio source to (atee sink)
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(GstEngineBinBase, gst_engine_bin_base, GST_TYPE_BIN);

// Forward declarations for static functions
static void gst_engine_bin_base_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_engine_bin_base_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_engine_bin_base_finalize(GObject *object);
static void gst_engine_bin_base_dispose(GObject *object);

static gboolean gst_engine_bin_base_start_record_impl(GstEngineBinBase *self, gchar* destination);
static gboolean gst_engine_bin_base_stop_record_impl(GstEngineBinBase *self);
static gboolean gst_engine_bin_base_start_stream_impl(GstEngineBinBase *self, gchar* location, gchar* username, gchar* password);
static gboolean gst_engine_bin_base_stop_stream_impl(GstEngineBinBase *self);


static void gst_engine_bin_base_init(GstEngineBinBase *self)
{
  self->priv = gst_engine_bin_base_get_instance_private(self);
  GST_INFO_OBJECT(self, "Initializing GstEngineBinBase");
  GstBin *bin = GST_BIN(self);

  // Default encoder names
  self->video_encoder_name = g_strdup(DEFAULT_VIDEO_ENCODER);
  self->audio_encoder_name = g_strdup(DEFAULT_AUDIO_ENCODER);

  // --- Create common elements ---
  self->atee = gst_element_factory_make("tee", "atee");
  self->video_encoder = gst_element_factory_make(self->video_encoder_name, "vencoder");
  self->venctee = gst_element_factory_make("tee", "venctee");
  self->qvpreview = gst_element_factory_make("queue", "qvpreview");
  self->preview = gst_element_factory_make("previewsink", "preview");
  self->qvpublishtee = gst_element_factory_make("queue", "qvdtee");
  self->publish = gst_element_factory_make("publishbin", "publish");

  // Common audio convert (before branching to AAC/Opus)
  self->aacqueue = gst_element_factory_make("queue", "aacqueue"); // Queue before convert
  self->aacconvert = gst_element_factory_make("audioconvert", "aacconvert");

  // Opus specific path elements (always created for preview)
  self->opusqueue = gst_element_factory_make("queue", "queueopus");
  self->opusconvert = gst_element_factory_make("audioconvert", "opusconvert");
  self->opusencoder = gst_element_factory_make(PREVIEW_AUDIO_ENCODER, "opusenc_preview");

  if (!self->atee || !self->video_encoder || !self->venctee ||
      !self->qvpreview || !self->preview || !self->qvpublishtee || !self->publish ||
      !self->aacqueue || !self->aacconvert ||
      !self->opusqueue || !self->opusconvert || !self->opusencoder) {
    GST_ERROR_OBJECT(self, "Failed to create one or more common elements");
    // Consider cleanup or returning FALSE if this were a fallible constructor
    return;
  }
  GST_DEBUG_OBJECT(self,"Created common elements");

  // Create the initial audio encoder for publishing based on property
  // This element will be added/linked later by recreate_audio_encoder
  self->audio_encoder = NULL; // Initially null, created by recreate function
  gst_engine_bin_base_recreate_audio_encoder(self); // Create the initial publish encoder


  // Set default video encoder properties (example for x264)
  if (g_strcmp0(self->video_encoder_name, "x264enc") == 0) {
      g_object_set(self->video_encoder, "bitrate", 1000, "tune", 0x00004 /* zerolatency */, "key-int-max", 60, NULL);
      GST_DEBUG_OBJECT(self, "Configured x264enc defaults");
  }

  // Add elements to bin (sources and specific audio encoder added by subclasses/recreate)
  gst_bin_add_many(bin,
                   self->atee, self->video_encoder, self->venctee,
                   self->aacqueue, self->aacconvert, /* audio_encoder added by recreate */
                   self->opusqueue, self->opusconvert, self->opusencoder,
                   self->publish, self->qvpreview, self->qvpublishtee, self->preview,
                   NULL);
   GST_DEBUG_OBJECT(self,"Added common elements to bin");

  // --- Link common elements ---
  // Video path: video_encoder -> venctee -> preview/publish queues
  // Use H264 caps for the link between encoder and tee
  GstCaps *h264_caps = gst_caps_new_simple ("video/x-h264",
                                       "stream-format", G_TYPE_STRING, "byte-stream", // Common format
                                       "alignment", G_TYPE_STRING, "au",
                                       NULL);
  if (g_strcmp0(self->video_encoder_name, "x264enc") == 0) {
      gst_caps_set_simple(h264_caps, "profile", G_TYPE_STRING, "constrained-baseline", NULL);
  }

  // Link encoder to tee using filter caps
  if (!gst_element_link_filtered(self->video_encoder, self->venctee, h264_caps)) {
      GST_WARNING_OBJECT(self, "Failed to link video encoder to tee with H264 caps: %" GST_PTR_FORMAT ". Trying without caps.", h264_caps);
       if (!gst_element_link(self->video_encoder, self->venctee)) {
           GST_ERROR_OBJECT(self, "Failed to link video encoder to tee even without caps");
           gst_caps_unref(h264_caps);
           return;
       }
  }
  gst_caps_unref(h264_caps);
  GST_DEBUG_OBJECT(self,"Linked video encoder to venctee");

  // Link tee outputs to downstream queues
  if (!gst_element_link(self->venctee, self->qvpreview) ||
      !gst_element_link(self->venctee, self->qvpublishtee)) {
      GST_ERROR_OBJECT(self, "Failed to link video tee to preview and publish paths");
      return;
  }
  GST_DEBUG_OBJECT(self,"Linked venctee to preview/publish queues");

  // Link queues to final sinks/bins using gst_element_link
  if (!gst_element_link(self->qvpublishtee, self->publish)) {
    GST_ERROR_OBJECT(self, "Failed link video publish queue to publish bin");
    return;
  }
   if (!gst_element_link(self->qvpreview, self->preview)) {
    GST_ERROR_OBJECT(self, "Failed link video preview queue to preview sink");
    return;
  }
  GST_DEBUG_OBJECT(self,"Linked video queues to publish/preview sinks");

  // Audio paths
  // Link static part of the common AAC/Publish path
  if (!gst_element_link(self->aacqueue, self->aacconvert)) {
       GST_ERROR_OBJECT(self, "Failed to link audio queue (aac) to audio convert");
       return;
  }
  GST_DEBUG_OBJECT(self,"Linked aacqueue -> aacconvert");

  // Link static part of the Opus/Preview path
  if (!gst_element_link_many(self->opusqueue, self->opusconvert, self->opusencoder, self->preview, NULL)) {
      GST_ERROR_OBJECT(self, "Failed to link Opus path (queue -> convert -> encoder -> preview sink)");
      return;
  }
  GST_DEBUG_OBJECT(self,"Linked opusqueue -> opusconvert -> opusencoder -> preview sink");

  // Link audio tee source pads to the queues (still requires pad linking)
  GstPadTemplate *atee_src_templ = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(self->atee), "src_%u");
  if (!atee_src_templ) {
    GST_ERROR_OBJECT(self, "Failed to get tee source pad template");
    return;
  }

  // Link tee pad to opusqueue
  GstPad *atee_opus_src = gst_element_request_pad(self->atee, atee_src_templ, NULL, NULL);
  GstPad *opusq_sink = gst_element_get_static_pad(self->opusqueue, "sink");
  if (!atee_opus_src || !opusq_sink || gst_pad_link(atee_opus_src, opusq_sink) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT(self, "Failed to link atee src to opus queue sink");
      if(atee_opus_src) gst_object_unref(atee_opus_src);
      if(opusq_sink) gst_object_unref(opusq_sink);
      return;
  }
  gst_object_unref(atee_opus_src);
  gst_object_unref(opusq_sink);
  GST_DEBUG_OBJECT(self,"Linked atee src -> opusqueue sink");

  // Link tee pad to aacqueue
  GstPad *atee_aac_src = gst_element_request_pad(self->atee, atee_src_templ, NULL, NULL);
  GstPad *aacq_sink = gst_element_get_static_pad(self->aacqueue, "sink");
  if (!atee_aac_src || !aacq_sink || gst_pad_link(atee_aac_src, aacq_sink) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT(self, "Failed to link atee src to aac queue sink");
      if(atee_aac_src) gst_object_unref(atee_aac_src);
      if(aacq_sink) gst_object_unref(aacq_sink);
      return;
  }
  gst_object_unref(atee_aac_src);
  gst_object_unref(aacq_sink);
  GST_DEBUG_OBJECT(self,"Linked atee src -> aacqueue sink");

  // Store the atee sink pad for subclasses to link their sources to
  GstPad *atee_sink = gst_element_get_static_pad(self->atee, "sink");
  if (!atee_sink) {
      GST_ERROR_OBJECT(self, "Failed to get atee sink pad");
      return; // Cannot proceed
  }
  self->priv->audio_target_pad = atee_sink; // Store (takes ownership)

  // Now that internal audio paths *up to* aacconvert are linked, recreate the publish encoder
  gst_engine_bin_base_recreate_audio_encoder(self); // This will link aacconvert -> audio_encoder -> publish

  // Store video target pad for subclasses
  GstPad *venc_sink = gst_element_get_static_pad(self->video_encoder, "sink");
   if (!venc_sink) {
      GST_ERROR_OBJECT(self, "Failed to get video encoder sink pad");
      return; // Cannot proceed
  }
  self->priv->video_target_pad = venc_sink; // Default target (takes ownership)


  GST_INFO_OBJECT(self, "GstEngineBinBase initialization completed (sources/linking pending subclass)");
}


static void gst_engine_bin_base_class_init(GstEngineBinBaseClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBinClass *bin_class = GST_BIN_CLASS(klass);

  GST_DEBUG_CATEGORY_INIT(gst_engine_bin_base_debug, "enginebinbase", 0, "Engine Bin Base");

  gobject_class->set_property = gst_engine_bin_base_set_property;
  gobject_class->get_property = gst_engine_bin_base_get_property;
  gobject_class->dispose = gst_engine_bin_base_dispose;
  gobject_class->finalize = gst_engine_bin_base_finalize;

  // Define virtual methods defaults
  klass->create_sources = NULL; // Must be implemented by subclasses needing internal sources
  klass->link_sources = NULL;   // Must be implemented by subclasses needing internal sources
  klass->create_sink_pads = NULL; // Must be implemented by subclasses needing sink pads

  // Install properties
  g_object_class_install_property(gobject_class, PROP_VIDEO_ENCODER,
      g_param_spec_string("video-encoder", "Video Encoder",
          "The name of the H264 video encoder element to use",
          DEFAULT_VIDEO_ENCODER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY)); // Changing requires re-creation

  g_object_class_install_property(gobject_class, PROP_AUDIO_ENCODER,
      g_param_spec_string("audio-encoder", "Audio Encoder (Publish)",
          "The name of the audio encoder element to use for publishing (e.g., avenc_aac, opusenc)",
          DEFAULT_AUDIO_ENCODER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)); // Allow changing dynamically

  // Define signals (using the stored signal IDs in the class struct)
  GstEngineBinBaseClass *ebbc = GST_ENGINE_BIN_BASE_CLASS(klass);
  GType record_params[1] = {G_TYPE_STRING};
  ebbc->signals[SIGNAL_START_RECORD] =
      g_signal_newv("start-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_base_start_record_impl), NULL, NULL),
                    NULL, NULL, g_cclosure_marshal_generic, G_TYPE_BOOLEAN, // Return type
                    1, record_params); // Param types

  ebbc->signals[SIGNAL_STOP_RECORD] =
      g_signal_newv("stop-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_base_stop_record_impl), NULL, NULL),
                    NULL, NULL, g_cclosure_marshal_generic, G_TYPE_BOOLEAN, // Return type
                    0, NULL); // Param types

  GType streamer_params[3] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING};
  ebbc->signals[SIGNAL_START_STREAM] =
      g_signal_newv("start-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_base_start_stream_impl), NULL, NULL),
                    NULL, NULL, g_cclosure_marshal_generic, G_TYPE_BOOLEAN, // Return type
                    3, streamer_params); // Param types

  ebbc->signals[SIGNAL_STOP_STREAM] =
      g_signal_newv("stop-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_base_stop_stream_impl), NULL, NULL),
                    NULL, NULL, g_cclosure_marshal_generic, G_TYPE_BOOLEAN, // Return type
                    0, NULL); // Param types

  gst_element_class_set_static_metadata(element_class,
                                        "Engine Bin Base (Abstract)", // Name
                                        "Generic/Bin/Encoder/Filter/Muxer", // Classification
                                        "Base bin for encoding and publishing/previewing media streams", // Description
                                        "Ludovic Bouguerra <ludovic.bouguerra@kalyzee.com>");

  // No static pads defined in base class; subclasses handle this.
}

static void gst_engine_bin_base_dispose(GObject *object)
{
  GstEngineBinBase *self = GST_ENGINE_BIN_BASE(object);
  GST_DEBUG_OBJECT(self, "Disposing GstEngineBinBase");

  // Release references to internal pads stored in private data
  if (self->priv->video_target_pad) {
    gst_object_unref(self->priv->video_target_pad);
    self->priv->video_target_pad = NULL;
  }
   if (self->priv->audio_target_pad) {
    gst_object_unref(self->priv->audio_target_pad);
    self->priv->audio_target_pad = NULL;
  }

  // Chain up to parent class dispose method
  G_OBJECT_CLASS(gst_engine_bin_base_parent_class)->dispose(object);
}


static void gst_engine_bin_base_finalize(GObject *object)
{
  GstEngineBinBase *self = GST_ENGINE_BIN_BASE(object);
  GST_DEBUG_OBJECT(self, "Finalizing GstEngineBinBase");

  g_free(self->video_encoder_name);
  g_free(self->audio_encoder_name);

  // Elements added to the bin are managed by the bin; no need to unref here.
  // However, if we held refs to elements *not* added to the bin, we'd unref them here.

  G_OBJECT_CLASS(gst_engine_bin_base_parent_class)->finalize(object);
}


void gst_engine_bin_base_recreate_audio_encoder(GstEngineBinBase *self)
{
    GstBin *bin = GST_BIN(self);
    gboolean linked_publish = FALSE;
    gboolean linked_convert = FALSE;
    // Remove pads related to atee linking, as it's done in init now
    // GstPad *atee_publish_src = NULL;
    // GstPad *aacq_sink = NULL;
    GstPad *convert_src = NULL;
    GstPad *enc_sink = NULL;
    GstPad *enc_src = NULL;
    GstPad *publish_sink = NULL;


    GST_INFO_OBJECT(self, "Recreating audio encoder for publishing: %s", self->audio_encoder_name);

    // 1. Unlink and remove the old audio encoder if it exists
    if (self->audio_encoder) {
        GST_DEBUG_OBJECT(self, "Removing existing audio encoder %" GST_PTR_FORMAT, self->audio_encoder);

        // Unlink from publish bin
        publish_sink = gst_element_get_static_pad(self->publish, "audio_sink");
        if(publish_sink && gst_pad_is_linked(publish_sink)){
             GstPad *peer = gst_pad_get_peer(publish_sink);
             if(peer) {
                 gst_pad_unlink(peer, publish_sink);
                 gst_object_unref(peer);
             }
        }
        if(publish_sink) gst_object_unref(publish_sink);

        // Unlink from converter
        convert_src = gst_element_get_static_pad(self->aacconvert, "src");
         if(convert_src && gst_pad_is_linked(convert_src)){
             GstPad *peer = gst_pad_get_peer(convert_src);
             if(peer) {
                 gst_pad_unlink(convert_src, peer);
                 gst_object_unref(peer);
             }
        }
       if(convert_src) gst_object_unref(convert_src);

        gst_element_set_state(self->audio_encoder, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(self), self->audio_encoder); // This unrefs the element
        self->audio_encoder = NULL;
    }

    // 2. Create the new audio encoder
    self->audio_encoder = gst_element_factory_make(self->audio_encoder_name, "audio_encoder_publish");
    if (!self->audio_encoder) {
        GST_ERROR_OBJECT(self, "Failed to create audio encoder: %s", self->audio_encoder_name);
        return;
    }
    GST_INFO_OBJECT(self, "Created new audio encoder: %s", self->audio_encoder_name);

    // 3. Add the new encoder to the bin
    if (!gst_bin_add(bin, self->audio_encoder)) {
        GST_ERROR_OBJECT(self, "Failed to add new audio encoder '%s' to bin", self->audio_encoder_name);
        gst_object_unref(self->audio_encoder);
        self->audio_encoder = NULL;
        return;
    }

    // 4. Link the new encoder: aacconvert -> self->audio_encoder -> publish

    // Link common convert output to new encoder input using gst_element_link
    if (!gst_element_link(self->aacconvert, self->audio_encoder)) {
        GST_ERROR_OBJECT(self, "Failed to link aacconvert to new encoder '%s'", self->audio_encoder_name);
        goto link_failed;
    }
    linked_convert = TRUE;
    GST_DEBUG_OBJECT(self,"Linked aacconvert -> new encoder '%s'", self->audio_encoder_name);

    // Link new encoder output to publish bin audio input using gst_element_link
    if (!gst_element_link(self->audio_encoder, self->publish)) {
        GST_ERROR_OBJECT(self, "Failed to link new encoder '%s' to publish bin", self->audio_encoder_name);
        // Attempt to unlink the previous successful link before going to fail
        gst_element_unlink(self->aacconvert, self->audio_encoder);
        goto link_failed;
    }
    linked_publish = TRUE;
    GST_DEBUG_OBJECT(self,"Linked new encoder '%s' -> publish bin", self->audio_encoder_name);


link_failed:
    // Cleanup pads on failure or success - No pads to clean up here now
    // Remove pads related to atee linking
    // ... (pad cleanup code removed) ...

    if(!linked_convert || !linked_publish) {
         GST_ERROR_OBJECT(self, "Failed to complete linking for new audio encoder %s", self->audio_encoder_name);
         // Attempt to remove the newly added encoder if linking failed
         if (self->audio_encoder) { // Check if it was created
            gst_element_set_state(self->audio_encoder, GST_STATE_NULL);
            gst_bin_remove(bin, self->audio_encoder); // unrefs if successful
            self->audio_encoder = NULL;
         }
         return; // Return void
    }

    // 5. Sync state with parent
    gst_element_sync_state_with_parent(self->audio_encoder);
    GST_INFO_OBJECT(self, "Successfully recreated and linked audio encoder '%s'", self->audio_encoder_name);
}


static void gst_engine_bin_base_set_property(GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
  GstEngineBinBase *self = GST_ENGINE_BIN_BASE(object);

  switch (prop_id) {
    case PROP_VIDEO_ENCODER:
      // Property is CONSTRUCT_ONLY, cannot be changed after construction.
      // GObject checks this, but we log a warning if attempted.
       GST_WARNING_OBJECT(self, "Attempted to change read-only 'video-encoder' property after construction.");
       // If it were changeable:
       // g_free(self->video_encoder_name);
       // self->video_encoder_name = g_value_dup_string(value);
       // gst_engine_bin_base_recreate_video_encoder(self); // Need similar logic as audio
      break;
    case PROP_AUDIO_ENCODER:
      {
        gchar *new_name = g_value_dup_string(value);
        if (g_strcmp0(self->audio_encoder_name, new_name) != 0) {
             GST_INFO_OBJECT(self, "Audio encoder name changing from '%s' to '%s'", self->audio_encoder_name, new_name);
             g_free(self->audio_encoder_name);
             self->audio_encoder_name = new_name; // Takes ownership of new_name
             // Recreate the audio encoder part of the pipeline
             gst_engine_bin_base_recreate_audio_encoder(self);
        } else {
            GST_DEBUG_OBJECT(self, "Audio encoder name set to the same value '%s'", new_name);
            g_free(new_name); // Free the duplicated string if name hasn't changed
        }
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_engine_bin_base_get_property(GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
  GstEngineBinBase *self = GST_ENGINE_BIN_BASE(object);

  switch (prop_id) {
    case PROP_VIDEO_ENCODER:
      g_value_set_string(value, self->video_encoder_name);
      break;
    case PROP_AUDIO_ENCODER:
      g_value_set_string(value, self->audio_encoder_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}


// --- Signal Implementations ---

static gboolean gst_engine_bin_base_start_record_impl(GstEngineBinBase *self, gchar* destination)
{
    GST_INFO_OBJECT(self, "Starting record to destination: %s", destination);
    gboolean ret = FALSE;
    // Emit signal on the 'publish' element (publishbin)
    g_signal_emit_by_name(self->publish, "start-record", destination, &ret);
    GST_INFO_OBJECT(self, "Record start signal emitted, result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_base_stop_record_impl(GstEngineBinBase *self)
{
    GST_INFO_OBJECT(self, "Stopping record");
    gboolean ret = FALSE;
    // Emit signal on the 'publish' element (publishbin)
    g_signal_emit_by_name(self->publish, "stop-record", &ret);
    GST_INFO_OBJECT(self, "Record stop signal emitted, result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_base_start_stream_impl(GstEngineBinBase *self, gchar* location, gchar* username, gchar* password)
{
    GST_INFO_OBJECT(self, "Starting stream to location: %s (user: %s)", location, username ? username: "none");
    gboolean ret = FALSE;
    // Emit signal on the 'publish' element (publishbin)
    g_signal_emit_by_name(self->publish, "start-stream", location, username, password, &ret);
    GST_INFO_OBJECT(self, "Stream start signal emitted, result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_base_stop_stream_impl(GstEngineBinBase *self)
{
    GST_INFO_OBJECT(self, "Stopping stream");
    gboolean ret = FALSE;
    // Emit signal on the 'publish' element (publishbin)
    g_signal_emit_by_name(self->publish, "stop-stream", &ret);
    GST_INFO_OBJECT(self, "Stream stop signal emitted, result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
} 