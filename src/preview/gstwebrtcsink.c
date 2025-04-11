#include "gstwebrtcsink.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#define DEFAULT_TURN_SERVER ""
#define DEFAULT_STUN_SERVER ""


/* properties */
enum
{
  PROP_0,
  PROP_STUN_SERVER,
  PROP_TURN_SERVER
};


enum {
  SIGNAL_ON_SDP_OFFER = 0,
  SIGNAL_ON_ICE_CANDIDATE,
  SIGNAL_SET_SDP_ANSWER,
  SIGNAL_ADD_ICE_CANDIDATE,
  LAST_SIGNAL
};

static guint gst_webrtc_sink_signals[LAST_SIGNAL] = {0};


#define gst_webrtc_sink_parent_class parent_class

GST_DEBUG_CATEGORY_STATIC (gst_webrtc_sink_debug); 
#define GST_CAT_DEFAULT gst_webrtc_sink_debug


struct _GstWebrtcSink
{
  GstBin parent_instance;
  GstElement *aqueue;
  GstElement *vqueue;

  GstElement *h264parse;
  GstElement *opusparse;

  GstElement *rtph264pay;
  GstElement *rtpopuspay;


  GstElement *webrtcbin;
};

G_DEFINE_TYPE(GstWebrtcSink, gst_webrtc_sink, GST_TYPE_BIN);



static void
on_ice_candidate_cb (GstElement * webrtcbin, guint mline_index,
    gchar * candidate, gpointer user_data)
{
    GstWebrtcSink *self = GST_WEBRTC_SINK(user_data);
    GST_DEBUG("ICE candidate callback triggered for mline index %u", mline_index);

    if (!self || !GST_IS_WEBRTC_SINK(self)) {
        GST_ERROR("Invalid WebRTC sink instance in ICE candidate callback");
        return;
    }

    if (!webrtcbin || !GST_IS_ELEMENT(webrtcbin)) {
        GST_ERROR("Invalid webrtcbin in ICE candidate callback");
        return;
    }

    if (!candidate) {
        GST_ERROR("Invalid ICE candidate string");
        return;
    }

    GST_INFO("New ICE candidate for mline index %u: %s", mline_index, candidate);
    GST_DEBUG("WebRTC bin %p generated ICE candidate", webrtcbin);

    // Verify signal exists before emitting
    if (!g_signal_lookup("on-ice-candidate", G_TYPE_FROM_INSTANCE(self))) {
        GST_ERROR("on-ice-candidate signal not found on WebRTC sink");
        return;
    }

    GST_DEBUG("Emitting on-ice-candidate signal for mline index %u", mline_index);
    g_signal_emit_by_name(self, "on-ice-candidate", mline_index, candidate);
    GST_DEBUG("Successfully emitted on-ice-candidate signal");
}

static void
on_offer_created_cb (GstPromise * promise, gpointer user_data)
{
  if (!promise) {
    GST_ERROR("Invalid promise in offer created callback");
    return;
  }

  gchar *sdp_string;
  GstStructure const *reply;
  GstPromise *local_desc_promise;
  GstWebRTCSessionDescription *offer = NULL;
  GstWebrtcSink *self = GST_WEBRTC_SINK(user_data);

  if (!self) {
    GST_ERROR("Invalid user data in offer created callback");
    return;
  }

  GST_INFO("SDP Offer Created for WebRTC sink %p", self->webrtcbin);

  reply = gst_promise_get_reply (promise);
  if (!reply) {
    GST_ERROR("Invalid reply in promise");
    gst_promise_unref (promise);
    return;
  }

  gst_structure_get (reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
      &offer, NULL);
  gst_promise_unref (promise);

  if (!offer) {
    GST_ERROR("Failed to get offer from reply");
    return;
  }

  GST_DEBUG("Setting local description for WebRTC sink");
  local_desc_promise = gst_promise_new ();
  if (!local_desc_promise) {
    GST_ERROR("Failed to create local description promise");
    gst_webrtc_session_description_free (offer);
    return;
  }

  g_signal_emit_by_name (self->webrtcbin, "set-local-description",
      offer, local_desc_promise);
  gst_promise_interrupt (local_desc_promise);
  gst_promise_unref (local_desc_promise);

  sdp_string = gst_sdp_message_as_text (offer->sdp);
  if (!sdp_string) {
    GST_ERROR("Failed to convert SDP to text");
    gst_webrtc_session_description_free (offer);
    return;
  }

  GST_DEBUG("Generated SDP offer: %s", sdp_string);

  if (!G_IS_OBJECT(self)) {
    GST_ERROR("Invalid GObject when emitting SDP offer signal");
    g_free(sdp_string);
    gst_webrtc_session_description_free (offer);
    return;
  }

  g_signal_emit_by_name(self, "on-sdp-offer", "offer", sdp_string);
  g_free(sdp_string);
  gst_webrtc_session_description_free (offer);
}

static void
on_negotiation_needed_cb (GstElement * webrtcbin, gpointer user_data)
{
  if (!webrtcbin || !GST_IS_ELEMENT(webrtcbin)) {
    GST_ERROR("Invalid webrtcbin in negotiation needed callback");
    return;
  }

  GstPromise *promise;
  GST_INFO("Negotiation needed for WebRTC sink %p", webrtcbin);

  promise = gst_promise_new_with_change_func (on_offer_created_cb,
      (gpointer) user_data, NULL);
  if (!promise) {
    GST_ERROR("Failed to create negotiation promise");
    return;
  }

  GST_DEBUG("Emitting create-offer signal");
  g_signal_emit_by_name (G_OBJECT (webrtcbin), "create-offer", NULL, promise);
}


static void gst_webrtc_sink_init(GstWebrtcSink *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  gulong signal_id;

  GST_INFO("Initializing WebRTC sink");

  self->aqueue = gst_element_factory_make("queue", "qvideo");
  if (!self->aqueue) {
    GST_ERROR("Failed to create audio queue");
    return;
  }
  g_object_set(self->aqueue, "leaky", TRUE, NULL);

  self->vqueue = gst_element_factory_make("queue", "qaudio");
  if (!self->vqueue) {
    GST_ERROR("Failed to create video queue");
    return;
  }
  g_object_set(self->vqueue, "leaky", TRUE, NULL);
  
  GST_INFO("Created audio and video queues");

  self->h264parse = gst_element_factory_make("h264parse", "h264parse");
  self->opusparse = gst_element_factory_make("opusparse", "opusparse");
  if (!self->h264parse || !self->opusparse) {
    GST_ERROR("Failed to create parsers");
    return;
  }

  GST_INFO("Created H264 and Opus parsers");

  self->rtph264pay = gst_element_factory_make("rtph264pay", "rtph264pay");
  self->rtpopuspay = gst_element_factory_make("rtpopuspay", "opuspay");
  if (!self->rtph264pay || !self->rtpopuspay) {
    GST_ERROR("Failed to create RTP payloaders");
    return;
  }

  GST_INFO("Created RTP payloaders");

  self->webrtcbin = gst_element_factory_make("webrtcbin", NULL);
  if (!self->webrtcbin) {
    GST_ERROR("Failed to create webrtcbin");
    return;
  }
  //g_object_set(self->webrtcbin, "bundle-policy", GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE, NULL);

  GST_INFO("Created webrtcbin element");
    
  gst_bin_add_many(bin, self->aqueue, self->opusparse, self->rtpopuspay,
                                          self->vqueue, self->h264parse, self->rtph264pay,
                                          self->webrtcbin, 
                                          NULL);

  GST_INFO("Added elements to bin");

  gst_element_link_many(self->vqueue, self->h264parse, self->rtph264pay, NULL);
  gst_element_link_many(self->aqueue, self->opusparse, self->rtpopuspay, NULL);

  GST_INFO("Linked video and audio pipelines");

  GstCaps *caps = gst_caps_new_simple ("application/x-rtp",
     "media", G_TYPE_STRING, "video",
     "encoding-name", G_TYPE_STRING, "H264",
     "payload", G_TYPE_INT, 96,
        NULL);

  GST_DEBUG("Linking video RTP payloader to webrtcbin");
  gst_element_link_filtered(self->rtph264pay, self->webrtcbin, caps);
  gst_caps_unref(caps);

  caps = gst_caps_new_simple ("application/x-rtp",
     "media", G_TYPE_STRING, "audio",
     "encoding-name", G_TYPE_STRING, "OPUS",
     "payload", G_TYPE_INT, 97,
        NULL);

  GST_DEBUG("Linking audio RTP payloader to webrtcbin");
  gst_element_link_filtered(self->rtpopuspay, self->webrtcbin, caps);
  gst_caps_unref(caps);

  GST_INFO("Configured RTP caps and linked to webrtcbin");
    
  GstWebRTCRTPTransceiver *trans;
  GArray *transceivers;
  g_signal_emit_by_name (self->webrtcbin, "get-transceivers",
      &transceivers);

  GST_INFO("Configuring %d transceivers for send-only mode", transceivers->len);
  for (int i=0; i<transceivers->len; i++){
      trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, i);
      g_object_set (trans, "direction",
          GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, NULL);
  }

  GST_INFO("Connecting WebRTC signals");

  // Connect negotiation needed signal
  signal_id = g_signal_connect (self->webrtcbin, "on-negotiation-needed",
      G_CALLBACK (on_negotiation_needed_cb), (gpointer) self);
  if (signal_id == 0) {
    GST_ERROR("Failed to connect negotiation needed signal - signal not found");
  } else {
    GST_DEBUG("Connected negotiation needed signal with ID %lu", signal_id);
  }

  // Connect ICE candidate signal
  signal_id = g_signal_connect (self->webrtcbin, "on-ice-candidate",
      G_CALLBACK (on_ice_candidate_cb), (gpointer) self);
  if (signal_id == 0) {
    GST_ERROR("Failed to connect ICE candidate signal - signal not found");
  } else {
    GST_DEBUG("Connected ICE candidate signal with ID %lu", signal_id);
  }

  // Verify signal existence
  if (!g_signal_lookup("on-negotiation-needed", G_OBJECT_TYPE(self->webrtcbin))) {
    GST_WARNING("on-negotiation-needed signal not found on webrtcbin");
  }
  if (!g_signal_lookup("on-ice-candidate", G_OBJECT_TYPE(self->webrtcbin))) {
    GST_WARNING("on-ice-candidate signal not found on webrtcbin");
  }

  GST_INFO("WebRTC signal connection completed");

  GstPad *pad = gst_element_get_static_pad(self->aqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("audio_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->vqueue, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("video_sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  GST_INFO("Added ghost pads for audio and video sinks");

  if (signal_id == 0) {
    GST_ERROR("Failed to connect on-sdp-offer signal");
    gst_object_unref(self->webrtcbin);
    self->webrtcbin = NULL;
    return;
  }
}

static void gst_webrtc_sink_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstWebrtcSink *self = GST_WEBRTC_SINK(object);

    switch (prop_id) {
        case PROP_STUN_SERVER:
            g_object_set_property(G_OBJECT(self->webrtcbin), "stun-server", value);
            break;
        case PROP_TURN_SERVER:
            g_object_set_property(G_OBJECT(self->webrtcbin), "turn-server", value);
            break;      
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}

static void gst_webrtc_sink_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstWebrtcSink *self = GST_WEBRTC_SINK(object);

    switch (prop_id) {   
        case PROP_STUN_SERVER:
            g_object_get_property(G_OBJECT(self->webrtcbin), "stun-server", value);
            break;
        case PROP_TURN_SERVER:
            g_object_get_property(G_OBJECT(self->webrtcbin), "turn-server", value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}

static gboolean gst_webrtc_sink_add_ice_candidate(GstWebrtcSink *self, guint mline_index, gchar *candidate_string){
    if (!self || !GST_IS_WEBRTC_SINK(self)) {
        GST_ERROR("Invalid WebRTC sink when adding ICE candidate");
        return FALSE;
    }

    if (!candidate_string) {
        GST_ERROR("Invalid ICE candidate string");
        return FALSE;
    }

    GST_INFO("Adding ICE candidate for mline index %u: %s", mline_index, candidate_string);
    g_signal_emit_by_name (self->webrtcbin, "add-ice-candidate", mline_index, candidate_string);
    return TRUE; 
}

static gboolean gst_webrtc_sink_set_sdp_answer(GstWebrtcSink *self, gchar* type, gchar* sdp){
    if (!self || !GST_IS_WEBRTC_SINK(self)) {
        GST_ERROR("Invalid WebRTC sink when setting SDP answer");
        return FALSE;
    }

    if (!type || !sdp) {
        GST_ERROR("Invalid SDP answer parameters");
        return FALSE;
    }

    GST_INFO("Setting SDP answer of type %s", type);
    GST_DEBUG("SDP answer content: %s", sdp);

    GstPromise *promise = NULL;
    GstSDPMessage *sdp_message = NULL;
    GstWebRTCSessionDescription *answer = NULL;
    gboolean ret = FALSE;
    int parse_ret;

    parse_ret = gst_sdp_message_new (&sdp_message);
    if (parse_ret != GST_SDP_OK){
      GST_ERROR("Failed to create SDP message");
      goto cleanup;
    }

    parse_ret = gst_sdp_message_parse_buffer ((guint8 *) sdp,
        strlen (sdp), sdp_message);
    if (parse_ret != GST_SDP_OK) {
      GST_ERROR("Could not parse SDP string");
      goto cleanup;
    }

    answer = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_ANSWER, sdp_message);
    if (answer == NULL){
      GST_ERROR("Failed to create WebRTC session description");
      goto cleanup;
    }

    promise = gst_promise_new ();
    if (!promise) {
      GST_ERROR("Failed to create promise");
      goto cleanup;
    }

    g_signal_emit_by_name (self->webrtcbin, "set-remote-description",
        answer, promise);
    gst_promise_interrupt (promise);
    
    GST_INFO("Successfully set remote description");
    ret = TRUE;

cleanup:
    if (promise) {
        gst_promise_unref (promise);
    }
    if (answer) {
        gst_webrtc_session_description_free (answer);
    }
    if (sdp_message) {
        //gst_sdp_message_free (sdp_message);
    }

    return ret;
}

static void gst_webrtc_sink_class_init(GstWebrtcSinkClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_webrtc_sink_set_property;
  object_class->get_property = gst_webrtc_sink_get_property;


  g_object_class_install_property(object_class, PROP_TURN_SERVER,
                                  g_param_spec_string("turn-server", "turn-server",
                                                   "turn-server", DEFAULT_TURN_SERVER,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_STUN_SERVER,
                                  g_param_spec_string("stun-server", "stun-server",
                                                   "stun-server", DEFAULT_STUN_SERVER,
                                                   G_PARAM_READWRITE));

  GType record_params[2] = {G_TYPE_UINT, G_TYPE_STRING};
  gst_webrtc_sink_signals[SIGNAL_ADD_ICE_CANDIDATE] =
      g_signal_newv("add-ice-candidate", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_webrtc_sink_add_ice_candidate), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    2, record_params); 

  GType sdp_params[2] = {G_TYPE_STRING, G_TYPE_STRING};
  gst_webrtc_sink_signals[SIGNAL_SET_SDP_ANSWER] =
      g_signal_newv("set-sdp-answer", G_TYPE_FROM_CLASS(klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                    g_cclosure_new(G_CALLBACK(gst_webrtc_sink_set_sdp_answer), NULL, NULL),
                    NULL, NULL, NULL, G_TYPE_BOOLEAN,
                    2, sdp_params); 

  gst_webrtc_sink_signals[SIGNAL_ON_SDP_OFFER] =
      g_signal_new ("on-sdp-offer", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

  gst_webrtc_sink_signals[SIGNAL_ON_ICE_CANDIDATE] =
      g_signal_new ("on-ice-candidate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);      

  GST_DEBUG_CATEGORY_INIT (gst_webrtc_sink_debug, "webrtcsink", 0,
      "WebRTC Sink Debug");


  gst_element_class_set_static_metadata(element_class,
                                        "WebRTCSink Bin",
                                        "WebRTCSink Bin",
                                        "WebRTCSink Bin",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
