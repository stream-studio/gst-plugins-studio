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

gboolean ${ELEMENT_FUNCTION_PREFIX}_plugin_init(GstPlugin *plugin)
{
  return gst_element_register(plugin, "${ELEMENT_NAME_CAMEL_CASE_LOWER}",
                              GST_RANK_NONE,
                              GST_TYPE_${ELEMENT_NAME_SNAKE_CASE_UPPER});
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "gst${ELEMENT_NAME_CAMEL_CASE_LOWER}"
#endif

/* gstreamer looks for this structure to register trackers
 *
 * exchange the string 'Template tracker' with your tracker description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ${ELEMENT_NAME_CAMEL_CASE_LOWER},
    "Stream Studio Bin",
    ${ELEMENT_FUNCTION_PREFIX}_plugin_init,
    PACKAGE_VERSION,
    "LGPL",
    "StreamStudio",
    "https://stream.studio/"
)