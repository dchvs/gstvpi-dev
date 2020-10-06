/*
 * Copyright (C) 2020 RidgeRun, LLC (http://www.ridgerun.com)
 * All Rights Reserved.
 *
 * The contents of this software are proprietary and confidential to RidgeRun,
 * LLC.  No part of this program may be photocopied, reproduced or translated
 * into another programming language without prior written consent of
 * RidgeRun, LLC.  The user is free to modify the source code after obtaining
 * a software license from RidgeRun.  All source code changes must be provided
 * back to RidgeRun without any encumbrance.
 */

#include <gst/check/gstcheck.h>

static const gchar *test_pipes[] = {
  "videotestsrc ! vpiupload name=upload ! capsfilter caps=video/x-raw(memory:VPIImage) ! fakesink",
  "fakesrc ! capsfilter caps=video/x-raw,width=1280,height=720,format=I420,framerate=30/1 ! vpiupload ! capsfilter caps=video/x-raw(memory:VPIImage) ! fakesink",
  "videotestsrc ! vpiupload ! capsfilter caps=video/x-raw ! fakesink",
  "videotestsrc ! capsfilter caps=video/x-raw,width=1280,height=720,format=I420,framerate=30/1 ! vpiupload ! capsfilter caps=video/x-raw(memory:VPIImage),width=640,height=480 ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_SUCCESS_POOL_NEGOTIATION,
  TEST_FAIL_POOL_NEGOTIATION,
  TEST_FAIL_CAPS_NEGOTIATION_SRC,
  TEST_FAIL_CAPS_NEGOTIATION_WIDTH_HEIGHT
};

GST_START_TEST (test_success_pool_negotiation)
{
  GstElement *pipeline = NULL;
  GstElement *vpiupload = NULL;
  GError *error = NULL;
  GstPad *sink_pad = NULL;
  GstPad *src_pad = NULL;
  GstCaps *sink_caps = NULL;
  GstCaps *src_caps = NULL;
  GstCaps *sink_caps_copy = NULL;

  pipeline =
      gst_parse_launch (test_pipes[TEST_SUCCESS_POOL_NEGOTIATION], &error);

  /* Check for errors creating pipeline */
  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  /* Wait for pipeline to preroll, at which point negotiation is finished */
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* Compare negotiated caps */
  vpiupload = gst_bin_get_by_name (GST_BIN (pipeline), "upload");

  sink_pad = gst_element_get_static_pad (vpiupload, "sink");
  fail_unless (sink_pad != NULL);
  sink_caps = gst_pad_get_current_caps (sink_pad);
  fail_unless (sink_caps != NULL);
  gst_object_unref (sink_pad);

  src_pad = gst_element_get_static_pad (vpiupload, "src");
  fail_unless (src_pad != NULL);
  src_caps = gst_pad_get_current_caps (src_pad);
  fail_unless (src_caps != NULL);
  gst_object_unref (src_pad);

  /* Src caps should be equal to the sink caps plus the VPIImage feature */
  sink_caps_copy = gst_caps_copy (sink_caps);
  gst_caps_unref (sink_caps);

  gst_caps_set_features (sink_caps_copy, 0,
      gst_caps_features_from_string ("memory:VPIImage"));

  fail_unless (gst_caps_is_equal (src_caps, sink_caps_copy));

  gst_caps_unref (sink_caps);
  gst_caps_unref (sink_caps_copy);

  /* Clean up */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_fail_pool_negotiation)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  pipeline = gst_parse_launch (test_pipes[TEST_FAIL_POOL_NEGOTIATION], &error);

  /* Check for errors creating pipeline */
  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  /* Wait for the element to fail on PLAYING */
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_FAILURE);

  /* Clean up */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
fail_caps_negotiation (const gchar * pipe_desc)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  pipeline = gst_parse_launch (pipe_desc, &error);

  /* Pipeline creation should fail due to negotation error */
  fail_if (g_strrstr (error->message, "could not link") == NULL, error);

  gst_object_unref (pipeline);
}

GST_START_TEST (test_fail_caps_negotiation_src)
{
  fail_caps_negotiation (test_pipes[TEST_FAIL_CAPS_NEGOTIATION_SRC]);
}

GST_END_TEST;

GST_START_TEST (test_fail_caps_negotiation_width_height)
{
  fail_caps_negotiation (test_pipes[TEST_FAIL_CAPS_NEGOTIATION_WIDTH_HEIGHT]);
}

GST_END_TEST;

static Suite *
gst_vpi_upload_suite (void)
{
  Suite *suite = suite_create ("vpiupload");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success_pool_negotiation);
  tcase_add_test (tc, test_fail_pool_negotiation);
  tcase_add_test (tc, test_fail_caps_negotiation_src);
  tcase_add_test (tc, test_fail_caps_negotiation_width_height);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_upload);
