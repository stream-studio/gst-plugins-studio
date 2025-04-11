#include "gstenginebin.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gstinfo.h>

/* properties */
enum
{
  PROP_0,
  PROP_VIDEO_ENCODER,
  PROP_AUDIO_ENCODER,
  PROP_USE_TEST_SOURCES,
  PROP_LAST
};

enum
{
  SIGNAL_START_RECORD = 0,
  SIGNAL_STOP_RECORD,
  SIGNAL_START_STREAM,
  SIGNAL_STOP_STREAM,
  LAST_SIGNAL
};

static guint gst_engine_bin_signals[LAST_SIGNAL] = {0};

#define gst_engine_bin_parent_class parent_class

struct _GstEngineBin
{
  GstBin parent_instance;
  GstElement *asource;
  GstElement *vsource;

  GstElement* atee;

  GstElement *vencoder;
  gchar *video_encoder_name;
  GstElement *video_encoder;

  GstElement *aacqueue;
  GstElement *aacconvert;
  GstElement *aacencoder;

  GstElement *opusqueue;
  GstElement *opusconvert;
  GstElement *opusencoder;
  gchar *audio_encoder_name;
  GstElement *audio_encoder;

  GstElement *venctee;

  GstElement *qvpreview;
  GstElement *preview;

  GstElement *qvpublishtee;
  GstElement *publish;

  gboolean use_test_sources;
};

G_DEFINE_TYPE(GstEngineBin, gst_engine_bin, GST_TYPE_BIN);


static void gst_engine_bin_init(GstEngineBin *self)
{
  GstBin *bin = GST_BIN(self);
  GST_INFO("Initializing GstEngineBin");

  self->use_test_sources = TRUE;
  self->video_encoder_name = g_strdup("x264enc");
  self->audio_encoder_name = g_strdup("avenc_aac");

  if (self->use_test_sources) {
    GST_INFO("Using test sources for video and audio");
    self->vsource = gst_element_factory_make("videotestsrc", "vsource");
    self->asource = gst_element_factory_make("audiotestsrc", "asource");
    if (!self->vsource || !self->asource) {
      GST_ERROR("Failed to create test sources");
      return;
    }
    g_object_set(self->asource, "is-live", TRUE, NULL);
    g_object_set(self->vsource, "is-live", TRUE, NULL);
  } else {
    GST_INFO("Expecting external video and audio sources");
  }

  self->atee = gst_element_factory_make("tee", "atee");
  if (!self->atee) {
    GST_ERROR("Failed to create audio tee");
    return;
  }
  GST_DEBUG("Created audio tee element");

  self->video_encoder = gst_element_factory_make(self->video_encoder_name, "vencoder");
  if (!self->video_encoder) {
    GST_ERROR("Failed to create video encoder: %s", self->video_encoder_name);
    return;
  }
  GST_INFO("Created video encoder: %s", self->video_encoder_name);
  
  if (g_strcmp0(self->video_encoder_name, "x264enc") == 0) {
    g_object_set(self->video_encoder, "bitrate", 1000, "tune", 0x00004, "key-int-max", 60, NULL);
    GST_DEBUG("Configured x264enc with bitrate=1000, tune=zerolatency, key-int-max=60");
  }

  self->venctee = gst_element_factory_make("tee", "venctee");
  if (!self->venctee) {
    GST_ERROR("Failed to create video tee");
    return;
  }
  GST_DEBUG("Created video tee element");
  
  self->qvpreview = gst_element_factory_make("queue", "qvpreview");
  self->qvpublishtee = gst_element_factory_make("queue", "qvdtee");
  if (!self->qvpreview || !self->qvpublishtee) {
    GST_ERROR("Failed to create preview queues");
    return;
  }
  GST_DEBUG("Created preview and publish queues");

  self->aacqueue = gst_element_factory_make("queue", "aacqueue");
  self->aacconvert = gst_element_factory_make("audioconvert", "aacconvert");
  self->audio_encoder = gst_element_factory_make(self->audio_encoder_name, "aacencoder");
  if (!self->aacqueue || !self->aacconvert || !self->audio_encoder) {
    GST_ERROR("Failed to create audio encoder pipeline");
    return;
  }
  GST_INFO("Created audio encoder: %s", self->audio_encoder_name);

  self->opusqueue = gst_element_factory_make("queue", "queueopus");
  self->opusconvert = gst_element_factory_make("audioconvert", "opusconvert");
  self->opusencoder = gst_element_factory_make("opusenc", "opusenc");
  if (!self->opusqueue || !self->opusconvert || !self->opusencoder) {
    GST_ERROR("Failed to create Opus encoder pipeline");
    return;
  }
  GST_DEBUG("Created Opus encoder pipeline");

  self->publish = gst_element_factory_make("publishbin", "publish");
  self->preview = gst_element_factory_make("previewsink", "preview");
  if (!self->publish || !self->preview) {
    GST_ERROR("Failed to create publish or preview elements");
    return;
  }
  GST_DEBUG("Created publish and preview elements");

  // Add elements to bin
  if (self->use_test_sources) {
    gst_bin_add_many(bin, 
      self->vsource, self->asource,
      self->atee, self->video_encoder, self->venctee,
      self->aacqueue, self->aacconvert, self->audio_encoder,
      self->opusqueue, self->opusconvert, self->opusencoder,
      self->publish, self->qvpreview, self->qvpublishtee, self->preview,
      NULL);
  } else {
    gst_bin_add_many(bin,
      self->atee, self->video_encoder, self->venctee,
      self->aacqueue, self->aacconvert, self->audio_encoder,
      self->opusqueue, self->opusconvert, self->opusencoder,
      self->publish, self->qvpreview, self->qvpublishtee, self->preview,
      NULL);
  }
  GST_DEBUG("Added all elements to bin");

  if (self->use_test_sources) {
    if (!gst_element_link(self->vsource, self->video_encoder) ||
        !gst_element_link(self->asource, self->atee)) {
      GST_ERROR("Failed to link test sources to encoders");
      return;
    }
    GST_DEBUG("Linked test sources to encoders");
  }

  GstCaps *caps = gst_caps_new_simple ("video/x-h264",
      "profile", G_TYPE_STRING,  "constrained-baseline",
      NULL);

  if (!gst_element_link_filtered(self->video_encoder, self->venctee, caps)) {
    GST_ERROR("Failed to link video encoder to tee with H264 caps");
    gst_caps_unref(caps);
    return;
  }
  gst_caps_unref(caps);
  GST_DEBUG("Linked video encoder to tee with H264 caps");
  
  if (!gst_element_link(self->venctee, self->qvpreview) ||
      !gst_element_link(self->venctee, self->qvpublishtee)) {
    GST_ERROR("Failed to link video tee to preview and publish paths");
    return;
  }
  GST_DEBUG("Linked video tee to preview and publish paths");

  if (!gst_element_link_many(self->atee, self->aacqueue, self->aacconvert, self->audio_encoder, NULL) ||
      !gst_element_link_many(self->atee, self->opusqueue, self->opusconvert, self->opusencoder, NULL)) {
    GST_ERROR("Failed to link audio paths");
    return;
  }
  GST_DEBUG("Linked audio paths");

  if (!gst_element_link_pads(self->qvpublishtee, NULL, self->publish, "video_sink") ||
      !gst_element_link_pads(self->audio_encoder, NULL, self->publish, "audio_sink")) {
    GST_ERROR("Failed to link video and audio to publish bin");
    return;
  }
  GST_DEBUG("Linked video and audio to publish bin");

  if (!gst_element_link_pads(self->qvpreview, NULL, self->preview, "video_sink") ||
      !gst_element_link_pads(self->opusencoder, NULL, self->preview, "audio_sink")) {
    GST_ERROR("Failed to link video and audio to preview sink");
    return;
  }
  GST_DEBUG("Linked video and audio to preview sink");
  
  GST_INFO("GstEngineBin initialization completed");
}

static void gst_engine_bin_set_property(GObject *object,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
  GstEngineBin *self = GST_ENGINE_BIN(object);

  switch (prop_id) {
    case PROP_VIDEO_ENCODER:
      g_free(self->video_encoder_name);
      self->video_encoder_name = g_value_dup_string(value);
      GST_INFO("Video encoder set to: %s", self->video_encoder_name);
      break;
    case PROP_AUDIO_ENCODER:
      g_free(self->audio_encoder_name);
      self->audio_encoder_name = g_value_dup_string(value);
      GST_INFO("Audio encoder set to: %s", self->audio_encoder_name);
      break;
    case PROP_USE_TEST_SOURCES:
      self->use_test_sources = g_value_get_boolean(value);
      GST_INFO("Use test sources set to: %s", self->use_test_sources ? "TRUE" : "FALSE");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gst_engine_bin_get_property(GObject *object,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
  GstEngineBin *self = GST_ENGINE_BIN(object);

  switch (prop_id) {
    case PROP_VIDEO_ENCODER:
      g_value_set_string(value, self->video_encoder_name);
      break;
    case PROP_AUDIO_ENCODER:
      g_value_set_string(value, self->audio_encoder_name);
      break;
    case PROP_USE_TEST_SOURCES:
      g_value_set_boolean(value, self->use_test_sources);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static gboolean gst_engine_bin_start_record(GstEngineBin *self, gchar* destination)
{
    GST_INFO("Starting record to destination: %s", destination);
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "start-record", destination, &ret);
    GST_INFO("Record start result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_stop_record(GstEngineBin *self)
{
    GST_INFO("Stopping record");
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "stop-record", &ret);
    GST_INFO("Record stop result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_start_stream(GstEngineBin *self, gchar* location, gchar* username, gchar* password)
{
    GST_INFO("Starting stream to location: %s", location);
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "start-stream", location, username, password, &ret);
    GST_INFO("Stream start result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}

static gboolean gst_engine_bin_stop_stream(GstEngineBin *self)
{
    GST_INFO("Stopping stream");
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "stop-stream", &ret);
    GST_INFO("Stream stop result: %s", ret ? "SUCCESS" : "FAILED");
    return ret;
}


static void gst_engine_bin_class_init(GstEngineBinClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = gst_engine_bin_set_property;
  object_class->get_property = gst_engine_bin_get_property;

  // Add sink pads for external sources
  gst_element_class_add_pad_template(element_class,
      gst_pad_template_new("video_sink", GST_PAD_SINK, GST_PAD_REQUEST,
          gst_caps_new_simple("video/x-raw",
              "format", G_TYPE_STRING, "I420",
              "width", G_TYPE_INT, 640,
              "height", G_TYPE_INT, 480,
              "framerate", GST_TYPE_FRACTION, 30, 1,
              NULL)));

  gst_element_class_add_pad_template(element_class,
      gst_pad_template_new("audio_sink", GST_PAD_SINK, GST_PAD_REQUEST,
          gst_caps_new_simple("audio/x-raw",
              "format", G_TYPE_STRING, "S16LE",
              "channels", G_TYPE_INT, 2,
              "rate", G_TYPE_INT, 44100,
              NULL)));

  g_object_class_install_property(object_class, PROP_VIDEO_ENCODER,
      g_param_spec_string("video-encoder", "Video Encoder",
          "The name of the video encoder to use",
          "x264enc",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(object_class, PROP_AUDIO_ENCODER,
      g_param_spec_string("audio-encoder", "Audio Encoder",
          "The name of the audio encoder to use",
          "avenc_aac",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(object_class, PROP_USE_TEST_SOURCES,
      g_param_spec_boolean("use-test-sources", "Use Test Sources",
          "Whether to use test sources or expect external sources",
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GType record_params[1] = {G_TYPE_STRING};
  gst_engine_bin_signals[SIGNAL_START_RECORD] =
      g_signal_newv("start-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_start_record), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    1, record_params); 

  gst_engine_bin_signals[SIGNAL_STOP_RECORD] =
      g_signal_newv("stop-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_stop_record), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    0, NULL); 


  GType streamer_params[3] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING};
  gst_engine_bin_signals[SIGNAL_START_STREAM] =
      g_signal_newv("start-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_start_stream), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    3, streamer_params); 

  gst_engine_bin_signals[SIGNAL_STOP_STREAM] =
      g_signal_newv("stop-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_engine_bin_stop_stream), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    0, NULL);      

  gst_element_class_set_static_metadata(element_class,
                                        "Engine Bin",
                                        "Engine Bin",
                                        "Engine Bin",
                                        "Ludovic Bouguerra <ludovic.bouguerra@kalyzee.com>");
}