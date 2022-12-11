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
  GstBaseTransform parent_instance;
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

G_DEFINE_TYPE(${ELEMENT_OBJECT_NAME}, ${ELEMENT_FUNCTION_PREFIX}, GST_TYPE_BASE_TRANSFORM);


static GstFlowReturn ${ELEMENT_FUNCTION_PREFIX}_transform_ip (GstBaseTransform *
    base, GstBuffer * outbuf);

static void ${ELEMENT_FUNCTION_PREFIX}_init(${ELEMENT_OBJECT_NAME} *self)
{
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


static GstFlowReturn ${ELEMENT_FUNCTION_PREFIX}_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  ${ELEMENT_OBJECT_NAME} *filter = ${ELEMENT_FUNCTION_PREFIX_UPPER}(base);

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));
  
  g_print("OK");

  return GST_FLOW_OK;
}


static void ${ELEMENT_FUNCTION_PREFIX}_class_init(${ELEMENT_CLASS_NAME} *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = ${ELEMENT_FUNCTION_PREFIX}_set_property;
  object_class->get_property = ${ELEMENT_FUNCTION_PREFIX}_get_property;

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (${ELEMENT_FUNCTION_PREFIX}_transform_ip);


  gst_element_class_set_static_metadata(element_class,
                                        "Template Filter",
                                        "Template Filter",
                                        "Template Filter",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
