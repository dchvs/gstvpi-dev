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

#define NUMBER_OF_STATE_CHANGES 5

static const gchar *test_pipes[] = {
  "videotestsrc ! vpiupload ! vpiundistort ! vpidownload ! fakesink",
  "videotestsrc ! vpiupload ! vpiundistort ! vpiundistort ! vpidownload ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_SINGLE_UNDISTORT,
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_DOUBLE_UNDISTORT,
};

static void
states_change (const gchar * pipe_desc)
{
  GstElement *pipeline = NULL;
  GError *error = NULL;
  gint i = 0;

  GST_INFO ("testing pipeline %s", pipe_desc);

  pipeline = gst_parse_launch (pipe_desc, &error);

  /* Check for errors creating pipeline */
  fail_if (error != NULL, error);
  fail_if (pipeline == NULL, error);

  for (i = 0; i < NUMBER_OF_STATE_CHANGES; i++) {

    fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
        GST_STATE_CHANGE_ASYNC);
    fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
        GST_STATE_CHANGE_SUCCESS);
    fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
        GST_STATE_CHANGE_SUCCESS);
  }

  gst_object_unref (pipeline);
}

GST_START_TEST (test_playing_to_null_multiple_times_single_undistort)
{
  states_change (test_pipes
      [TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_SINGLE_UNDISTORT]);
}

GST_END_TEST;

GST_START_TEST (test_playing_to_null_multiple_times_double_undistort)
{
  states_change (test_pipes
      [TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_DOUBLE_UNDISTORT]);
}

GST_END_TEST;

static Suite *
gst_vpi_undistort_suite (void)
{
  Suite *suite = suite_create ("vpiundistort");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_playing_to_null_multiple_times_single_undistort);
  tcase_add_test (tc, test_playing_to_null_multiple_times_double_undistort);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_undistort);
