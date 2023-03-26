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

/* 
 * Test that the pad numbering assigned by aggregator behaves as follows:
 * 1. If a pad number is requested, it must be assigned if it is available
 * 2. When numbering automatically, the largest available pad number is used
 * 3. Pad names must be unique
 */
GST_START_TEST (test_stream)
{
  GstElement *streamsink;
  GST_INFO ("preparing test");
  streamsink = gst_element_factory_make ("streamsink", NULL);

  /* cleanup */
  gst_object_unref (streamsink);
}

GST_END_TEST;


static Suite * stream_suite(){
    Suite *s = suite_create ("streamsink");
    TCase *tc_chain = tcase_create ("general");
    suite_add_tcase (s, tc_chain);
    tcase_add_test (tc_chain, test_stream);

    return s;
}

GST_CHECK_MAIN (stream);