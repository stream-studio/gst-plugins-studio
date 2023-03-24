#include "gstdewarp.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0,
};

struct _GstDewarp
{
  GstBin parent_instance;
};

G_DEFINE_TYPE(GstDewarp, gst_dewarp, GST_TYPE_BIN);


static void gst_dewarp_init(GstDewarp *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);

}

static void gst_dewarp_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstDewarp *self = GST_DEWARP(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_dewarp_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstDewarp *self = GST_DEWARP(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void gst_dewarp_class_init(GstDewarpClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_dewarp_set_property;
  object_class->get_property = gst_dewarp_get_property;


  gst_element_class_set_static_metadata(element_class,
                                        "dewarp",
                                        "dewarp",
                                        "dewarp",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
