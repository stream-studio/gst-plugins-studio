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
  GstElement* webrtcbin;
  GstPreviewSink* parent;
} PreviewSinkReceiverEntry;


G_DEFINE_TYPE(GstPreviewSink, gst_preview_sink, GST_TYPE_BIN);


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
    GstPromise *promise;
    GstSDPMessage *sdp;
    GstWebRTCSessionDescription *answer;
    int ret;

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

    GST_DEBUG ("Received SDP:\n%s\n", sdp_string);

    ret = gst_sdp_message_new (&sdp);
    g_assert_cmphex (ret, ==, GST_SDP_OK);

    ret =
        gst_sdp_message_parse_buffer ((guint8 *) sdp_string,
        strlen (sdp_string), sdp);
    if (ret != GST_SDP_OK) {
      GST_DEBUG ("Could not parse SDP string\n");
      goto cleanup;
    }

    answer = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER,
        sdp);
    g_assert_nonnull (answer);

    promise = gst_promise_new ();
    g_signal_emit_by_name (receiver_entry->webrtcbin, "set-remote-description",
        answer, promise);
    gst_promise_interrupt (promise);
    gst_promise_unref (promise);
  } else if (g_strcmp0 (action_string, "ice") == 0) {
    guint mline_index;
    const gchar *candidate_string;

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
    candidate_string = json_object_get_string_member (data_json_object,
        "candidate");

    g_print ("Received ICE candidate with mline index %u; candidate: %s\n",
        mline_index, candidate_string);

    g_signal_emit_by_name (receiver_entry->webrtcbin, "add-ice-candidate",
        mline_index, candidate_string);
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

  g_assert (receiver_entry != NULL);

  if (receiver_entry->connection != NULL)
    GST_DEBUG("Connection is not null releasing %p, %p", receiver_entry_ptr, receiver_entry->connection);
    g_object_unref (G_OBJECT (receiver_entry->connection));

  if (receiver_entry->bin != NULL){
    GST_DEBUG("Receiver entry is not null releasing %p, %p", receiver_entry_ptr, receiver_entry->connection);

    gboolean result;
    g_signal_emit_by_name(receiver_entry->parent->tee, "stop", receiver_entry->bin, &result);
  }

  g_slice_free1 (sizeof (PreviewSinkReceiverEntry), receiver_entry);
}


static void
on_offer_created_cb (GstPromise * promise, gpointer user_data)
{
  GST_DEBUG("SDP Generated");
  gchar *sdp_string;
  gchar *json_string;
  JsonObject *sdp_json;
  JsonObject *sdp_data_json;
  GstStructure const *reply;
  GstPromise *local_desc_promise;
  GstWebRTCSessionDescription *offer = NULL;
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;

  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
      &offer, NULL);
  gst_promise_unref (promise);

  local_desc_promise = gst_promise_new ();
  g_signal_emit_by_name (receiver_entry->webrtcbin, "set-local-description",
      offer, local_desc_promise);
  gst_promise_interrupt (local_desc_promise);
  gst_promise_unref (local_desc_promise);

  sdp_string = gst_sdp_message_as_text (offer->sdp);
  g_print ("Negotiation offer created:\n%s\n", sdp_string);

  sdp_json = json_object_new ();
  json_object_set_string_member (sdp_json, "action", "sdp");

  sdp_data_json = json_object_new ();
  json_object_set_string_member (sdp_data_json, "type", "offer");
  json_object_set_string_member (sdp_data_json, "sdp", sdp_string);
  json_object_set_object_member (sdp_json, "params", sdp_data_json);

  json_string = get_string_from_json_object (sdp_json);
  json_object_unref (sdp_json);

  soup_websocket_connection_send_text (receiver_entry->connection, json_string);
  g_free (json_string);
  g_free (sdp_string);

  gst_webrtc_session_description_free (offer);
}

void
on_negotiation_needed_cb (GstElement * webrtcbin, gpointer user_data)
{
  GstPromise *promise;
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;
  GST_DEBUG("Negociation needed for receiver entry");
  g_print ("Creating negotiation offer\n");

  promise = gst_promise_new_with_change_func (on_offer_created_cb,
      (gpointer) receiver_entry, NULL);
  g_signal_emit_by_name (G_OBJECT (webrtcbin), "create-offer", NULL, promise);
}



void
on_ice_candidate_cb (G_GNUC_UNUSED GstElement * webrtcbin, guint mline_index,
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

    GstElement *receiver_bin = gst_bin_new(NULL);
    
    GstElement *aqueue = gst_element_factory_make("queue", "qvideo");
    g_object_set(aqueue, "leaky", TRUE, NULL);

    GstElement *vqueue = gst_element_factory_make("queue", "qaudio");
    g_object_set(vqueue, "leaky", TRUE, NULL);
    
    GstElement *h264parse = gst_element_factory_make("h264parse", "h264parse");
    GstElement *opusparse = gst_element_factory_make("opusparse", "opusparse");

    GstElement *rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");

    GstElement *rtpopuspay = gst_element_factory_make("rtpopuspay", "opuspay");
    g_object_set(rtpopuspay, "payload", 127, NULL);
    receiver_entry->webrtcbin = gst_element_factory_make("webrtcbin", NULL);
    receiver_entry->bin = receiver_bin;


    gst_bin_add_many(GST_BIN(receiver_bin), aqueue, opusparse, rtpopuspay,
                                            vqueue, h264parse, rtph264pay,
                                            receiver_entry->webrtcbin, 
                                            NULL);

    gst_element_link_many(vqueue, h264parse, rtph264pay, receiver_entry->webrtcbin, NULL);
    gst_element_link_many(aqueue, opusparse, rtpopuspay, receiver_entry->webrtcbin, NULL);

    GstWebRTCRTPTransceiver *trans;
    GArray *transceivers;
    g_signal_emit_by_name (receiver_entry->webrtcbin, "get-transceivers",
          &transceivers);

    for (int i=0; i<transceivers->len; i++){
        trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, i);
        g_object_set (trans, "direction",
            GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);
    }

    g_signal_connect (receiver_entry->webrtcbin, "on-negotiation-needed",
        G_CALLBACK (on_negotiation_needed_cb), (gpointer) receiver_entry);

    g_signal_connect (receiver_entry->webrtcbin, "on-ice-candidate",
        G_CALLBACK (on_ice_candidate_cb), (gpointer) receiver_entry);

    GstPad *pad = gst_element_get_static_pad(aqueue, "sink");
    gst_element_add_pad(receiver_bin, gst_ghost_pad_new("audio_sink", pad));
    gst_object_unref(GST_OBJECT(pad));

    pad = gst_element_get_static_pad(vqueue, "sink");
    gst_element_add_pad(receiver_bin, gst_ghost_pad_new("video_sink", pad));
    gst_object_unref(GST_OBJECT(pad));

    gboolean result;
    g_signal_emit_by_name(receiver_entry->parent->tee, "start", receiver_bin, &result);

}


PreviewSinkReceiverEntry *
create_receiver_entry (GstPreviewSink *self, SoupWebsocketConnection * connection)
{
  GError *error;
  PreviewSinkReceiverEntry *receiver_entry;

  receiver_entry = g_slice_alloc0 (sizeof (PreviewSinkReceiverEntry));
  receiver_entry->parent = self;
  receiver_entry->connection = connection;
  receiver_entry->webrtcbin = NULL;
  receiver_entry->bin = NULL;

  g_object_ref (G_OBJECT (connection));

  g_signal_connect (G_OBJECT (connection), "message",
      G_CALLBACK (soup_websocket_message_cb), (gpointer) receiver_entry);

  error = NULL;

  return receiver_entry;

cleanup:
  destroy_receiver_entry ((gpointer) receiver_entry);
  return NULL;
}



static void
soup_websocket_handler (G_GNUC_UNUSED SoupServer * server,
    SoupWebsocketConnection * connection, G_GNUC_UNUSED const char *path,
    G_GNUC_UNUSED SoupClientContext * client_context, gpointer user_data)
{
  GstPreviewSink *self = GST_PREVIEW_SINK(user_data);

  GST_INFO ("New WebSocket Connection %p", (gpointer) connection);

  g_signal_connect (G_OBJECT (connection), "closed",
      G_CALLBACK (soup_websocket_closed_cb), (gpointer) self);


  PreviewSinkReceiverEntry *receiver_entry = create_receiver_entry (self, connection);
  g_hash_table_replace (self->receivers, connection, receiver_entry);
}




static gboolean gst_preview_sink_start_server(GstPreviewSink *self){

  self->soup_server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "webrtc-soup-server", NULL);
      
  soup_server_add_websocket_handler (self->soup_server, "/ws", NULL, NULL,
      soup_websocket_handler, (gpointer) self, NULL);
  
  soup_server_listen_all (self->soup_server, self->port,
      (SoupServerListenOptions) 0, NULL);

  return TRUE;
}


static gboolean gst_preview_sink_stop_server(GstPreviewSink *self){
  
  g_object_unref (G_OBJECT (self->soup_server));
  return TRUE;
}

static void gst_preview_sink_init(GstPreviewSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);

  
  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");
  
  self->h264parse = gst_element_factory_make("h264parse", "vparse");
  self->opusparse = gst_element_factory_make("opusparse", "aparse");

  self->tee = gst_element_factory_make("dynamictee", "dtee");
  self->receivers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  gst_bin_add_many(bin, self->aqueue, self->vqueue, self->h264parse, self->opusparse, self->tee, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->tee, NULL);
  gst_element_link_many(self->aqueue, self->opusparse, self->tee, NULL);


  self->host = g_strdup_printf("%s", DEFAULT_HOST);;
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
