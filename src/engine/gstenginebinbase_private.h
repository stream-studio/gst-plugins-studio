// src/engine/gstenginebinbase_private.h
#ifndef __GST_ENGINE_BIN_BASE_PRIVATE_H__
#define __GST_ENGINE_BIN_BASE_PRIVATE_H__

#include <gst/gst.h> // For GstPad

// This header defines the private structure for GstEngineBinBase
// It is included by subclasses to access private members like target pads.

struct _GstEngineBinBasePrivate {
  GstPad *video_target_pad; // Target pad for video source (encoder sink or venctee sink)
  GstPad *audio_target_pad; // Target pad for audio source (atee sink)

  // Add other private members if needed in the future
  gpointer padding[4];
};

#endif /* __GST_ENGINE_BIN_BASE_PRIVATE_H__ */ 