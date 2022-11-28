#include "${ELEMENT_FILENAME_H}"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* properties */
enum
{
  PROP_0,
};

struct _${ELEMENT_OBJECT_NAME}
{
  GstBin parent_instance;
};

G_DEFINE_TYPE(${ELEMENT_OBJECT_NAME}, ${ELEMENT_FUNCTION_PREFIX}, GST_TYPE_BIN);


static void ${ELEMENT_FUNCTION_PREFIX}_init(${ELEMENT_OBJECT_NAME} *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);

}

static void ${ELEMENT_FUNCTION_PREFIX}_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    ${ELEMENT_OBJECT_NAME} *self = ${ELEMENT_FUNCTION_PREFIX_UPPER}(object);

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void ${ELEMENT_FUNCTION_PREFIX}_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    ${ELEMENT_OBJECT_NAME} *self = ${ELEMENT_FUNCTION_PREFIX_UPPER}(object);

    switch (prop_id) {   
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void ${ELEMENT_FUNCTION_PREFIX}_class_init(${ELEMENT_CLASS_NAME} *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = ${ELEMENT_FUNCTION_PREFIX}_set_property;
  object_class->get_property = ${ELEMENT_FUNCTION_PREFIX}_get_property;


  gst_element_class_set_static_metadata(element_class,
                                        "Custom Bin",
                                        "Custom Bin",
                                        "Custom Bin",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
