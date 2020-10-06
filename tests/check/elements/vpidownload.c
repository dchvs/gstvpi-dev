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
  "fakesrc ! capsfilter caps=video/x-raw(memory:VPIImage),width=1280,height=720,format=I420,framerate=30/1 ! vpidownload name=download ! fakesink",
  "fakesrc ! capsfilter caps=video/x-raw,width=1280,height=720,format=I420,framerate=30/1 ! vpidownload ! fakesink",
  "fakesrc ! capsfilter caps=video/x-raw(memory:VPIImage),width=1280,height=720,format=I420,framerate=30/1 ! vpidownload ! capsfilter caps=video/x-raw(memory:VPIImage) ! fakesink",
  "fakesrc ! capsfilter caps=video/x-raw(memory:VPIImage),width=1280,height=720,format=I420,framerate=30/1 ! vpidownload ! capsfilter caps=video/x-raw,width=640,height=480 ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_SUCCESS_NEGOTIATION,
  TEST_FAIL_PAD_COMPATIBILITY_SINK,
  TEST_FAIL_PAD_COMPATIBILITY_SRC,
  TEST_FAIL_PAD_COMPATIBILITY_WIDTH_HEIGHT
};

GST_START_TEST (test_success_negotiation)
{
  GstElement *pipeline = NULL;
  GstElement *vpidownload = NULL;
  GError *error = NULL;
  GstPad *sink_pad = NULL;
  GstPad *src_pad = NULL;
  GstCaps *sink_caps = NULL;
  GstCaps *src_caps = NULL;
  GstCaps *src_caps_copy = NULL;

  pipeline = gst_parse_launch (test_pipes[TEST_SUCCESS_NEGOTIATION], &error);

  /* Check for errors creating pipeline */
  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);

  /* Wait for pipeline to preroll, at which point negotiation is finished */
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* Compare negotiated caps */
  vpidownload = gst_bin_get_by_name (GST_BIN (pipeline), "download");

  sink_pad = gst_element_get_static_pad (vpidownload, "sink");
  fail_unless (sink_pad != NULL);
  sink_caps = gst_pad_get_current_caps (sink_pad);
  fail_unless (sink_caps != NULL);
  gst_object_unref (sink_pad);

  src_pad = gst_element_get_static_pad (vpidownload, "src");
  fail_unless (src_pad != NULL);
  src_caps = gst_pad_get_current_caps (src_pad);
  fail_unless (src_caps != NULL);
  gst_object_unref (src_pad);

  /* Sink caps should be equal to the src caps plus the VPIImage feature */
  src_caps_copy = gst_caps_copy (src_caps);
  gst_caps_unref (src_caps);

  gst_caps_set_features (src_caps_copy, 0,
      gst_caps_features_from_string ("memory:VPIImage"));

  fail_unless (gst_caps_is_equal (sink_caps, src_caps_copy));

  gst_caps_unref (sink_caps);
  gst_caps_unref (src_caps_copy);

  /* Clean up */
  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
fail_pad_compatibility (const gchar * pipe_desc)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  pipeline = gst_parse_launch (pipe_desc, &error);

  /* Pipeline creation should fail due to compatibility error */
  fail_if (g_strrstr (error->message, "could not link") == NULL, error);

  gst_object_unref (pipeline);
}

GST_START_TEST (test_fail_pad_compatibility_sink)
{
  fail_pad_compatibility (test_pipes[TEST_FAIL_PAD_COMPATIBILITY_SINK]);
}

GST_END_TEST;

GST_START_TEST (test_fail_pad_compatibility_src)
{
  fail_pad_compatibility (test_pipes[TEST_FAIL_PAD_COMPATIBILITY_SRC]);
}

GST_END_TEST;

GST_START_TEST (test_fail_pad_compatibility_width_height)
{
  fail_pad_compatibility (test_pipes[TEST_FAIL_PAD_COMPATIBILITY_WIDTH_HEIGHT]);
}

GST_END_TEST;

static Suite *
gst_vpi_download_suite (void)
{
  Suite *suite = suite_create ("vpidownload");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_success_negotiation);
  tcase_add_test (tc, test_fail_pad_compatibility_sink);
  tcase_add_test (tc, test_fail_pad_compatibility_src);
  tcase_add_test (tc, test_fail_pad_compatibility_width_height);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_download);
