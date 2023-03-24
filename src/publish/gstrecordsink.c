#include "gstrecordsink.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_record_sink_debug); 
#define GST_CAT_DEFAULT gst_record_sink_debug

#define DEFAULT_LOCATION "record.mp4"

#define gst_record_sink_parent_class parent_class

/* properties */
enum
{
  PROP_0,
  PROP_LOCATION
};

enum
{
  LAST_SIGNAL
};

static guint gst_record_sink_signals[LAST_SIGNAL] = {0};

struct _GstRecordSink
{
  GstBin parent_instance;
  
  GstElement* aqueue;  
  GstElement* vqueue;
  
  GstElement* h264parse;
  GstElement* aacparse;

  GstElement* mp4mux;
  GstElement* filesink;
};

G_DEFINE_TYPE(GstRecordSink, gst_record_sink, GST_TYPE_BIN);


static void gst_record_sink_init(GstRecordSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  
  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");
  
  self->h264parse = gst_element_factory_make("h264parse", "vparse");
  self->aacparse = gst_element_factory_make("aacparse", "aparse");

  self->mp4mux = gst_element_factory_make("mp4mux", "mux");
  self->filesink = gst_element_factory_make("filesink", "sink");

  gst_bin_add_many(GST_BIN(bin), self->vqueue, self->h264parse, self->aqueue, self->aacparse, self->mp4mux, self->filesink, NULL);
  gst_element_link_many(self->aqueue, self->aacparse, self->mp4mux, self->filesink, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->mp4mux, NULL);

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

}


static void gst_video_recorder_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){


}


static void gst_record_sink_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstRecordSink *self = GST_RECORD_SINK(object);

    switch (prop_id) {
        case PROP_LOCATION:
            g_object_set_property(G_OBJECT(self->filesink), "location", value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
          break;
    }

}

static void gst_record_sink_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstRecordSink *self = GST_RECORD_SINK(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}



static void gst_record_sink_class_init(GstRecordSinkClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBinClass *bin_class = GST_BIN_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_record_sink_set_property;
  object_class->get_property = gst_record_sink_get_property;

  g_object_class_install_property(object_class, PROP_LOCATION,
                                  g_param_spec_string("location", "Location",
                                                   "File location", DEFAULT_LOCATION,
                                                   G_PARAM_READWRITE));

  gst_element_class_set_static_metadata(element_class,
                                        "RecordSink",
                                        "RecordSink",
                                        "RecordSink",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio");
}