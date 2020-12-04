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

#define NUMBER_BOXES 5
#define NUMBER_PARAMS 4
#define SLEEP_TIME 500000

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
  "videotestsrc ! capsfilter caps=video/x-raw,width=1280,height=720 ! "
      "vpiupload ! vpiklttracker name=tracker boxes=\"<<613,332,23,23>, "
      "<669,329,30,29>>\" ! vpidownload ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY8,
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY16,
  TEST_FAIL_WHEN_INVALID_NUMBER_OF_BOX_PARAMS,
  TEST_REDEFINING_BOXES_ON_THE_FLY,
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

GST_START_TEST (test_fail_when_redefining_boxes_on_the_fly)
{
  GstElement *pipeline = NULL;
  GstElement *tracker = NULL;
  GValue gst_array = G_VALUE_INIT;
  GValue box = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;
  gint array[NUMBER_BOXES][NUMBER_PARAMS] = { {613, 332, 23, 23}
  , {669, 329, 30, 30}
  , {790, 376, 41, 22}
  , {669, 329, 30, 30}
  , {669, 329, 30, 30}
  };

  g_value_init (&gst_array, GST_TYPE_ARRAY);

  for (i = 0; i < NUMBER_BOXES; i++) {

    g_value_init (&box, GST_TYPE_ARRAY);

    for (j = 0; j < NUMBER_PARAMS; j++) {

      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, array[i][j]);
      gst_value_array_append_value (&box, &value);
      g_value_unset (&value);
    }

    gst_value_array_append_value (&gst_array, &box);
    g_value_unset (&box);
  }

  pipeline =
      test_create_pipeline (test_pipes[TEST_REDEFINING_BOXES_ON_THE_FLY]);

  tracker = gst_bin_get_by_name (GST_BIN (pipeline), "tracker");

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* Test to process the old boxes and the new boxes for a while */
  g_usleep (SLEEP_TIME);

  g_object_set_property (G_OBJECT (tracker), "boxes", &gst_array);

  g_usleep (SLEEP_TIME);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
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
  tcase_add_test (tc, test_fail_when_more_than_64_box_provided);
  tcase_add_test (tc, test_fail_when_redefining_boxes_on_the_fly);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_klt_tracker);
