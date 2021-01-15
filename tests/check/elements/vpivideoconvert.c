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

#include <gst/check/gstharness.h>

#include "tests/check/test_utils.h"

static const gchar *test_pipes[] = {
  "videotestsrc ! video/x-raw,format=BGR ! vpiupload ! vpivideoconvert ! vpidownload ! video/x-raw,format=RGB ! fakesink",
  "videotestsrc ! video/x-raw,format=BGR,width=320,height=240 ! vpiupload ! vpivideoconvert ! vpidownload ! video/x-raw,format=RGB,width=640,height=480 ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES,
  TEST_BLOCK_RESOLUTION_CHANGE,
};

GST_START_TEST (test_playing_to_null_multiple_times)
{
  test_states_change (test_pipes[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES]);
}

GST_END_TEST;

GST_START_TEST (test_bypass_on_same_caps)
{
  GstHarness *h;
  GstBuffer *in_buf;
  GstBuffer *out_buf;
  const gchar *caps =
      "video/x-raw(memory:VPIImage),format=GRAY8,width=320,height=240,framerate=30/1";
  const gsize size = 320 * 240;

  h = gst_harness_new ("vpivideoconvert");

  /* Define caps */
  gst_harness_set_src_caps_str (h, caps);
  gst_harness_set_sink_caps_str (h, caps);

  /* Create a dummy buffer */
  in_buf = gst_harness_create_buffer (h, size);

  /* Push the buffer */
  gst_harness_push (h, in_buf);

  /* Pull out the buffer */
  out_buf = gst_harness_pull (h);

  /* validate the buffer in is the same as buffer out */
  fail_unless (in_buf == out_buf);

  /* cleanup */
  gst_buffer_unref (out_buf);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_block_resolution_change)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;

  pipeline =
      gst_parse_launch (test_pipes[TEST_BLOCK_RESOLUTION_CHANGE], &error);

  GST_ERROR ("%d", error->code);

  /* Check for errors creating pipeline */
  fail_if (error == NULL);
  assert_equals_int (3, error->code);

  gst_object_unref (pipeline);
}

GST_END_TEST;

static Suite *
gst_vpi_video_convert_suite (void)
{
  Suite *suite = suite_create ("vpivideoconvert");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_playing_to_null_multiple_times);
  tcase_add_test (tc, test_bypass_on_same_caps);
  tcase_add_test (tc, test_block_resolution_change);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_video_convert);
