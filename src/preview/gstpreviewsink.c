#include "gstpreviewsink.h"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/sdp/sdp.h>
#include <libsoup/soup.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include <json-glib/json-glib.h>


#define DEFAULT_HOST "0.0.0.0"
#define DEFAULT_PORT 9000

#define gst_preview_sink_parent_class parent_class

GST_DEBUG_CATEGORY_STATIC (gst_preview_sink_debug); 
#define GST_CAT_DEFAULT gst_preview_sink_debug

/* properties */
enum
{
  PROP_0,
  PROP_PORT,
  PROP_HOST
};

struct _GstPreviewSink
{
  GstBin parent_instance;

  GstElement* aqueue;  
  GstElement* vqueue;
  
  GstElement* h264parse;
  GstElement* opusparse;

  GstElement* tee;
  GHashTable* receivers;

  GSocketAddress* addr;

  gchar* host;
  gint port;

  SoupServer *soup_server;

};

typedef struct{
  SoupWebsocketConnection *connection;
  GstElement* bin;
  GstPreviewSink* parent;
} PreviewSinkReceiverEntry;


G_DEFINE_TYPE(GstPreviewSink, gst_preview_sink, GST_TYPE_BIN);

void play_receiver_entry (PreviewSinkReceiverEntry * receiver_entry);

static gchar *
get_string_from_json_object (JsonObject * object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);
  return text;
}

static void
soup_websocket_closed_cb (SoupWebsocketConnection * connection,
    gpointer user_data)
{

  GstPreviewSink *self = GST_PREVIEW_SINK(user_data);
  g_hash_table_remove (self->receivers, connection);


  GST_INFO ("Closed WebSocket connection %p, now there is %i active connexions\n", (gpointer) connection, g_hash_table_size(self->receivers));
}

void
soup_websocket_message_cb (G_GNUC_UNUSED SoupWebsocketConnection * connection,
    SoupWebsocketDataType data_type, GBytes * message, gpointer user_data)
{
  gsize size;
  gchar *data;
  gchar *data_string;
  const gchar *action_string;
  JsonNode *root_json;
  JsonObject *root_json_object;
  JsonObject *data_json_object;
  JsonParser *json_parser = NULL;
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;

  switch (data_type) {
    case SOUP_WEBSOCKET_DATA_BINARY:
      GST_DEBUG ("Received unknown binary message, ignoring");
      g_bytes_unref (message);
      return;

    case SOUP_WEBSOCKET_DATA_TEXT:
      data = g_bytes_unref_to_data (message, &size);
      /* Convert to NULL-terminated string */
      data_string = g_strndup (data, size);
      GST_DEBUG("Received message, %s", data_string);
      g_free (data);
      break;

    default:
      g_assert_not_reached ();
  }

  json_parser = json_parser_new ();
  if (!json_parser_load_from_data (json_parser, data_string, -1, NULL))
    goto unknown_message;

  root_json = json_parser_get_root (json_parser);
  if (!JSON_NODE_HOLDS_OBJECT (root_json))
    goto unknown_message;

  root_json_object = json_node_get_object (root_json);

  if (!json_object_has_member (root_json_object, "action")) {
    GST_DEBUG ("Received message without type field\n");
    goto cleanup;
  }
  action_string = json_object_get_string_member (root_json_object, "action");

  data_json_object = NULL;

  if (json_object_has_member (root_json_object, "params")) {
    GST_DEBUG ("Received message without data field\n");
    data_json_object = json_object_get_object_member (root_json_object, "params");
  }

  if (g_strcmp0 (action_string, "play") == 0) {
    play_receiver_entry(receiver_entry);
  }else if (g_strcmp0 (action_string, "sdp") == 0) {
    if (!data_json_object){
      GST_DEBUG("SDP action MUST contain params with SDP message");
      goto cleanup;
    }

    const gchar *sdp_type_string;
    const gchar *sdp_string;
    gboolean ret;

    if (!json_object_has_member (data_json_object, "type")) {
      GST_DEBUG ("Received SDP message without type field\n");
      goto cleanup;
    }
    sdp_type_string = json_object_get_string_member (data_json_object, "type");

    if (g_strcmp0 (sdp_type_string, "answer") != 0) {
      GST_DEBUG ("Expected SDP message type \"answer\", got \"%s\"\n",
          sdp_type_string);
      goto cleanup;
    }

    if (!json_object_has_member (data_json_object, "sdp")) {
      GST_DEBUG ("Received SDP message without SDP sing\n");
      goto cleanup;
    }
    sdp_string = json_object_get_string_member (data_json_object, "sdp");


    g_signal_emit_by_name (receiver_entry->bin, "set-sdp-answer", sdp_type_string, sdp_string, &ret);

  } else if (g_strcmp0 (action_string, "ice") == 0) {
    guint mline_index;
    const gchar *candidate_string;
    if (!data_json_object){
      GST_DEBUG("Ice action MUST contain params with Ice message");
      goto cleanup;
    }

    if (!json_object_has_member (data_json_object, "sdpMLineIndex")) {
      GST_DEBUG ("Received ICE message without mline index\n");
      goto cleanup;
    }
    mline_index =
        json_object_get_int_member (data_json_object, "sdpMLineIndex");

    if (!json_object_has_member (data_json_object, "candidate")) {
      GST_DEBUG ("Received ICE message without ICE candidate string\n");
      goto cleanup;
    }
    candidate_string = g_strdup(json_object_get_string_member (data_json_object,
        "candidate"));

    g_print ("Received ICE candidate with mline index %u; candidate: %s\n",
        mline_index, candidate_string);

    gboolean ret;
    g_signal_emit_by_name (receiver_entry->bin, "add-ice-candidate", mline_index, candidate_string, &ret);

  } else
    goto unknown_message;

cleanup:
  if (json_parser != NULL)
    g_object_unref (G_OBJECT (json_parser));
  g_free (data_string);
  return;

unknown_message:
  g_error ("Unknown message \"%s\", ignoring", data_string);
  goto cleanup;
}

void
destroy_receiver_entry (gpointer receiver_entry_ptr)
{
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) receiver_entry_ptr;
  GST_DEBUG("Releasing receiver entry %p, %p", receiver_entry_ptr, receiver_entry->connection);

  if (receiver_entry != NULL){
    if (receiver_entry->connection != NULL){
      GST_DEBUG("Connection is not null releasing %p, %p", receiver_entry_ptr, receiver_entry->connection);
      g_object_unref (G_OBJECT (receiver_entry->connection));
    }
    
    if (receiver_entry->bin != NULL){
      GST_DEBUG("Receiver entry is not null releasing %p, %p", receiver_entry_ptr, receiver_entry->connection);

      gboolean result;
      g_signal_emit_by_name(receiver_entry->parent->tee, "stop", receiver_entry->bin, &result);
    }

    g_slice_free1 (sizeof (PreviewSinkReceiverEntry), receiver_entry);
  }

}


static void gst_preview_sink_on_sdp_offer (GstElement* webrtc, gchar* type, gchar *sdp, gpointer user_data)
{
  GST_DEBUG("SDP Generated");
  gchar *json_string;
  JsonObject *sdp_json;
  JsonObject *sdp_data_json;
  
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;

  sdp_json = json_object_new ();
  json_object_set_string_member (sdp_json, "action", "sdp");

  sdp_data_json = json_object_new ();
  json_object_set_string_member (sdp_data_json, "type", type);
  json_object_set_string_member (sdp_data_json, "sdp", sdp);
  json_object_set_object_member (sdp_json, "params", sdp_data_json);

  json_string = get_string_from_json_object (sdp_json);
  json_object_unref (sdp_json);

  soup_websocket_connection_send_text (receiver_entry->connection, json_string);
  g_free (json_string);

}


static void gst_preview_sink_on_ice_candidate (G_GNUC_UNUSED GstElement * webrtcbin, guint mline_index,
    gchar * candidate, gpointer user_data)
{
  JsonObject *ice_json;
  JsonObject *ice_data_json;
  gchar *json_string;
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;

  ice_json = json_object_new ();
  json_object_set_string_member (ice_json, "action", "ice");

  ice_data_json = json_object_new ();
  json_object_set_int_member (ice_data_json, "sdpMLineIndex", mline_index);
  json_object_set_string_member (ice_data_json, "candidate", candidate);
  json_object_set_object_member (ice_json, "params", ice_data_json);

  json_string = get_string_from_json_object (ice_json);
  json_object_unref (ice_json);

  soup_websocket_connection_send_text (receiver_entry->connection, json_string);
  g_free (json_string);
}

void play_receiver_entry (PreviewSinkReceiverEntry * receiver_entry){
    
    GST_DEBUG("Creating ressources for PLAYING");
    GstElement *sender_bin = gst_element_factory_make("webrtcsink", NULL);
    // TODO - receive STUN + TURN from peer
    g_object_set(sender_bin, "stun-server", "stun://stun.l.google.com:19302", NULL);
    //g_object_set(sender_bin, "turn-server", NULL, NULL);
    receiver_entry->bin = sender_bin;

    g_signal_connect (receiver_entry->bin, "on-sdp-offer",
        G_CALLBACK (gst_preview_sink_on_sdp_offer), (gpointer) receiver_entry);

    g_signal_connect (receiver_entry->bin, "on-ice-candidate",
        G_CALLBACK (gst_preview_sink_on_ice_candidate), (gpointer) receiver_entry);


    gboolean result;
    g_signal_emit_by_name(receiver_entry->parent->tee, "start", sender_bin, &result);

}


PreviewSinkReceiverEntry *
create_receiver_entry (GstPreviewSink *self, SoupWebsocketConnection * connection)
{
  PreviewSinkReceiverEntry *receiver_entry;

  receiver_entry = g_slice_alloc0 (sizeof (PreviewSinkReceiverEntry));
  receiver_entry->parent = self;
  receiver_entry->connection = connection;
  receiver_entry->bin = NULL;

  g_object_ref (G_OBJECT (connection));

  g_signal_connect (G_OBJECT (connection), "message",
      G_CALLBACK (soup_websocket_message_cb), (gpointer) receiver_entry);

  return receiver_entry;
}

static void
soup_websocket_handler (SoupServer *server,
                        SoupServerMessage       *msg,
                        const char *path,
                        SoupWebsocketConnection *connection,
                        gpointer user_data)
{
  GstPreviewSink *self = GST_PREVIEW_SINK(user_data);

  GST_INFO ("New WebSocket Connection %p", (gpointer) connection);

  g_signal_connect (G_OBJECT (connection), "closed",
      G_CALLBACK (soup_websocket_closed_cb), (gpointer) self);

  PreviewSinkReceiverEntry *receiver_entry = create_receiver_entry (self, connection);
  g_hash_table_replace (self->receivers, connection, receiver_entry);
}



static gboolean gst_preview_sink_start_server(GstPreviewSink *self)
{
  GST_INFO ("Libsoup 3.0 server now listening for connections");

  self->soup_server = soup_server_new ("server-header", "webrtc-soup-server", NULL);

  soup_server_add_websocket_handler (self->soup_server, "/ws", NULL, NULL,
      soup_websocket_handler, (gpointer) self, NULL);

  if (!soup_server_listen_all (self->soup_server, self->port, 0, NULL)) {
    GST_ERROR ("Failed to start SoupServer on port %d", self->port);
    return FALSE;
  }

  return TRUE;
}


static gboolean gst_preview_sink_stop_server(GstPreviewSink *self)
{
  if (self->soup_server != NULL) {
    g_object_unref (G_OBJECT (self->soup_server));
    self->soup_server = NULL;
  }
  return TRUE;
}

static void gst_preview_sink_init(GstPreviewSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);

  
  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");
  
  g_object_set(self->aqueue, "leaky", 2, NULL);
  g_object_set(self->vqueue, "leaky", 2, NULL);

  self->h264parse = gst_element_factory_make("h264parse", "vparse");
  self->opusparse = gst_element_factory_make("opusparse", "aparse");

  self->tee = gst_element_factory_make("dynamictee", "dtee");
  self->receivers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  gst_bin_add_many(bin, self->aqueue, self->vqueue, self->h264parse, self->opusparse, self->tee, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->tee, NULL);
  gst_element_link_many(self->aqueue, self->opusparse, self->tee, NULL);


  self->host = g_strdup_printf("%s", DEFAULT_HOST);
  self->port = DEFAULT_PORT;

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));
}

static GstStateChangeReturn gst_preview_sink_change_state(GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstPreviewSink *self = GST_PREVIEW_SINK(element);

  switch (transition) {
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	  if (!gst_preview_sink_start_server (self))
		return GST_STATE_CHANGE_FAILURE;
	  break;
	default:
	  break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
	return ret;

  switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
	  gst_preview_sink_stop_server (self);
	  break;
	default:
	  break;
  }

  return ret;
}


static void gst_preview_sink_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstPreviewSink *self = GST_PREVIEW_SINK(object);

    switch (prop_id) {
        case PROP_HOST:
            if (self->host != NULL){
              free(self->host);
            }
            self->host = g_strdup_printf("%s", g_value_get_string(value));
          break;
        case PROP_PORT:
            self->port = g_value_get_int(value);
          break;      
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_preview_sink_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstPreviewSink *self = GST_PREVIEW_SINK(object);

    switch (prop_id) { 
        case PROP_HOST:
            g_value_set_string(value, self->host);
          break;        
        case PROP_PORT:
            g_value_set_int(value, self->port);
          break;      
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static void gst_preview_sink_class_init(GstPreviewSinkClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_preview_sink_set_property;
  object_class->get_property = gst_preview_sink_get_property;
  element_class->change_state = gst_preview_sink_change_state;

  g_object_class_install_property(object_class, PROP_HOST,
                                  g_param_spec_string("host", "host",
                                                   "host", DEFAULT_HOST,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_PORT,
                                  g_param_spec_uint("port", "port",
                                                   "port", 1, 65535, DEFAULT_PORT,
                                                   G_PARAM_READWRITE));


  GST_DEBUG_CATEGORY_INIT (gst_preview_sink_debug, "previewsink", 0,
      "Preview Sink Debug");


  gst_element_class_set_static_metadata(element_class,
                                        "Gstreamer WebRTC Preview Server",
                                        "Gstreamer WebRTC Preview Server",
                                        "Gstreamer WebRTC Preview Server",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
