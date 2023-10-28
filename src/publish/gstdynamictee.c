#include "gstdynamictee.h"
#include "gstproxybin.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0
};

enum
{
  SIGNAL_START,
  SIGNAL_STOP,
  LAST_SIGNAL
};

static guint gst_dynamic_tee_signals[LAST_SIGNAL] = {0};


GST_DEBUG_CATEGORY_STATIC (gst_preview_sink_debug); 
#define GST_CAT_DEFAULT gst_preview_sink_debug

#define gst_dynamic_tee_parent_class parent_class


struct _GstDynamicTee
{
  GstBin parent_instance;
  GstElement *taudio;
  GstElement *tvideo;
};

G_DEFINE_TYPE(GstDynamicTee, gst_dynamic_tee, GST_TYPE_BIN);


static void gst_dynamic_tee_init(GstDynamicTee *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  self->taudio = gst_element_factory_make("tee", "atee");
  self->tvideo = gst_element_factory_make("tee", "vtee");

  g_object_set(self->taudio, "allow-not-linked", TRUE, NULL);
  g_object_set(self->tvideo, "allow-not-linked", TRUE, NULL);
  gst_bin_add_many(bin, self->taudio, self->tvideo, NULL);

  GstPad *pad = gst_element_get_static_pad(self->taudio, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->tvideo, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

}

static void gst_dynamic_tee_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstDynamicTee *self = GST_DYNAMIC_TEE(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_dynamic_tee_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstDynamicTee *self = GST_DYNAMIC_TEE(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void gst_dynamic_tee_on_element_error (GstElement* proxy, gchar* message, gpointer user_data){
    GST_DEBUG ("ERROR from element %s with message %s\n", GST_OBJECT_NAME(proxy), message);
    GstDynamicTee* tee = GST_DYNAMIC_TEE(user_data);  
    gst_element_set_state(proxy, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(tee), proxy);
    
    
}

static void gst_dynamic_tee_on_element_eos (GstElement* proxy, gpointer user_data){
    GST_DEBUG ("EOS from element %s\n", GST_OBJECT_NAME(proxy)); 
    GstDynamicTee* tee = GST_DYNAMIC_TEE(user_data);  
    gst_element_set_state(proxy, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(tee), proxy);

}


static gboolean gst_dynamic_tee_start(GstDynamicTee *self, gpointer element_ptr){
  GST_DEBUG("DynamicTee start with new element");
  GstElement* element = GST_ELEMENT(element_ptr);
  
  if (GST_IS_PROXY_BIN(element)){
    GST_DEBUG("Proxy bin detected");
    g_signal_connect (element, "on-error",
          G_CALLBACK (gst_dynamic_tee_on_element_error), (gpointer) self);

    g_signal_connect (element, "on-eos",
          G_CALLBACK (gst_dynamic_tee_on_element_eos), (gpointer) self);

  }



  gst_bin_add(GST_BIN(self), element);
  gst_element_sync_state_with_parent(element);
  gst_element_link(self->tvideo, element);
  gst_element_link(self->taudio, element);
  
  return FALSE;
}

static GstPadProbeReturn stop_bin_callback (GstPad * pad, GstPadProbeInfo * info, gpointer user_data){
    gst_pad_remove_probe (pad, GST_PAD_PROBE_INFO_ID (info));
    GstPad *sink_pad = gst_pad_get_peer(pad);
    gst_pad_send_event(sink_pad, gst_event_new_eos());
    
    gst_pad_unlink(pad, sink_pad);
    g_object_unref(sink_pad);
 
    return GST_PAD_PROBE_DROP;
}

static void stop_bin(GstDynamicTee *self, GstElement *element){
  GstPad *vsink = gst_element_get_static_pad(element, "video_sink");
  GstPad *asink = gst_element_get_static_pad(element, "audio_sink");

  GstPad *vsrc = gst_pad_get_peer(vsink);
  GstPad *asrc = gst_pad_get_peer(asink);

  gst_pad_add_probe(vsrc, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, stop_bin_callback, self, NULL);
  gst_pad_add_probe(asrc, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, stop_bin_callback, self, NULL);
  
  gst_object_unref(vsink);
  gst_object_unref(asink);
  gst_object_unref(vsrc);
  gst_object_unref(asrc);
        
}

static gboolean gst_dynamic_tee_stop(GstDynamicTee *self, gpointer element_ptr){
  GstElement* element = GST_ELEMENT(element_ptr);
  stop_bin(self, element);
  return FALSE;
}

typedef struct{
  const gchar* name;
  GstElement *element;
  GstDynamicTee* tee;
} clean_bin_t;

static gboolean clean_bin(gpointer data_ptr){
  clean_bin_t *data = (clean_bin_t*) data_ptr;
  GstBin* bin = GST_BIN(data->tee);
  if (data->element != NULL){
    gst_element_set_state(data->element, GST_STATE_NULL);
    gst_bin_remove(bin, data->element);
  }

  return G_SOURCE_REMOVE;
}



static void handle_message (GstBin * bin, GstMessage * message){
    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_EOS) {

          GST_DEBUG ("EOS from element %s\n", GST_OBJECT_NAME (GST_MESSAGE_SRC (message)));
          const gchar* src_name = GST_OBJECT_NAME(GST_MESSAGE_SRC (message));
          GstDynamicTee* tee = GST_DYNAMIC_TEE(bin);
        
          clean_bin_t *clean_bin_data = malloc(sizeof(clean_bin_t));
          clean_bin_data->element = GST_ELEMENT(GST_MESSAGE_SRC (message));
          clean_bin_data->name = src_name;
          clean_bin_data->tee = tee;
          g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, clean_bin, clean_bin_data, g_free);
    }

    if(message){
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
    }
}

static void gst_dynamic_tee_class_init(GstDynamicTeeClass *klass)
{
  GstBinClass *bin_class = GST_BIN_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = gst_dynamic_tee_set_property;
  object_class->get_property = gst_dynamic_tee_get_property;
  bin_class->handle_message = handle_message;

  GType tee_params[1] = {G_TYPE_POINTER};

  gst_dynamic_tee_signals[SIGNAL_START] =
      g_signal_newv("start", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_dynamic_tee_start), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    1, tee_params); 

  gst_dynamic_tee_signals[SIGNAL_STOP] =
      g_signal_newv("stop", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_dynamic_tee_stop), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    1, tee_params);      

  GST_DEBUG_CATEGORY_INIT (gst_preview_sink_debug, "dynamictee", 0,
      "dynamictee");


  gst_element_class_set_static_metadata(element_class,
                                        "DynamicTee",
                                        "DynamicTee",
                                        "DynamicTee",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}