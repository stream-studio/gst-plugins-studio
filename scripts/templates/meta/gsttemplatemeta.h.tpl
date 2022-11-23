#ifndef __GST_${ELEMENT_NAME_SNAKE_CASE_UPPER}_H__
#define __GST_${ELEMENT_NAME_SNAKE_CASE_UPPER}_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define ${ELEMENT_NAME_SNAKE_CASE_UPPER}_API_TYPE (${ELEMENT_FUNCTION_PREFIX}_api_get_type())
#define ${ELEMENT_NAME_SNAKE_CASE_UPPER}_INFO (${ELEMENT_FUNCTION_PREFIX}_get_info())

typedef struct _${ELEMENT_OBJECT_NAME} ${ELEMENT_OBJECT_NAME};


struct _${ELEMENT_OBJECT_NAME}
{
    GstMeta meta;
    gint fake;
};

GType ${ELEMENT_FUNCTION_PREFIX}_api_get_type(void);

GST_EXPORT
const GstMetaInfo *${ELEMENT_FUNCTION_PREFIX}_get_info(void);

GST_EXPORT
GstAnalyserMuxMeta *gst_buffer_add_${ELEMENT_NAME_SNAKE_CASE}(GstBuffer *buffer, gint fake);

GST_EXPORT
gboolean gst_buffer_remove_${ELEMENT_NAME_SNAKE_CASE}(GstBuffer *buffer);

GST_EXPORT
GstAnalyserMuxMeta *gst_buffer_get_${ELEMENT_NAME_SNAKE_CASE}(GstBuffer *b);


G_END_DECLS

#endif