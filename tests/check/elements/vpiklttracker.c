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

#include <glib/gprintf.h>

#include "tests/check/test_utils.h"

static const gchar *test_pipes[] = {
  "videotestsrc ! capsfilter caps=video/x-raw,width=1280,height=720,format=GRAY8 ! "
      "vpiupload ! vpiklttracker boxes=\"<<613,332,23,23>, <669,329,30,29>, <790, 376, 41, 22>>\" ! "
      "vpidownload ! fakesink",
  "videotestsrc ! capsfilter caps=video/x-raw,width=1280,height=720,format=GRAY16_LE ! "
      "vpiupload ! vpiklttracker boxes=\"<<613,332,23,23>, <669,329,30,29>, <790, 376, 41, 22>>\" ! "
      "vpidownload ! fakesink",
  "videotestsrc ! capsfilter caps=video/x-raw,width=1280,height=720 ! "
      "vpiupload ! vpiklttracker boxes=\"<<613,332,23>, <669,329,30>, <790, 376, 41, 22>>\" ! "
      "vpidownload ! fakesink",
  "videotestsrc ! vpiupload ! vpiklttracker ! vpidownload ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY8,
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY16,
  TEST_FAIL_WHEN_INVALID_NUMBER_OF_BOX_PARAMS,
  TEST_FAIL_WHEN_NO_BOXES_PROVIDED
};

GST_START_TEST (test_playing_to_null_multiple_times_gray8)
{
  test_states_change (test_pipes[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY8]);
}

GST_END_TEST;

GST_START_TEST (test_playing_to_null_multiple_times_gray_16)
{
  test_states_change (test_pipes[TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY16]);
}

GST_END_TEST;

GST_START_TEST (test_fail_when_invalid_number_of_box_params)
{
  test_fail_properties_configuration (test_pipes
      [TEST_FAIL_WHEN_INVALID_NUMBER_OF_BOX_PARAMS]);
}

GST_END_TEST;

GST_START_TEST (test_fail_when_no_boxes_provided)
{
  test_fail_properties_configuration (test_pipes
      [TEST_FAIL_WHEN_NO_BOXES_PROVIDED]);
}

GST_END_TEST;

GST_START_TEST (test_fail_when_more_than_64_box_provided)
{
  gint num_boxes = 64;
  gchar pipe[2048];
  gchar box[] = "<613,332,23,23>";
  gint i = 0;

  g_sprintf (pipe, "videotestsrc ! capsfilter caps=video/x-raw,width=1280,"
      "height=720 ! vpiupload ! vpiklttracker boxes=\"<");

  for (i = 0; i < num_boxes; i++) {
    g_sprintf (pipe, "%s%s,", pipe, box);
  }
  g_sprintf (pipe, "%s%s>\" ! vpidownload ! fakesink", pipe, box);

  test_fail_properties_configuration (pipe);
}

GST_END_TEST;

static Suite *
gst_vpi_klt_tracker_suite (void)
{
  Suite *suite = suite_create ("vpiklttracker");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_playing_to_null_multiple_times_gray8);
  tcase_add_test (tc, test_playing_to_null_multiple_times_gray_16);
  tcase_add_test (tc, test_fail_when_invalid_number_of_box_params);
  tcase_add_test (tc, test_fail_when_no_boxes_provided);
  tcase_add_test (tc, test_fail_when_more_than_64_box_provided);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_klt_tracker);
