#ifndef __GST_ENGINE_BIN_BASE_H__
#define __GST_ENGINE_BIN_BASE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_ENGINE_BIN_BASE (gst_engine_bin_base_get_type())
#define GST_ENGINE_BIN_BASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ENGINE_BIN_BASE, GstEngineBinBase))
#define GST_ENGINE_BIN_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ENGINE_BIN_BASE, GstEngineBinBaseClass))
#define GST_IS_ENGINE_BIN_BASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ENGINE_BIN_BASE))
#define GST_IS_ENGINE_BIN_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ENGINE_BIN_BASE))
#define GST_ENGINE_BIN_BASE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_ENGINE_BIN_BASE, GstEngineBinBaseClass))

// Signals enum for clarity
enum
{
  SIGNAL_START_RECORD = 0,
  SIGNAL_STOP_RECORD,
  SIGNAL_START_STREAM,
  SIGNAL_STOP_STREAM,
  LAST_SIGNAL
};

typedef struct _GstEngineBinBase GstEngineBinBase;
typedef struct _GstEngineBinBaseClass GstEngineBinBaseClass;
typedef struct _GstEngineBinBasePrivate GstEngineBinBasePrivate;

struct _GstEngineBinBase
{
  GstBin parent_instance;

  // Common elements - Accessible by subclasses
  GstElement *atee;

  gchar *video_encoder_name;
  GstElement *video_encoder; // The H264 encoder

  GstElement *venctee;

  // Common audio path elements (before specific encoder)
  GstElement *aacqueue;
  GstElement *aacconvert;

  // Publish audio encoder (dynamic: AAC, Opus, etc.)
  gchar *audio_encoder_name;
  GstElement *audio_encoder;

  // Preview audio path elements (always Opus)
  GstElement *opusqueue;
  GstElement *opusconvert;
  GstElement *opusencoder; // Opus encoder specifically for preview

  // Preview elements
  GstElement *qvpreview;
  GstElement *preview; // previewsink

  // Publish elements
  GstElement *qvpublishtee;
  GstElement *publish; // publishbin

  // Private data
  GstEngineBinBasePrivate *priv;
};

struct _GstEngineBinBaseClass
{
  GstBinClass parent_class;

  // --- Virtual Methods --- 
  // Subclasses implement these to handle their specific source setup or sink pads

  // For source elements (testsrc, appsrc)
  gboolean (*create_sources) (GstEngineBinBase *self);
  gboolean (*link_sources) (GstEngineBinBase *self);

  // For sink pads (pad)
  gboolean (*create_sink_pads) (GstEngineBinBase *self);

  // --- Signals --- 
  guint signals[LAST_SIGNAL]; // Array to hold signal IDs

  // --- Padding for future expansion --- 
  gpointer padding[4];
};

// Function to get the GType of the abstract base class
GType gst_engine_bin_base_get_type(void);

// Helper function potentially useful for subclasses or external code
void gst_engine_bin_base_recreate_audio_encoder(GstEngineBinBase *self);


G_END_DECLS

#endif /* __GST_ENGINE_BIN_BASE_H__ */ 