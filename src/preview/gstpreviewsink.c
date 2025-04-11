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
  GMutex receivers_mutex;  // Protects access to receivers hash table

  GSocketAddress* addr;

  gchar* host;
  gint port;

  SoupServer *soup_server;
  GMutex server_mutex;  // Protects server operations

};

typedef struct{
  SoupWebsocketConnection *connection;
  GstElement* bin;
  GstPreviewSink* parent;
  gboolean cleaned_up;
} PreviewSinkReceiverEntry;


G_DEFINE_TYPE(GstPreviewSink, gst_preview_sink, GST_TYPE_BIN);


static void cleanup_receiver_entry_resources(PreviewSinkReceiverEntry*);

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
    PreviewSinkReceiverEntry *receiver_entry = NULL;
    
    g_mutex_lock(&self->receivers_mutex);
    receiver_entry = g_hash_table_lookup(self->receivers, connection);
    if (receiver_entry) {
        // Remove from hash table first to prevent any new operations
        g_hash_table_remove(self->receivers, connection);
        
        // Cleanup resources while still holding the mutex
        cleanup_receiver_entry_resources(receiver_entry);
        if (!receiver_entry->cleaned_up) {
            g_slice_free1(sizeof(PreviewSinkReceiverEntry), receiver_entry);
        }

    }
    g_mutex_unlock(&self->receivers_mutex);

    GST_INFO("Closed WebSocket connection %p, now there is %i active connexions\n", 
             (gpointer) connection, g_hash_table_size(self->receivers));
}


#define SAFE_UNREF(obj) do { if ((obj) != NULL) { g_object_unref(obj); (obj) = NULL; } } while(0)
#define SAFE_FREE(ptr)  do { if ((ptr) != NULL) { g_free(ptr); (ptr) = NULL; } } while(0)

static void
soup_websocket_message_cb (G_GNUC_UNUSED SoupWebsocketConnection * connection,
    SoupWebsocketDataType data_type, GBytes * message, gpointer user_data)
{
    GBytes *safe_message = g_bytes_ref(message);
    PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;
    GstPreviewSink *self = NULL;

    if (!receiver_entry || !receiver_entry->parent) {
        GST_ERROR("Invalid receiver entry");
        g_bytes_unref(message);
        return;
    }


    self = receiver_entry->parent;

    g_mutex_lock(&self->receivers_mutex);
    if (!g_hash_table_contains(self->receivers, connection)) {
        GST_WARNING("Received message for disconnected client");
        g_mutex_unlock(&self->receivers_mutex);
        g_bytes_unref(message);
        return;
    }
    g_mutex_unlock(&self->receivers_mutex);

    if (soup_websocket_connection_get_state(receiver_entry->connection) != SOUP_WEBSOCKET_STATE_OPEN) {
        GST_WARNING("Connection is not open, ignoring message");
        g_bytes_unref(message);
        return;
    }

    gsize size = 0;
    gchar *data = NULL;
    gchar *data_string = NULL;
    const gchar *action_string = NULL;
    JsonNode *root_json = NULL;
    JsonObject *root_json_object = NULL;
    JsonObject *data_json_object = NULL;
    JsonParser *json_parser = NULL;

    GST_DEBUG("WebSocket message received for receiver entry %p", receiver_entry);

    switch (data_type) {
        case SOUP_WEBSOCKET_DATA_BINARY:
            GST_DEBUG ("Received unknown binary message, ignoring");
            g_bytes_unref (safe_message);
            return;

        case SOUP_WEBSOCKET_DATA_TEXT:
            data = g_bytes_unref_to_data(safe_message, &size);
            data_string = g_strndup (data, size);
            
            GST_DEBUG("Received message: %s", data_string ? data_string : "(null)");
            g_free (data);
            break;

        default:
            g_assert_not_reached ();
    }


    json_parser = json_parser_new ();
    if (!json_parser) {
        GST_ERROR("Failed to create JSON parser");
        goto unknown_message;
    }


    if (!data_string || strlen(data_string) == 0) {
        GST_ERROR("Received empty or null JSON data");
        goto unknown_message;
    }

    if (!json_parser_load_from_data (json_parser, data_string, -1, NULL))
        goto unknown_message;

    root_json = json_parser_get_root (json_parser);
    if (!JSON_NODE_HOLDS_OBJECT (root_json))
        goto unknown_message;

    root_json_object = json_node_get_object (root_json);

    if (!json_object_has_member (root_json_object, "action")) {
        GST_DEBUG ("Received message without action field");
        goto cleanup;
    }

    action_string = json_object_get_string_member (root_json_object, "action");
    GST_DEBUG("Processing action: %s", action_string);

    data_json_object = NULL;
    if (json_object_has_member (root_json_object, "params")) {
        data_json_object = json_object_get_object_member (root_json_object, "params");
        if (!data_json_object) {
            GST_WARNING("params field exists but is not an object");
            goto cleanup;
        }
    }

    if (g_strcmp0 (action_string, "play") == 0) {
        GST_INFO("Received play action, setting up WebRTC resources");
        play_receiver_entry(receiver_entry);

    } else if (g_strcmp0 (action_string, "sdp") == 0) {
        if (!data_json_object) {
            GST_ERROR("SDP action requires params field");
            goto cleanup;
        }

        const gchar *sdp_type_string = NULL;
        const gchar *sdp_string = NULL;
        gboolean ret = FALSE;

        if (!json_object_has_member (data_json_object, "type")) {
            GST_ERROR("Received SDP message without type");
            goto cleanup;
        }
        sdp_type_string = json_object_get_string_member (data_json_object, "type");

        if (g_strcmp0 (sdp_type_string, "answer") != 0) {
            GST_ERROR("Expected SDP message type 'answer', got '%s'", sdp_type_string);
            goto cleanup;
        }

        if (!json_object_has_member (data_json_object, "sdp")) {
            GST_ERROR("Received SDP message without sdp string");
            goto cleanup;
        }
        sdp_string = json_object_get_string_member (data_json_object, "sdp");

        if (!sdp_string) {
            GST_ERROR("Received NULL SDP string");
            goto cleanup;
        }

        if (!receiver_entry || !receiver_entry->bin) {
            GST_ERROR("Cannot set SDP answer - receiver entry or bin is NULL");
            goto cleanup;
        }

        gchar *sdp_copy = g_strdup(sdp_string);
        g_signal_emit_by_name (receiver_entry->bin, "set-sdp-answer", sdp_type_string, sdp_copy, &ret);
        if (!ret) {
            GST_ERROR("Failed to set SDP answer");
            g_free(sdp_copy);
        }

    } else if (g_strcmp0 (action_string, "ice") == 0) {
        guint mline_index = 0;
        const gchar *candidate_string = NULL;

        if (!data_json_object) {
            GST_DEBUG("ICE message missing params");
            goto cleanup;
        }

        if (!json_object_has_member (data_json_object, "sdpMLineIndex") ||
            !json_object_has_member (data_json_object, "candidate")) {
            GST_DEBUG("ICE message missing required fields");
            goto cleanup;
        }

        mline_index = json_object_get_int_member (data_json_object, "sdpMLineIndex");
        candidate_string = json_object_get_string_member (data_json_object, "candidate");

        if (!candidate_string) {
            GST_ERROR("ICE candidate string is NULL");
            goto cleanup;
        }

        if (!receiver_entry->bin || !GST_IS_ELEMENT(receiver_entry->bin)) {
            GST_ERROR("Invalid WebRTC bin for ICE");
            goto cleanup;
        }

        gchar *candidate_copy = g_strdup(candidate_string);
        gboolean success = FALSE;
        g_signal_emit_by_name (receiver_entry->bin, "add-ice-candidate", mline_index, candidate_copy, &success);
        g_free(candidate_copy);

        if (!success) {
            GST_ERROR("Failed to add ICE candidate");
        }

    } else {
        GST_WARNING("Unknown action: %s", action_string);
        goto cleanup;
    }

cleanup:
    SAFE_UNREF(json_parser);
    SAFE_FREE(data_string);
    return;

unknown_message:
    GST_ERROR("Unknown message: %s", data_string ? data_string : "(null)");
    goto cleanup;
}

void destroy_receiver_entry (gpointer receiver_entry_ptr)
{
    PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) receiver_entry_ptr;
    if (!receiver_entry) {
        GST_ERROR("Invalid receiver entry pointer");
        return;
    }

    GST_DEBUG("Releasing receiver entry %p", receiver_entry_ptr);

    // Only free the entry itself, resources are already cleaned up
    g_slice_free1(sizeof(PreviewSinkReceiverEntry), receiver_entry);
}


static void gst_preview_sink_on_sdp_offer (GstElement* webrtc, gchar* type, gchar *sdp, gpointer user_data)
{
  PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;
  GstPreviewSink *self = NULL;
  
  if (!receiver_entry || !receiver_entry->parent) {
    GST_ERROR("Invalid receiver entry");
    return;
  }
  
  self = receiver_entry->parent;
  
  // Verify the connection is still in our hash table
  g_mutex_lock(&self->receivers_mutex);
  if (!g_hash_table_contains(self->receivers, receiver_entry->connection)) {
    GST_WARNING("Cannot send SDP offer to disconnected client");
    g_mutex_unlock(&self->receivers_mutex);
    return;
  }
  g_mutex_unlock(&self->receivers_mutex);

  // Check if connection is still open
  if (soup_websocket_connection_get_state(receiver_entry->connection) != SOUP_WEBSOCKET_STATE_OPEN) {
    GST_WARNING("Connection is not open, cannot send SDP offer");
    return;
  }

  GST_INFO("Received SDP offer of type: %s", type);
  gchar *json_string;
  JsonObject *sdp_json;
  JsonObject *sdp_data_json;
  
  sdp_json = json_object_new ();
  json_object_set_string_member (sdp_json, "action", "sdp");

  sdp_data_json = json_object_new ();
  json_object_set_string_member (sdp_data_json, "type", type);
  json_object_set_string_member (sdp_data_json, "sdp", sdp);
  json_object_set_object_member (sdp_json, "params", sdp_data_json);

  json_string = get_string_from_json_object (sdp_json);
  json_object_unref (sdp_json);

  GST_DEBUG("Sending SDP offer to client: %s", json_string);
  soup_websocket_connection_send_text (receiver_entry->connection, json_string);
  g_free (json_string);
}


static void gst_preview_sink_on_ice_candidate (G_GNUC_UNUSED GstElement * webrtcbin, guint mline_index,
    gchar * candidate, gpointer user_data)
{
    PreviewSinkReceiverEntry *receiver_entry = (PreviewSinkReceiverEntry *) user_data;
    GstPreviewSink *self = NULL;
    
    if (!receiver_entry || !receiver_entry->parent) {
        GST_ERROR("Invalid receiver entry");
        return;
    }
    
    self = receiver_entry->parent;
    
    // Verify the connection is still in our hash table
    g_mutex_lock(&self->receivers_mutex);
    if (!g_hash_table_contains(self->receivers, receiver_entry->connection)) {
        GST_WARNING("Cannot send ICE candidate to disconnected client");
        g_mutex_unlock(&self->receivers_mutex);
        return;
    }
    g_mutex_unlock(&self->receivers_mutex);

    // Check if connection is still open
    if (soup_websocket_connection_get_state(receiver_entry->connection) != SOUP_WEBSOCKET_STATE_OPEN) {
        GST_WARNING("Connection is not open, cannot send ICE candidate");
        return;
    }

    if (!candidate || strlen(candidate) == 0) {
      GST_INFO("Received empty ICE candidate, ignoring");
      return;
    }



    GST_DEBUG("Received ICE candidate for mline index %u", mline_index);
    JsonObject *ice_json;
    JsonObject *ice_data_json;
    gchar *json_string;

    ice_json = json_object_new ();
    json_object_set_string_member (ice_json, "action", "ice");

    ice_data_json = json_object_new ();
    json_object_set_int_member (ice_data_json, "sdpMLineIndex", mline_index);
    json_object_set_string_member (ice_data_json, "candidate", candidate);
    json_object_set_object_member (ice_json, "params", ice_data_json);

    json_string = get_string_from_json_object (ice_json);
    json_object_unref (ice_json);

    GST_DEBUG("Sending ICE candidate to client: %s", json_string);
    soup_websocket_connection_send_text (receiver_entry->connection, json_string);
    g_free (json_string);
}

void play_receiver_entry (PreviewSinkReceiverEntry * receiver_entry){
    if (!receiver_entry || !receiver_entry->parent) {
        GST_ERROR("Invalid receiver entry or parent");
        return;
    }
    
    GstPreviewSink *self = receiver_entry->parent;
    
    // Verify the connection is still in our hash table
    g_mutex_lock(&self->receivers_mutex);
    if (!g_hash_table_contains(self->receivers, receiver_entry->connection)) {
        GST_WARNING("Cannot setup WebRTC for disconnected client");
        g_mutex_unlock(&self->receivers_mutex);
        return;
    }
    g_mutex_unlock(&self->receivers_mutex);
    
    GST_INFO("Creating WebRTC resources for PLAYING");
    GstElement *sender_bin = gst_element_factory_make("webrtcsink", NULL);
    if (!sender_bin) {
        GST_ERROR("Failed to create webrtcsink element");
        return;
    }
    
    GST_DEBUG("Created webrtcsink element %p", sender_bin);
    
    // Take ownership of the sender_bin
    gst_object_ref_sink(sender_bin);
    
    // TODO - receive STUN + TURN from peer
    g_object_set(sender_bin, "stun-server", "stun://stun.l.google.com:19302", NULL);
    GST_DEBUG("Configured STUN server");

    // Store the sender_bin in the receiver entry
    g_mutex_lock(&self->receivers_mutex);
    if (receiver_entry->bin) {
        GST_WARNING("Replacing existing WebRTC bin %p with new one %p", receiver_entry->bin, sender_bin);
        gst_object_unref(receiver_entry->bin);
    }
    receiver_entry->bin = sender_bin;
    g_mutex_unlock(&self->receivers_mutex);

    GST_INFO("Created webrtcsink with STUN server");

    // Connect signals
    gulong signal_id = g_signal_connect (sender_bin, "on-sdp-offer",
        G_CALLBACK (gst_preview_sink_on_sdp_offer), (gpointer) receiver_entry);
    if (signal_id == 0) {
        GST_ERROR("Failed to connect on-sdp-offer signal");
        gst_object_unref(sender_bin);
        receiver_entry->bin = NULL;
        return;
    }
    GST_DEBUG("Connected on-sdp-offer signal with ID %lu", signal_id);

    signal_id = g_signal_connect (sender_bin, "on-ice-candidate",
        G_CALLBACK (gst_preview_sink_on_ice_candidate), (gpointer) receiver_entry);
    if (signal_id == 0) {
        GST_ERROR("Failed to connect on-ice-candidate signal");
        gst_object_unref(sender_bin);
        receiver_entry->bin = NULL;
        return;
    }
    GST_DEBUG("Connected on-ice-candidate signal with ID %lu", signal_id);

    GST_INFO("Connected WebRTC signals");

    // Start the sender bin
    gboolean result = FALSE;
    g_signal_emit_by_name(receiver_entry->parent->tee, "start", sender_bin, &result);
    
    if (!result) {
        GST_ERROR("Failed to start WebRTC sender bin");
        gst_object_unref(sender_bin);
        receiver_entry->bin = NULL;
    } else {
        GST_INFO("Successfully started WebRTC sender bin");
    }
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
  
  g_mutex_lock(&self->receivers_mutex);
  g_hash_table_replace (self->receivers, connection, receiver_entry);
  g_mutex_unlock(&self->receivers_mutex);
}



static gboolean gst_preview_sink_start_server(GstPreviewSink *self)
{
  gboolean ret = FALSE;
  
  g_mutex_lock(&self->server_mutex);
  
  GST_INFO ("Libsoup 3.0 server now listening for connections");

  self->soup_server = soup_server_new ("server-header", "webrtc-soup-server", NULL);

  soup_server_add_websocket_handler (self->soup_server, "/ws", NULL, NULL,
      soup_websocket_handler, (gpointer) self, NULL);

  if (!soup_server_listen_all (self->soup_server, self->port, 0, NULL)) {
    GST_ERROR ("Failed to start SoupServer on port %d", self->port);
    g_object_unref (G_OBJECT (self->soup_server));
    self->soup_server = NULL;
  } else {
    ret = TRUE;
  }
  
  g_mutex_unlock(&self->server_mutex);
  return ret;
}


static gboolean gst_preview_sink_stop_server(GstPreviewSink *self)
{
  g_mutex_lock(&self->server_mutex);
  
  if (self->soup_server != NULL) {
    g_object_unref (G_OBJECT (self->soup_server));
    self->soup_server = NULL;
  }
  
  g_mutex_unlock(&self->server_mutex);
  return TRUE;
}

static void gst_preview_sink_init(GstPreviewSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);

  GST_INFO("Initializing preview sink");
  
  g_mutex_init(&self->receivers_mutex);
  g_mutex_init(&self->server_mutex);
  
  self->aqueue = gst_element_factory_make("queue", "aqueue");
  self->vqueue = gst_element_factory_make("queue", "vqueue");
  
  g_object_set(self->aqueue, "leaky", 2, NULL);
  g_object_set(self->vqueue, "leaky", 2, NULL);

  GST_INFO("Created audio and video queues");

  self->h264parse = gst_element_factory_make("h264parse", "vparse");
  self->opusparse = gst_element_factory_make("opusparse", "aparse");

  GST_INFO("Created H264 and Opus parsers");

  self->tee = gst_element_factory_make("dynamictee", "dtee");
  self->receivers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  GST_INFO("Created dynamic tee and receiver hash table");

  gst_bin_add_many(bin, self->aqueue, self->vqueue, self->h264parse, self->opusparse, self->tee, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->tee, NULL);
  gst_element_link_many(self->aqueue, self->opusparse, self->tee, NULL);

  GST_INFO("Added and linked elements in bin");

  self->host = g_strdup_printf("%s", DEFAULT_HOST);
  self->port = DEFAULT_PORT;

  GST_INFO("Set default host: %s, port: %d", self->host, self->port);

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  GST_INFO("Added ghost pads for audio and video sinks");
}

static void cleanup_receiver_entry_resources(PreviewSinkReceiverEntry *receiver_entry)
{
    if (!receiver_entry || receiver_entry->cleaned_up) {
        return;
    }

    GST_INFO("Cleaning up resources for receiver entry %p", receiver_entry);
    receiver_entry->cleaned_up = TRUE; // ✅ Marque comme nettoyé

    // Déconnexion des signaux
    if (GST_IS_ELEMENT(receiver_entry->bin)) {
        g_signal_handlers_disconnect_by_data(receiver_entry->bin, receiver_entry);
    }
    if (SOUP_IS_WEBSOCKET_CONNECTION(receiver_entry->connection)) {
        g_signal_handlers_disconnect_by_data(receiver_entry->connection, receiver_entry);
    }

    // 🔻 Stop and free WebRTC bin
    if (GST_IS_ELEMENT(receiver_entry->bin)) {
        GST_INFO("Stopping and cleaning up WebRTC bin %p", receiver_entry->bin);
        gst_element_set_state(receiver_entry->bin, GST_STATE_NULL);

        gboolean result = FALSE;
        if (receiver_entry->parent && receiver_entry->parent->tee) {
            g_signal_emit_by_name(receiver_entry->parent->tee, "stop", receiver_entry->bin, &result);
        }

        gst_object_unref(receiver_entry->bin);
        receiver_entry->bin = NULL;
    }

    // 🔻 Ferme la WebSocket
    if (SOUP_IS_WEBSOCKET_CONNECTION(receiver_entry->connection)) {
        GST_INFO("Closing WebSocket connection %p", receiver_entry->connection);
        soup_websocket_connection_close(receiver_entry->connection, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
        g_object_unref(receiver_entry->connection);
        receiver_entry->connection = NULL;
    }
}

static void gst_preview_sink_cleanup_all_connections(GstPreviewSink *self)
{
    GHashTableIter iter;
    gpointer key, value;

    GST_INFO("Cleaning up all connections");

    g_mutex_lock(&self->receivers_mutex);
    g_hash_table_iter_init(&iter, self->receivers);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        PreviewSinkReceiverEntry *entry = (PreviewSinkReceiverEntry *)value;
        cleanup_receiver_entry_resources(entry);
        g_slice_free1(sizeof(PreviewSinkReceiverEntry), entry);
    }
    g_hash_table_remove_all(self->receivers);
    g_mutex_unlock(&self->receivers_mutex);
}

static GstStateChangeReturn gst_preview_sink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstPreviewSink *self = GST_PREVIEW_SINK(element);

    switch (transition) {
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            if (!gst_preview_sink_start_server(self))
                return GST_STATE_CHANGE_FAILURE;
            break;
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            gst_preview_sink_cleanup_all_connections(self);
            gst_preview_sink_stop_server(self);
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
            gst_preview_sink_cleanup_all_connections(self);
            break;
        default:
            break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

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

static void gst_preview_sink_finalize(GObject *object)
{
  GstPreviewSink *self = GST_PREVIEW_SINK(object);

  if (self->host) {
    g_free(self->host);
    self->host = NULL;
  }

  g_mutex_clear(&self->receivers_mutex);
  g_mutex_clear(&self->server_mutex);

  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_preview_sink_class_init(GstPreviewSinkClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = gst_preview_sink_set_property;
  object_class->get_property = gst_preview_sink_get_property;
  object_class->finalize = gst_preview_sink_finalize;
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
