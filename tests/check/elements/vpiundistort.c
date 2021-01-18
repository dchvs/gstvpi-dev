/*
 * Copyright (C) 2020-2021 RidgeRun, LLC (http://www.ridgerun.com)
 * All Rights Reserved.
 *
 * The contents of this software are proprietary and confidential to RidgeRun,
 * LLC.  No part of this program may be photocopied, reproduced or translated
 * into another programming language without prior written consent of
 * RidgeRun, LLC.  The user is free to modify the source code after obtaining
 * a software license from RidgeRun.  All source code changes must be provided
 * back to RidgeRun without any encumbrance.
 */

#include "tests/check/test_utils.h"

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

GST_START_TEST (test_playing_to_null_multiple_times_single_undistort)
{
  test_states_change (test_pipes
      [TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_SINGLE_UNDISTORT]);
}

GST_END_TEST;

GST_START_TEST (test_playing_to_null_multiple_times_double_undistort)
{
  test_states_change (test_pipes
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
