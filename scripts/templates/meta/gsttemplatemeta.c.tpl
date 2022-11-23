#include "${ELEMENT_FILENAME_H}"

static gboolean ${ELEMENT_FUNCTION_PREFIX}_init(GstMeta *meta, gpointer params, GstBuffer *buffer);
static void ${ELEMENT_FUNCTION_PREFIX}_free(GstMeta *meta, GstBuffer *buffer);
static gboolean ${ELEMENT_FUNCTION_PREFIX}_transform(GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer,
                                         GQuark type, gpointer data);


GType ${ELEMENT_FUNCTION_PREFIX}_api_get_type(void)
{
    static const gchar *tags[] = {NULL};
    static volatile GType type;
    if (g_once_init_enter(const_cast<GType *>(&type)))
    {
        GType _type = gst_meta_api_type_register("${ELEMENT_OBJECT_NAME}API", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

const GstMetaInfo *${ELEMENT_FUNCTION_PREFIX}_get_info(void)
{
    static const GstMetaInfo *${ELEMENT_FUNCTION_PREFIX}_info = NULL;

    if (g_once_init_enter(&${ELEMENT_FUNCTION_PREFIX}_info))
    {
        const GstMetaInfo *meta = gst_meta_register(GST_ANALYSER_MUX_META_API_TYPE, /* api type */
                                                    "${ELEMENT_OBJECT_NAME}",          /* implementation type */
                                                    sizeof(${ELEMENT_OBJECT_NAME}),    /* size of the structure */
                                                    ${ELEMENT_FUNCTION_PREFIX}_init,
                                                    (GstMetaFreeFunction)${ELEMENT_FUNCTION_PREFIX}_free,
                                                    ${ELEMENT_FUNCTION_PREFIX}_transform);
        g_once_init_leave(&${ELEMENT_FUNCTION_PREFIX}_info, meta);
    }
    return ${ELEMENT_FUNCTION_PREFIX}_info;
}

static gboolean ${ELEMENT_FUNCTION_PREFIX}_init(GstMeta *meta, gpointer params, GstBuffer *buffer)
{
    ${ELEMENT_OBJECT_NAME} *${ELEMENT_FUNCTION_PREFIX} = (${ELEMENT_OBJECT_NAME} *)meta;
    ${ELEMENT_FUNCTION_PREFIX}->origin = FALSE;
    return TRUE;
}

static void ${ELEMENT_FUNCTION_PREFIX}_free(GstMeta *meta, GstBuffer *buffer)
{
    ${ELEMENT_OBJECT_NAME} *${ELEMENT_FUNCTION_PREFIX} = (${ELEMENT_OBJECT_NAME} *)meta;
    gst_buffer_unref(${ELEMENT_FUNCTION_PREFIX}->origin);
}


static gboolean ${ELEMENT_FUNCTION_PREFIX}_transform(GstBuffer *transbuf, GstMeta *meta, GstBuffer *buffer,
                                         GQuark type, gpointer data)
{
    ${ELEMENT_OBJECT_NAME} *${ELEMENT_FUNCTION_PREFIX} = (${ELEMENT_OBJECT_NAME} *)meta;
    gst_buffer_add_analyser_mux_meta(transbuf, ${ELEMENT_FUNCTION_PREFIX}->fake);
    if (gst_buffer_is_writable(buffer))
    {
        gst_buffer_remove_meta(buffer, meta);
    }
    return TRUE;
}

GstAnalyserMuxMeta *gst_buffer_get_analyser_mux_meta(GstBuffer *buffer)
{
    ${ELEMENT_OBJECT_NAME} *meta = (${ELEMENT_OBJECT_NAME} *)gst_buffer_get_meta((buffer), ${ELEMENT_NAME_SNAKE_CASE_UPPER}_API_TYPE);
    return meta;
}

GstAnalyserMuxMeta *gst_buffer_add_${ELEMENT_NAME_SNAKE_CASE}(GstBuffer *buffer, gint fake)
{
    ${ELEMENT_OBJECT_NAME} *${ELEMENT_FUNCTION_PREFIX} = NULL;

    g_return_val_if_fail((int)GST_IS_BUFFER(buffer), NULL);

    if (!gst_buffer_is_writable(buffer))
        return ${ELEMENT_FUNCTION_PREFIX};

    ${ELEMENT_FUNCTION_PREFIX} = (${ELEMENT_OBJECT_NAME} *)gst_buffer_add_meta(buffer, GST_ANALYSER_MUX_META_INFO, NULL);

    ${ELEMENT_FUNCTION_PREFIX}->fake = fake;

    return ${ELEMENT_FUNCTION_PREFIX};
}


gboolean gst_buffer_remove_${ELEMENT_NAME_SNAKE_CASE}(GstBuffer *buffer)
{
    g_return_val_if_fail((int)GST_IS_BUFFER(buffer), false);

    ${ELEMENT_OBJECT_NAME} *meta = (${ELEMENT_OBJECT_NAME} *)gst_buffer_get_meta((buffer), GST_ANALYSER_MUX_META_API_TYPE);

    if (meta == NULL)
        return TRUE;

    if (!gst_buffer_is_writable(buffer))
        return FALSE;

    return gst_buffer_remove_meta(buffer, &meta->meta);
}
