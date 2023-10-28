#include "gstproxybin.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0,
  PROP_CHILD,
};

enum {
  SIGNAL_ON_EOS = 0,
  SIGNAL_ON_ERROR,
  LAST_SIGNAL
};

static guint gst_proxy_bin_signals[LAST_SIGNAL] = {0};


#define gst_proxy_bin_parent_class parent_class

#define GST_CAT_DEFAULT gst_proxy_bin_debug
GST_DEBUG_CATEGORY_STATIC (gst_proxy_bin_debug); 

struct _GstProxyBin
{
  GstBin parent_instance;
  /** 
   * Proxy Sink part
  */
  GstElement *aqueue;
  GstElement *vqueue;
  GstElement *asink;
  GstElement *vsink;

  /** 
   * Proxy Source part
  */
  GstElement *pipeline;
  GstBus *bus;
  
  GstElement *asrc;
  GstElement *vsrc;

  GstElement *child;
};




G_DEFINE_TYPE(GstProxyBin, gst_proxy_bin, GST_TYPE_BIN);




static gboolean bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
  GstProxyBin *self = GST_PROXY_BIN(data);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *err;
      gchar *debug;

      gst_message_parse_error (message, &err, &debug);
      g_signal_emit_by_name(self, "on-error", err->message);
      g_error_free (err);
      g_free (debug);
      break;
    }
    case GST_MESSAGE_EOS:
      g_signal_emit_by_name(self, "on-eos");
      break;
    default:
      /* unhandled message */
      break;
  }


  return TRUE;
}

static void gst_proxy_bin_init(GstProxyBin *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  
  GST_INFO("Proxy bin init");
  
  self->child = NULL;

  self->pipeline = gst_pipeline_new(NULL);
  self->bus = gst_pipeline_get_bus(GST_PIPELINE(self->pipeline));
  gst_bus_add_watch (self->bus, bus_callback, self);

  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");

  self->asink = gst_element_factory_make("proxysink", "asink");
  self->vsink = gst_element_factory_make("proxysink", "vsink");

  self->asrc = gst_element_factory_make("proxysrc", "asrc");
  self->vsrc = gst_element_factory_make("proxysrc", "vsrc");
  gst_bin_add_many(GST_BIN(self->pipeline), self->asrc, self->vsrc, NULL);

  gst_bin_add_many(bin, self->aqueue, self->vqueue, self->asink, self->vsink, NULL);
  
  gst_element_link(self->aqueue, self->asink);
  gst_element_link(self->vqueue, self->vsink);

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));


}


static GstStateChangeReturn gst_proxy_bin_change_state(GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstProxyBin *self = GST_PROXY_BIN(element);

  switch (transition) {
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_INFO("Proxy bin change state to playing"); 

      if (self->child == NULL)
        return GST_STATE_CHANGE_FAILURE;
      
      
      GST_INFO("Adding child in subpipeline"); 
      gst_bin_add(GST_BIN(self->pipeline), self->child);
      
      GST_DEBUG("Adding child in subpipeline"); 
      gst_element_sync_state_with_parent(self->child);

      g_object_set(self->asrc, "proxysink", self->asink, NULL);
      g_object_set(self->vsrc, "proxysink", self->vsink, NULL);

      gst_element_link_pads(self->asrc, "src", self->child, "audio_sink");
      gst_element_link_pads(self->vsrc, "src", self->child, "video_sink");

      GstClockTime time = gst_element_get_base_time(GST_ELEMENT_PARENT(self));
      gst_element_set_base_time (self->pipeline, time);
      
      gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
	  break;
	default:
	  break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
	return ret;

  switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_DEBUG("Proxy bin change state to PAUSED"); 
      gst_element_set_state(self->pipeline, GST_STATE_NULL);
	  break;
	default:
	  break;
  }

  return ret;
}


static void gst_proxy_bin_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstProxyBin *self = GST_PROXY_BIN(object);

    switch (prop_id) {
        case PROP_CHILD:
            self->child = GST_ELEMENT(g_value_get_object(value));
        break;           
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_proxy_bin_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstProxyBin *self = GST_PROXY_BIN(object);

    switch (prop_id) {   
        case PROP_CHILD:
            g_value_set_object(value, self->child);
        break;          
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void gst_proxy_bin_class_init(GstProxyBinClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);



  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_proxy_bin_set_property;
  object_class->get_property = gst_proxy_bin_get_property;
  element_class->change_state = gst_proxy_bin_change_state;


  gst_proxy_bin_signals[SIGNAL_ON_EOS] =
      g_signal_new ("on-eos", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  gst_proxy_bin_signals[SIGNAL_ON_ERROR] =
      g_signal_new ("on-error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);      

  GST_DEBUG_CATEGORY_INIT (gst_proxy_bin_debug, "proxybin", 0,
      "Proxy Bin Debug");

  g_object_class_install_property(object_class, PROP_CHILD,
                                  g_param_spec_object("child", "child",
                                                   "child", GST_TYPE_ELEMENT,
                                                   G_PARAM_READWRITE));

  gst_element_class_set_static_metadata(element_class,
                                        "Proxy Bin",
                                        "Proxy Bin",
                                        "Proxy Bin",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
