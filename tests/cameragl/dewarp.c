#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/check/gstconsistencychecker.h>
#include <gst/check/gstharness.h>

#include <gst/gst.h>

GST_START_TEST (test_dewarp)
{
  GstElement *dewarp;
  GST_INFO ("preparing test");
  dewarp = gst_element_factory_make ("dewarp", NULL);
  

  /* cleanup */
  gst_object_unref (dewarp);
}

GST_END_TEST;


GST_START_TEST (test_get_set_values)
{
  GstElement *dewarp;
  GST_INFO ("preparing test");
  dewarp = gst_element_factory_make ("dewarp", NULL);
  
  gfloat x, y, fx, fy, scale, a, b;

  g_object_get(dewarp, "x", &x, "y", &y, "fx", &fx, "fy", &fy, "scale", &scale, "a", &a, "b", &b, NULL);
  assert_equals_float(1.0, x);
  assert_equals_float(1.0, y);
  assert_equals_float(1.0, scale);
  assert_equals_float(1.0, a);
  assert_equals_float(1.0, b);
  assert_equals_float(0.0, fx);
  assert_equals_float(0.0, fy);

  g_object_set(dewarp, "x", 1.01, "y", 1.02, "fx", 1.03, "fy", 1.04, "scale", 1.05, "a", 1.06, "b", 1.07, NULL);
  g_object_get(dewarp, "x", &x, "y", &y, "fx", &fx, "fy", &fy, "scale", &scale, "a", &a, "b", &b, NULL);

  assert_equals_float(1.01, x);
  assert_equals_float(1.02, y);
  assert_equals_float(1.03, fx);
  assert_equals_float(1.04, fy);
  assert_equals_float(1.05, scale);
  assert_equals_float(1.06, a);
  assert_equals_float(1.07, b);

  /* cleanup */
  gst_object_unref (dewarp);
}

GST_END_TEST;

static Suite * stream_suite(){
    Suite *s = suite_create ("dewarp");
    TCase *tc_chain = tcase_create ("general");
    suite_add_tcase (s, tc_chain);
    tcase_add_test (tc_chain, test_dewarp);
    tcase_add_test (tc_chain, test_get_set_values);

    return s;
}

GST_CHECK_MAIN (stream);