#include "gstpublishbin.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

static guint gst_publish_bin_signals[LAST_SIGNAL] = {0};


struct _GstPublishBin
{
  GstBin parent_instance;

  GstElement *venctee;
  GstElement *aacenctee;

  GstElement *dtee;

  GstElement *recorder;
  GstElement *streamer;
};

G_DEFINE_TYPE(GstPublishBin, gst_publish_bin, GST_TYPE_BIN);


static void gst_publish_bin_init(GstPublishBin *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  

  self->venctee = gst_element_factory_make("tee", "venctee");
  self->aacenctee = gst_element_factory_make("tee", "aacenctee");

  self->dtee = gst_element_factory_make("dynamictee", "dtee");
  

  gst_bin_add_many(bin, self->venctee, self->aacenctee, self->dtee, NULL);
  gst_element_link_pads(self->venctee, NULL, self->dtee, "video_sink");
  gst_element_link_pads(self->aacenctee, NULL, self->dtee, "audio_sink");


  GstPad *pad = gst_element_get_static_pad(self->aacenctee, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->venctee, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));
  
  self->recorder = NULL;
  self->streamer = NULL;


}

static void gst_publish_bin_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstPublishBin *self = GST_PUBLISH_BIN(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_publish_bin_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstPublishBin *self = GST_PUBLISH_BIN(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}


static gboolean gst_publish_bin_start_record(GstPublishBin *self, gchar* destination){
    gboolean ret = FALSE;
    
    if (!self->recorder){
      self->recorder = gst_element_factory_make("proxybin", "precorder"); 
      GstElement *recorder = gst_element_factory_make("recordsink", "recorder");
      g_object_set(recorder, "location", destination, NULL);
      g_object_set(self->recorder, "child", recorder, NULL);

      g_signal_emit_by_name(self->dtee, "start", self->recorder, &ret);
      
      ret = TRUE;

    }
    
    return ret;
}

static gboolean gst_publish_bin_stop_record(GstPublishBin *self){
    gboolean ret = FALSE;
    
    if (self->recorder){
      g_signal_emit_by_name(self->dtee, "stop", self->recorder, &ret);
      self->recorder = NULL;
      ret = TRUE;
    }
        
    return ret;
}

static gboolean gst_publish_bin_start_stream(GstPublishBin *self, gchar* location, gchar* username, gchar* password){

    gboolean ret = FALSE;
        
    if (!self->streamer){
      self->streamer = gst_element_factory_make("proxybin", "pstreamer");
      GstElement *streamer = gst_element_factory_make("streamsink", "streamer");
      g_object_set(streamer, "location", location, NULL);
      if (! g_strcmp0(username, "")){
        g_object_set(streamer, "username", username, NULL);
      }
      if (! g_strcmp0(password, "")){
        g_object_set(streamer, "password", password, NULL);
      }      
      g_object_set(self->streamer, "child", streamer, NULL);
      g_signal_emit_by_name(self->dtee, "start", self->streamer, &ret);
      ret = TRUE;
    }
    
    return FALSE;
}

static gboolean gst_publish_bin_stop_stream(GstPublishBin *self){
  gboolean ret = FALSE;
    
  if (self->streamer){
    g_signal_emit_by_name(self->dtee, "stop", self->streamer, &ret);
    self->streamer = NULL;
  }

  return FALSE;
}



static void gst_publish_bin_class_init(GstPublishBinClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_publish_bin_set_property;
  object_class->get_property = gst_publish_bin_get_property;


  GType record_params[1] = {G_TYPE_STRING};
  gst_publish_bin_signals[SIGNAL_START_RECORD] =
      g_signal_newv("start-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_publish_bin_start_record), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    1, record_params); 

  gst_publish_bin_signals[SIGNAL_STOP_RECORD] =
      g_signal_newv("stop-record", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_publish_bin_stop_record), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    0, NULL); 


  GType streamer_params[3] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING};
  gst_publish_bin_signals[SIGNAL_START_STREAM] =
      g_signal_newv("start-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_publish_bin_start_stream), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    3, streamer_params); 

  gst_publish_bin_signals[SIGNAL_STOP_STREAM] =
      g_signal_newv("stop-stream", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_publish_bin_stop_stream), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    0, NULL);      


  gst_element_class_set_static_metadata(element_class,
                                        "Publish Bin",
                                        "Publish Bin",
                                        "Publish Bin",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
