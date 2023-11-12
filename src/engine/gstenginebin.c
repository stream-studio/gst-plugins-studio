#include "gstenginebin.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0,
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

  GstElement *aacqueue;
  GstElement *aacconvert;
  GstElement *aacencoder;

  GstElement *opusqueue;
  GstElement *opusconvert;
  GstElement *opusencoder;

  GstElement *venctee;

  GstElement *qvpreview;
  GstElement *preview;

  GstElement *qvpublishtee;
  GstElement *publish;
};

G_DEFINE_TYPE(GstEngineBin, gst_engine_bin, GST_TYPE_BIN);


static void gst_engine_bin_init(GstEngineBin *self)
{
  GstBin *bin = GST_BIN(self);

  self->vsource = gst_element_factory_make("videotestsrc", "vsource");
  self->asource = gst_element_factory_make("audiotestsrc", "asource");
  g_object_set(self->asource, "is-live", TRUE, NULL);
  g_object_set(self->vsource, "is-live", TRUE, NULL);

  self->atee = gst_element_factory_make("tee", "atee");

  self->vencoder = gst_element_factory_make("x264enc", "vencoder");
  g_object_set(self->vencoder, "bitrate", 1000, "tune", 0x00004, "key-int-max", 60, NULL);


  self->venctee = gst_element_factory_make("tee", "venctee");
  
  self->qvpreview = gst_element_factory_make("queue", "qvpreview");
  self->qvpublishtee = gst_element_factory_make("queue", "qvdtee");


  self->aacqueue = gst_element_factory_make("queue", "aacqueue");
  self->aacconvert = gst_element_factory_make("audioconvert", "aacconvert");
  self->aacencoder = gst_element_factory_make("avenc_aac", "aacencoder");

  self->opusqueue = gst_element_factory_make("queue", "queueopus");
  self->opusconvert = gst_element_factory_make("audioconvert", "opusconvert");
  self->opusencoder = gst_element_factory_make("opusenc", "opusenc");


  self->publish = gst_element_factory_make("publishbin", "publish");
  self->preview = gst_element_factory_make("previewsink", "preview");


  gst_bin_add_many(bin, self->vsource, self->vencoder, self->venctee,
                        self->asource, self->atee, self->aacqueue, self->aacconvert, self->aacencoder, self->opusqueue, self->opusconvert, self->opusencoder,
                        self->publish, self->qvpreview, self->qvpublishtee, self->preview, NULL);
  gst_element_link(self->vsource, self->vencoder);
  gst_element_link(self->asource, self->atee);


  GstCaps *caps = gst_caps_new_simple ("video/x-h264",
      "profile", G_TYPE_STRING,  "constrained-baseline",
      NULL);

  gst_element_link_filtered(self->vencoder, self->venctee, caps);
  gst_caps_unref(caps);
  
  gst_element_link(self->venctee, self->qvpreview);
  gst_element_link(self->venctee, self->qvpublishtee);


  gst_element_link_many(self->atee, self->aacqueue, self->aacconvert, self->aacencoder, NULL);
  gst_element_link_many(self->atee, self->opusqueue, self->opusconvert, self->opusencoder, NULL);

  gst_element_link_pads(self->qvpublishtee, NULL, self->publish, "video_sink");
  gst_element_link_pads(self->aacencoder, NULL, self->publish, "audio_sink");


  gst_element_link_pads(self->qvpreview, NULL, self->preview, "video_sink");
  gst_element_link_pads(self->opusencoder, NULL, self->preview, "audio_sink");
  
}

static void gst_engine_bin_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstEngineBin *self = GST_ENGINE_BIN(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_engine_bin_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstEngineBin *self = GST_ENGINE_BIN(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static gboolean gst_engine_bin_start_record(GstEngineBin *self, gchar* destination){
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "start-record", destination, &ret);
    return ret;
}

static gboolean gst_engine_bin_stop_record(GstEngineBin *self){
    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "stop-record", &ret);
    return ret;
}

static gboolean gst_engine_bin_start_stream(GstEngineBin *self, gchar* location, gchar* username, gchar* password){

    gboolean ret = FALSE;
    g_signal_emit_by_name(self->publish, "start-stream", location, username, password, &ret);        
    return FALSE;
}

static gboolean gst_engine_bin_stop_stream(GstEngineBin *self){
  gboolean ret = FALSE;
  g_signal_emit_by_name(self->publish, "stop-stream", &ret);

  return FALSE;
}


static void gst_engine_bin_class_init(GstEngineBinClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_engine_bin_set_property;
  object_class->get_property = gst_engine_bin_get_property;

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