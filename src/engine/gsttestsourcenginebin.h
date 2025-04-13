// src/engine/gsttestsourcenginebin.h
#ifndef __GST_TEST_SRC_ENGINE_BIN_H__
#define __GST_TEST_SRC_ENGINE_BIN_H__

#include "gstenginebinbase.h" // Include the base class header

G_BEGIN_DECLS

#define GST_TYPE_TEST_SRC_ENGINE_BIN (gst_test_src_engine_bin_get_type())
#define GST_TEST_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TEST_SRC_ENGINE_BIN, GstTestSrcEngineBin))
#define GST_TEST_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TEST_SRC_ENGINE_BIN, GstTestSrcEngineBinClass))
#define GST_IS_TEST_SRC_ENGINE_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TEST_SRC_ENGINE_BIN))
#define GST_IS_TEST_SRC_ENGINE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TEST_SRC_ENGINE_BIN))
#define GST_TEST_SRC_ENGINE_BIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_TEST_SRC_ENGINE_BIN, GstTestSrcEngineBinClass))

typedef struct _GstTestSrcEngineBin GstTestSrcEngineBin;
typedef struct _GstTestSrcEngineBinClass GstTestSrcEngineBinClass;

// Inherits from GstEngineBinBase, adds specific source elements
struct _GstTestSrcEngineBin
{
  GstEngineBinBase parent_instance;

  // Elements specific to this subclass
  GstElement *vsource; // Video test source
  GstElement *asource; // Audio test source
};

// Inherits from GstEngineBinBaseClass
struct _GstTestSrcEngineBinClass
{
  GstEngineBinBaseClass parent_class;

   // Padding for future expansion
  gpointer padding[4];
};

// Function to get the GType of this concrete class
GType gst_test_src_engine_bin_get_type(void);

G_END_DECLS

#endif /* __GST_TEST_SRC_ENGINE_BIN_H__ */ 