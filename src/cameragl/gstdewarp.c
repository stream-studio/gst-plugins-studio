#include "gstdewarp.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#define DEFAULT_FX 0.0
#define MIN_FX 0.0
#define MAX_FX 4.0


#define DEFAULT_FY 0.0
#define MIN_FY 0.0
#define MAX_FY 4.0

#define DEFAULT_SCALE 1.0
#define MIN_SCALE 0.0
#define MAX_SCALE 2.0 

#define DEFAULT_A 1.0
#define MIN_A 0.0
#define MAX_A 4.0 

#define DEFAULT_B 1.0
#define MIN_B 0.0
#define MAX_B 4.0 

#define DEFAULT_X 1.0
#define MIN_X 0.0
#define MAX_X 2.0 

#define DEFAULT_Y 1.0
#define MIN_Y 0.0
#define MAX_Y 2.0 

static const gchar* vertex_shader = 
      "#version 100\n"
      "attribute vec4 a_position;\n"
      "attribute vec2 a_texcoord;\n"
      "varying vec2 v_texcoord;\n"
      "varying vec3 vPosition;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    vPosition = a_position.xyz;\n"
      "    gl_Position = a_position;\n"
      "    v_texcoord = a_texcoord;\n"
      "}\n";


static const gchar* fragment_shader = 
"#version 100\n" 
"#ifdef GL_ES\n"
" precision mediump float;\n"
"#endif\n"
"varying vec3 vPosition; \n"
"varying vec2 v_texcoord; \n"
"uniform sampler2D tex; \n"
"uniform float time; \n"
"uniform float width; \n"
"uniform float height; \n"
"uniform float a; \n"
"uniform float b; \n"
"uniform float fx; \n"
"uniform float fy; \n"
"uniform float scale; \n"
"uniform float x; \n"
"uniform float y; \n"
"\n"
"// a: 0.328, b: 0.339, Fx: 0.02, Fy: 0.04, scale: 0.343, x: 1.003, y: 0.999\n"
"vec2 GLCoord2TextureCoord(vec2 glCoord) { \n"
"    return glCoord  * vec2(1.0, 1.0)/ 2.0 + vec2(0.5, 0.5); \n"
"}\n"
"void main () {\n"
"    vec3 vPos = vPosition;\n"
"    vec3 uLensS = vec3(a, b, scale);\n"
"    vec2 vMapping = vPos.xy;\n"
"    vMapping.x = vMapping.x + ((pow(vPos.y, 2.0)/scale)*vPos.x/scale)*-fx;\n"
"    vMapping.y = vMapping.y + ((pow(vPos.x, 2.0)/scale)*vPos.y/scale)*-fy;\n"
"    vMapping = vMapping * uLensS.xy;\n"
"    vMapping = GLCoord2TextureCoord(vMapping/scale);\n"
"    vec4 texture = texture2D(tex, vMapping);\n"
"    if(vMapping.x > 0.99 || vMapping.x < 0.01 || vMapping.y > 0.99 || vMapping.y < 0.01){\n"
"        texture = vec4(0.0, 0.0, 0.0, 1.0);\n"
"    }\n"
"    gl_FragColor = texture;\n"
"}\n";

GST_DEBUG_CATEGORY_STATIC (gst_dewarp_debug); 
#define GST_CAT_DEFAULT gst_dewarp_debug

/* properties */
enum
{
  PROP_0,
  PROP_FX,
  PROP_FY,
  PROP_X,
  PROP_Y,
  PROP_A,
  PROP_B,
  PROP_SCALE,


};

struct _GstDewarp
{
  GstBin parent_instance;
  GstElement* shader;
  GstStructure *uniforms;
};

G_DEFINE_TYPE(GstDewarp, gst_dewarp, GST_TYPE_BIN);
  

static void gst_dewarp_init(GstDewarp *self)
{
  GstBin *bin = GST_BIN(self);
  GstElement *element = GST_ELEMENT(self);
  self->shader = gst_element_factory_make("glshader", "shader");
  
  gst_bin_add(bin, self->shader);

  self->uniforms = gst_structure_new("uniforms", 
                      "x", G_TYPE_FLOAT, DEFAULT_X,
                      "y", G_TYPE_FLOAT, DEFAULT_Y, 
                      "fx", G_TYPE_FLOAT, DEFAULT_FX,
                      "fy", G_TYPE_FLOAT, DEFAULT_FY,
                      "a", G_TYPE_FLOAT, DEFAULT_A,
                      "b", G_TYPE_FLOAT, DEFAULT_B,  
                      "scale", G_TYPE_FLOAT, DEFAULT_SCALE, 
                      NULL
                   );

  g_object_set(self->shader, 
               "fragment", fragment_shader, 
               "vertex", vertex_shader, 
               "uniforms", self->uniforms, 
               NULL);
  
  GstPad *pad = gst_element_get_static_pad(self->shader, "sink");
  gst_element_add_pad(element, gst_ghost_pad_new("sink", pad));
  gst_object_unref(GST_OBJECT(pad));

  pad = gst_element_get_static_pad(self->shader, "src");
  gst_element_add_pad(element, gst_ghost_pad_new("src", pad));
  gst_object_unref(GST_OBJECT(pad));

}

static void get_value(GstDewarp *self, const gchar* key, GValue *dest){
  const GValue *value = gst_structure_get_value(self->uniforms, key);
  g_value_set_float(dest, g_value_get_float(value));
}


static void set_value(GstDewarp *self, const gchar* key, const GValue* value){
    gst_structure_set_value(self->uniforms, key, value);
    g_object_set(self->shader, "uniforms", self->uniforms, NULL);
}

static void gst_dewarp_set_property(GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec){
    GstDewarp *self = GST_DEWARP(object);
    

    switch (prop_id) {
        case PROP_A:
            set_value(self, "a", value);
          break;
        case PROP_B:
            set_value(self, "b", value);
          break;
        case PROP_FX:
            set_value(self, "fx", value);
          break;
        case PROP_FY:
            set_value(self, "fy", value);
          break;
        case PROP_SCALE:
            set_value(self, "scale", value);
          break;
        case PROP_X: 
            set_value(self, "x", value);
          break;
        case PROP_Y:
            set_value(self, "y", value);
          break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;    
    }

}


static void gst_dewarp_get_property(GObject *object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec){

    GstDewarp *self = GST_DEWARP(object);

    switch (prop_id) {
        case PROP_A:
          get_value(self, "a", value);
          break;
        case PROP_B:
          get_value(self, "b", value);
          break;
        case PROP_FX:
          get_value(self, "fx", value);
          break;
        case PROP_FY:
          get_value(self, "fy", value);
          break;
        case PROP_SCALE:
          get_value(self, "scale", value);
          break;
        case PROP_X:
          get_value(self, "x", value); 
          break;
        case PROP_Y:
          get_value(self, "y", value);
          break;         
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

}


static void gst_dewarp_finalize(GObject *object){
  GstDewarp *self = GST_DEWARP(object);
  gst_structure_free(self->uniforms);
}

static void gst_dewarp_class_init(GstDewarpClass *klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = gst_dewarp_set_property;
  object_class->get_property = gst_dewarp_get_property;
  object_class->finalize = gst_dewarp_finalize;

  g_object_class_install_property(object_class, PROP_FX,
                                  g_param_spec_float("fx", "fx",
                                                   "fx value", MIN_FX, MAX_FY, DEFAULT_FX,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_FY,
                                  g_param_spec_float("fy", "Location",
                                                   "fy value", MIN_FY, MAX_FY, DEFAULT_FY,
                                                   G_PARAM_READWRITE));
                                                   
  g_object_class_install_property(object_class, PROP_X,
                                  g_param_spec_float("x", "x",
                                                   "X Value", MIN_X, MAX_X, DEFAULT_X,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_Y,
                                  g_param_spec_float("y", "y",
                                                   "Y Value", MIN_Y, MAX_Y, DEFAULT_Y,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_A,
                                  g_param_spec_float("a", "a",
                                                   "A Value", MIN_A, MAX_A, DEFAULT_A,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_B,
                                  g_param_spec_float("b", "b",
                                                   "B Value", MIN_B, MAX_B, DEFAULT_B,
                                                   G_PARAM_READWRITE));

  g_object_class_install_property(object_class, PROP_SCALE,
                                  g_param_spec_float("scale", "Scale",
                                                   "Scale Value", MIN_SCALE, MAX_SCALE, DEFAULT_SCALE,
                                                   G_PARAM_READWRITE));
                                               

  gst_element_class_set_static_metadata(element_class,
                                        "dewarp",
                                        "dewarp",
                                        "dewarp",
                                        "Ludovic Bouguerra <ludovic.bouguerra@stream.studio>");
}
