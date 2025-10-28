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

  GstElement* vtee;
  GstElement* atee;
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


static void cleanup_receiver_entry_resources(PreviewSinkReceiverEntry*, gboolean close_connection);

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
    
    GST_INFO("WebSocket connection %p closed callback triggered", connection);
    
    g_mutex_lock(&self->receivers_mutex);
    guint connections_before = g_hash_table_size(self->receivers);
    GST_INFO("Current connections before cleanup: %u", connections_before);
    
    receiver_entry = g_hash_table_lookup(self->receivers, connection);
    if (receiver_entry) {
        GST_INFO("Found receiver entry %p for closed connection %p", receiver_entry, connection);
        
        // Cleanup resources while still holding the mutex
        GST_INFO("Cleaning up resources for receiver entry %p", receiver_entry);
        cleanup_receiver_entry_resources(receiver_entry, FALSE); // Connection is already closed
        
        if (!receiver_entry->cleaned_up) {
            GST_INFO("Freeing receiver entry %p (not marked as cleaned up)", receiver_entry);
            g_slice_free1(sizeof(PreviewSinkReceiverEntry), receiver_entry);
            GST_INFO("Receiver entry freed");
        } else {
            GST_INFO("Receiver entry %p already marked as cleaned up", receiver_entry);
        }
        
        GST_INFO("Removing connection %p from hash table", connection);
        gboolean removed = g_hash_table_remove(self->receivers, connection);
        GST_INFO("Hash table removal result: %s", removed ? "SUCCESS" : "FAILED");
    } else {
        GST_WARNING("No receiver entry found for closed connection %p", connection);
    }
    
    guint connections_after = g_hash_table_size(self->receivers);
    g_mutex_unlock(&self->receivers_mutex);

    GST_INFO("Closed WebSocket connection %p, connections: %u -> %u", 
             connection, connections_before, connections_after);
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

    // Add sender_bin to the main previewsink bin
    gst_bin_add(GST_BIN(self), sender_bin);
    gst_element_sync_state_with_parent(sender_bin);
    
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

    // Link sender bin to video and audio tees manually
    GstPad *vtee_src_pad = gst_element_request_pad_simple(receiver_entry->parent->vtee, "src_%u");
    GstPad *atee_src_pad = gst_element_request_pad_simple(receiver_entry->parent->atee, "src_%u");
    
    if (!vtee_src_pad || !atee_src_pad) {
        GST_ERROR("Failed to request source pads from tees");
        if (vtee_src_pad) {
            gst_element_release_request_pad(receiver_entry->parent->vtee, vtee_src_pad);
            gst_object_unref(vtee_src_pad);
        }
        if (atee_src_pad) {
            gst_element_release_request_pad(receiver_entry->parent->atee, atee_src_pad);
            gst_object_unref(atee_src_pad);
        }
        gst_object_unref(sender_bin);
        receiver_entry->bin = NULL;
        return;
    }
    
    GstPad *video_sink_pad = gst_element_get_static_pad(sender_bin, "video_sink");
    GstPad *audio_sink_pad = gst_element_get_static_pad(sender_bin, "audio_sink");
    
    if (!video_sink_pad || !audio_sink_pad) {
        GST_ERROR("Failed to get sink pads from WebRTC sender bin (video_sink_pad: %p, audio_sink_pad: %p)", video_sink_pad, audio_sink_pad);
        gst_element_release_request_pad(receiver_entry->parent->vtee, vtee_src_pad);
        gst_element_release_request_pad(receiver_entry->parent->atee, atee_src_pad);
        gst_object_unref(vtee_src_pad);
        gst_object_unref(atee_src_pad);
        if (video_sink_pad) gst_object_unref(video_sink_pad);
        if (audio_sink_pad) gst_object_unref(audio_sink_pad);
        gst_bin_remove(GST_BIN(receiver_entry->parent), sender_bin);
        receiver_entry->bin = NULL;
        return;
    }
    
    GstPadLinkReturn video_link_result = gst_pad_link(vtee_src_pad, video_sink_pad);
    GstPadLinkReturn audio_link_result = gst_pad_link(atee_src_pad, audio_sink_pad);
    
    if (video_link_result != GST_PAD_LINK_OK || audio_link_result != GST_PAD_LINK_OK) {
        GST_ERROR("Failed to link tees to WebRTC sender bin (video_link: %d, audio_link: %d)", video_link_result, audio_link_result);
        gst_element_release_request_pad(receiver_entry->parent->vtee, vtee_src_pad);
        gst_element_release_request_pad(receiver_entry->parent->atee, atee_src_pad);
        gst_object_unref(vtee_src_pad);
        gst_object_unref(atee_src_pad);
        gst_object_unref(video_sink_pad);
        gst_object_unref(audio_sink_pad);
        gst_bin_remove(GST_BIN(receiver_entry->parent), sender_bin);
        receiver_entry->bin = NULL;
        return;
    }
    
    gst_object_unref(vtee_src_pad);
    gst_object_unref(atee_src_pad);
    gst_object_unref(video_sink_pad);
    gst_object_unref(audio_sink_pad);
    
    GST_INFO("Successfully linked WebRTC sender bin to video and audio tees");
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

  self->vtee = gst_element_factory_make("tee", "vtee");
  self->atee = gst_element_factory_make("tee", "atee");
  g_object_set(self->vtee, "allow-not-linked", TRUE, NULL);
  g_object_set(self->atee, "allow-not-linked", TRUE, NULL);
  self->receivers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
      destroy_receiver_entry);

  GST_INFO("Created video and audio tees and receiver hash table");

  gst_bin_add_many(bin, self->aqueue, self->vqueue, self->h264parse, self->opusparse, self->vtee, self->atee, NULL);
  gst_element_link_many(self->vqueue, self->h264parse, self->vtee, NULL);
  gst_element_link_many(self->aqueue, self->opusparse, self->atee, NULL);

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

static void cleanup_receiver_entry_resources(PreviewSinkReceiverEntry *receiver_entry, gboolean close_connection)
{
    if (!receiver_entry || receiver_entry->cleaned_up) {
        return;
    }

    GST_INFO("Cleaning up resources for receiver entry %p (close_connection: %s)", receiver_entry, close_connection ? "TRUE" : "FALSE");
    receiver_entry->cleaned_up = TRUE; // ✅ Marque comme nettoyé



    if (GST_IS_ELEMENT(receiver_entry->bin)) {
        GST_INFO("Stopping and cleaning up WebRTC bin %p", receiver_entry->bin);

        // Find and unlink the tee pads connected to this bin
        GST_INFO("Finding and unlinking tee pads for WebRTC bin %p", receiver_entry->bin);
        GstPad *video_sink_pad = gst_element_get_static_pad(receiver_entry->bin, "video_sink");
        GstPad *audio_sink_pad = gst_element_get_static_pad(receiver_entry->bin, "audio_sink");
        
        GST_INFO("Retrieved sink pads: video_sink_pad=%p, audio_sink_pad=%p", video_sink_pad, audio_sink_pad);
        
        if (video_sink_pad) {
            GstPad *vtee_src_pad = gst_pad_get_peer(video_sink_pad);
            GST_INFO("Video sink pad peer: vtee_src_pad=%p", vtee_src_pad);
            if (vtee_src_pad) {
                GST_INFO("Unlinking video pads: %s:%s -> %s:%s", 
                         GST_DEBUG_PAD_NAME(vtee_src_pad), GST_DEBUG_PAD_NAME(video_sink_pad));
                gboolean unlink_result = gst_pad_unlink(vtee_src_pad, video_sink_pad);
                GST_INFO("Video pad unlink result: %s", unlink_result ? "SUCCESS" : "FAILED");
                
                GST_INFO("Releasing request pad %s:%s from vtee", GST_DEBUG_PAD_NAME(vtee_src_pad));
                gst_element_release_request_pad(receiver_entry->parent->vtee, vtee_src_pad);
                GST_INFO("Released video tee request pad");
                
                gst_object_unref(vtee_src_pad);
                GST_INFO("Unreferenced video tee source pad");
            } else {
                GST_WARNING("Video sink pad has no peer - may already be unlinked");
            }
            gst_object_unref(video_sink_pad);
            GST_INFO("Unreferenced video sink pad");
        } else {
            GST_WARNING("Could not get video sink pad from WebRTC bin");
        }
        
        if (audio_sink_pad) {
            GstPad *atee_src_pad = gst_pad_get_peer(audio_sink_pad);
            GST_INFO("Audio sink pad peer: atee_src_pad=%p", atee_src_pad);
            if (atee_src_pad) {
                GST_INFO("Unlinking audio pads: %s:%s -> %s:%s", 
                         GST_DEBUG_PAD_NAME(atee_src_pad), GST_DEBUG_PAD_NAME(audio_sink_pad));
                gboolean unlink_result = gst_pad_unlink(atee_src_pad, audio_sink_pad);
                GST_INFO("Audio pad unlink result: %s", unlink_result ? "SUCCESS" : "FAILED");
                
                GST_INFO("Releasing request pad %s:%s from atee", GST_DEBUG_PAD_NAME(atee_src_pad));
                gst_element_release_request_pad(receiver_entry->parent->atee, atee_src_pad);
                GST_INFO("Released audio tee request pad");
                
                gst_object_unref(atee_src_pad);
                GST_INFO("Unreferenced audio tee source pad");
            } else {
                GST_WARNING("Audio sink pad has no peer - may already be unlinked");
            }
            gst_object_unref(audio_sink_pad);
            GST_INFO("Unreferenced audio sink pad");
        } else {
            GST_WARNING("Could not get audio sink pad from WebRTC bin");
        }

        GST_INFO("Removing WebRTC bin %p from parent bin %p", receiver_entry->bin, receiver_entry->parent);
        gboolean remove_result = gst_bin_remove(GST_BIN(receiver_entry->parent), receiver_entry->bin);
        GST_INFO("WebRTC bin removal result: %s", remove_result ? "SUCCESS" : "FAILED");

        GST_INFO("Setting WebRTC bin %p state to NULL", receiver_entry->bin);
        GstStateChangeReturn state_ret = gst_element_set_state(receiver_entry->bin, GST_STATE_NULL);
        GST_INFO("WebRTC bin state change result: %s", gst_element_state_change_return_get_name(state_ret));
        

        
        GST_INFO("Setting receiver_entry->bin to NULL");
        receiver_entry->bin = NULL;
    }

    if (SOUP_IS_WEBSOCKET_CONNECTION(receiver_entry->connection)) {
        SoupWebsocketState conn_state = soup_websocket_connection_get_state(receiver_entry->connection);
        GST_INFO("WebSocket connection %p current state: %d", receiver_entry->connection, conn_state);
        
        if (close_connection && conn_state == SOUP_WEBSOCKET_STATE_OPEN) {
            GST_INFO("Actively closing open WebSocket connection %p", receiver_entry->connection);
            soup_websocket_connection_close(receiver_entry->connection, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
            GST_INFO("WebSocket connection close request sent");
        } else {
            GST_INFO("Not closing WebSocket connection %p (close_connection: %s, state: %d)", 
                     receiver_entry->connection, close_connection ? "TRUE" : "FALSE", conn_state);
        }
        
        GST_INFO("Unreferencing WebSocket connection %p", receiver_entry->connection);
        g_object_unref(receiver_entry->connection);
        GST_INFO("WebSocket connection unreferenced");
        receiver_entry->connection = NULL;
    } else {
        GST_WARNING("receiver_entry->connection is not a valid WebSocket connection");
    }
}

static void gst_preview_sink_cleanup_all_connections(GstPreviewSink *self)
{
    GHashTableIter iter;
    gpointer key, value;
    guint connection_count = 0;

    GST_INFO("Cleaning up all connections");

    g_mutex_lock(&self->receivers_mutex);
    connection_count = g_hash_table_size(self->receivers);
    GST_INFO("Found %u connections to clean up", connection_count);
    
    g_hash_table_iter_init(&iter, self->receivers);
    guint cleaned = 0;
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        PreviewSinkReceiverEntry *entry = (PreviewSinkReceiverEntry *)value;
        GST_INFO("Cleaning up connection %u/%u: entry=%p, connection=%p, bin=%p", 
                 ++cleaned, connection_count, entry, entry ? entry->connection : NULL, entry ? entry->bin : NULL);
        cleanup_receiver_entry_resources(entry, TRUE); // Actively close connections
        GST_INFO("Freeing receiver entry %p", entry);
        g_slice_free1(sizeof(PreviewSinkReceiverEntry), entry);
        GST_INFO("Receiver entry freed");
    }
    
    GST_INFO("Removing all entries from hash table");
    g_hash_table_remove_all(self->receivers);
    GST_INFO("Hash table cleared, final size: %u", g_hash_table_size(self->receivers));
    g_mutex_unlock(&self->receivers_mutex);
    
    GST_INFO("All connections cleanup completed");
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
                                  g_param_spec_int("port", "port",
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
