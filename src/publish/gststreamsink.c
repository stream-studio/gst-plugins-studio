#include "gststreamsink.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DEFAULT_LOCATION "rtmp://localhost/app"
#define DEFAULT_USERNAME NULL
#define DEFAULT_PASSWORD NULL

GST_DEBUG_CATEGORY_STATIC (gst_stream_sink_debug); 
#define GST_CAT_DEFAULT gst_stream_sink_debug


/* properties */
enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_USERNAME,
  PROP_PASSWORD,
  N_PROPERTIES

};

enum
{
  SIGNAL_START_STREAM = 0,
  SIGNAL_STOP_STREAM,
  LAST_SIGNAL
};

static guint gst_stream_sink_signals[LAST_SIGNAL] = {0};

struct _GstStreamSink
{
  GstBin parent_instance;

  GstElement* aqueue;
  GstElement* avalve;
  
  GstElement* vvalve;
  GstElement* vqueue;
  
  GstElement* h264parse;
  GstElement* aacparse;

  GstElement* flvmux;
  GstElement* rtmpsink;
};

#define gst_stream_sink_parent_class parent_class
G_DEFINE_TYPE(GstStreamSink, gst_stream_sink, GST_TYPE_BIN);


static void gst_stream_sink_init(GstStreamSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  
  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");
  
  self->h264parse = gst_element_factory_make("h264parse", "vparse");
  self->aacparse = gst_element_factory_make("aacparse", "aparse");

  self->flvmux = gst_element_factory_make("flvmux", "mux");
  self->rtmpsink = gst_element_factory_make("rtmp2sink", "sink");

  gst_bin_add_many(GST_BIN(bin), self->vqueue, self->h264parse, self->aqueue, self->aacparse, self->flvmux, self->rtmpsink, NULL);
  gst_element_link_many(self->aqueue, self->aacparse, self->flvmux, self->rtmpsink, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->flvmux, NULL);

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

}



static void gst_stream_sink_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstStreamSink *self = GST_STREAM_SINK(object);

    switch (prop_id) {
        case PROP_LOCATION:
            g_object_set_property(G_OBJECT(self->rtmpsink), "location", value);
            break;
        case PROP_USERNAME:
            g_object_set_property(G_OBJECT(self->rtmpsink), "username", value);
            break;
        case PROP_PASSWORD:
            g_object_set_property(G_OBJECT(self->rtmpsink), "password", value);
            break;                              
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_stream_sink_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstStreamSink *self = GST_STREAM_SINK(object);

    switch (prop_id) {
        case PROP_LOCATION:
            g_object_get_property(G_OBJECT(self->rtmpsink), "location", value);
            break;
        case PROP_USERNAME:
            g_object_get_property(G_OBJECT(self->rtmpsink), "username", value);
            break;
        case PROP_PASSWORD:
            g_object_get_property(G_OBJECT(self->rtmpsink), "password", value);
            break;                                 
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void handle_message (GstBin * bin, GstMessage * message){
    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {

        const gchar* src_name = GST_OBJECT_NAME(GST_MESSAGE_SRC (message));
        GError *err = NULL;
        gchar *dbg_info = NULL;

        gst_message_parse_error (message, &err, &dbg_info);
        g_printerr ("ERROR from element %s: %s\n",
            GST_OBJECT_NAME (message->src), err->message);
        g_printerr ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");          
        
    }

    if(message){
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
    }
}

static void gst_stream_sink_class_init(GstStreamSinkClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBinClass *bin_class = GST_BIN_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_stream_sink_set_property;
  object_class->get_property = gst_stream_sink_get_property;
  bin_class->handle_message = handle_message;
  
  g_object_class_install_property(object_class, PROP_LOCATION,
                                  g_param_spec_string("location", "Location",
                                                   "File location", DEFAULT_LOCATION,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_USERNAME,
                                  g_param_spec_string("username", "username",
                                                   "Username", DEFAULT_USERNAME,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_PASSWORD,
                                  g_param_spec_string("password", "password",
                                                   "Password", DEFAULT_PASSWORD,
                                                   G_PARAM_READWRITE));

  gst_element_class_set_static_metadata(element_class,
                                        "Stream Sink",
                                        "Stream Bin",
                                        "RTMP Stream Sink",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio");
}