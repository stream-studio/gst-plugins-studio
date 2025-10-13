#include "gstrecordsink.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <glib.h>

GST_DEBUG_CATEGORY_STATIC (gst_record_sink_debug);
#define GST_CAT_DEFAULT gst_record_sink_debug

/* Properties */
enum {
  PROP_0,
  PROP_LOCATION,   /* Output HLS directory */
};

enum { LAST_SIGNAL };
static guint gst_record_sink_signals[LAST_SIGNAL] = {0};

#define DEFAULT_LOCATION "."

struct _GstRecordSink {
  GstBin parent_instance;

  /* Elements */
  GstElement *aqueue;
  GstElement *vqueue;
  GstElement *h264parse;
  GstElement *aacparse;
  GstElement *hlssink;

  /* State / property */
  gchar *location_dir; /* Directory where HLS playlist and segments are written */
};

G_DEFINE_TYPE (GstRecordSink, gst_record_sink, GST_TYPE_BIN);

/* --- Helper to update HLS sink properties --- */
static void
gst_record_sink_update_hls_props (GstRecordSink *self)
{
  if (!self->hlssink || !self->location_dir)
    return;

  /* Build full paths for playlist and segment files */
  gchar *playlist = g_build_filename (self->location_dir, "playlist.m3u8", NULL);
  gchar *segments = g_build_filename (self->location_dir, "segment%05d.ts", NULL);

  /* Configure hlssink2 properties */
  g_object_set (self->hlssink,
                "playlist-location", playlist,
                "location",          segments,
                "target-duration",   2,     /* segment duration in seconds */
                "playlist-length",   0,     /* no limit to playlist length */
                "max-files",         0,     /* keep all segments, no deletion */
                NULL);

  g_free (playlist);
  g_free (segments);
}

/* --- GObject property handlers --- */
static void
gst_record_sink_set_property (GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
  GstRecordSink *self = GST_RECORD_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION: {
      const gchar *dir = g_value_get_string (value);
      g_free (self->location_dir);
      self->location_dir = g_strdup (dir ? dir : DEFAULT_LOCATION);
      gst_record_sink_update_hls_props (self);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_record_sink_get_property (GObject *object, guint prop_id,
                              GValue *value, GParamSpec *pspec)
{
  GstRecordSink *self = GST_RECORD_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, self->location_dir ? self->location_dir : DEFAULT_LOCATION);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

/* --- Instance initialization --- */
static void
gst_record_sink_init (GstRecordSink *self)
{
  GstBin *bin = GST_BIN (self);
  GstElement *element = GST_ELEMENT (self);

  self->location_dir = g_strdup (DEFAULT_LOCATION);

  /* Create elements */
  self->aqueue    = gst_element_factory_make ("queue",      "aqueue");
  self->vqueue    = gst_element_factory_make ("queue",      "vqueue");
  self->h264parse = gst_element_factory_make ("h264parse",  "vparse");
  self->aacparse  = gst_element_factory_make ("aacparse",   "aparse");
  self->hlssink   = gst_element_factory_make ("hlssink2",   "hlssink");

  if (!self->aqueue || !self->vqueue || !self->h264parse || !self->aacparse || !self->hlssink) {
    GST_ERROR_OBJECT (self, "Failed to create one or more GStreamer elements");
    return;
  }

  /* Add elements to the bin */
  gst_bin_add_many (bin,
                    self->vqueue, self->h264parse,
                    self->aqueue, self->aacparse,
                    self->hlssink,
                    NULL);

  /* Link queues to their respective parsers */
  if (!gst_element_link (self->vqueue, self->h264parse)) {
    GST_ERROR_OBJECT (self, "Failed to link vqueue -> h264parse");
  }
  if (!gst_element_link (self->aqueue, self->aacparse)) {
    GST_ERROR_OBJECT (self, "Failed to link aqueue -> aacparse");
  }

  /* Link parsed streams to hlssink2 pads ("video" and "audio") */
  if (!gst_element_link_pads (self->h264parse, "src", self->hlssink, "video")) {
    GST_ERROR_OBJECT (self, "Failed to link h264parse:src -> hlssink2:video");
  }
  if (!gst_element_link_pads (self->aacparse, "src", self->hlssink, "audio")) {
    GST_ERROR_OBJECT (self, "Failed to link aacparse:src -> hlssink2:audio");
  }

  /* Create ghost pads for external linking */
  {
    GstPad *pad;

    pad = gst_element_get_static_pad (self->vqueue, "sink");
    gst_element_add_pad (element, gst_ghost_pad_new ("video_sink", pad));
    gst_object_unref (pad);

    pad = gst_element_get_static_pad (self->aqueue, "sink");
    gst_element_add_pad (element, gst_ghost_pad_new ("audio_sink", pad));
    gst_object_unref (pad);
  }

  /* Apply default HLS sink properties */
  gst_record_sink_update_hls_props (self);
}

/* --- Class initialization --- */
static void
gst_record_sink_class_init (GstRecordSinkClass *klass)
{
  GObjectClass     *object_class  = G_OBJECT_CLASS (klass);
  GstElementClass  *element_class = GST_ELEMENT_CLASS (klass);

  object_class->set_property = gst_record_sink_set_property;
  object_class->get_property = gst_record_sink_get_property;

  /* Install "location" property (output directory) */
  g_object_class_install_property (object_class, PROP_LOCATION,
    g_param_spec_string ("location",
                         "Location",
                         "HLS output directory (playlist + segments)",
                         DEFAULT_LOCATION,
                         G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "RecordSink (HLS)",
      "Sink/Recorder",
      "HLS recording bin using hlssink2, linking H264/AAC parsers to audio/video pads",
      "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");

  GST_DEBUG_CATEGORY_INIT (gst_record_sink_debug, "recordsink", 0, "RecordSink HLS");
}