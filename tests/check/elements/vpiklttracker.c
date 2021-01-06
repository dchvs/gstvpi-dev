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

#define MAX_BOXES 64
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
      "vpiupload ! vpiklttracker name=tracker boxes=\"<<613,332,23,23>, "
      "<669,329,30,29>>\" ! vpidownload ! fakesink",
  NULL,
};

enum
{
  /* test names */
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY8,
  TEST_PLAYING_TO_NULL_MULTIPLE_TIMES_GRAY16,
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

GST_START_TEST (test_discard_when_more_than_64_box_provided)
{
  GstElement *pipeline = NULL;
  GstElement *tracker = NULL;
  gint num_boxes = 68;
  gchar pipe[2048];
  gchar box[] = "<613,332,23,23>";
  gint i = 0;
  GValue get_gst_array = G_VALUE_INIT;

  g_sprintf (pipe, "videotestsrc ! capsfilter caps=video/x-raw,width=1280,"
      "height=720 ! vpiupload ! vpiklttracker name=tracker boxes=\"<");

  for (i = 0; i < num_boxes; i++) {
    g_sprintf (pipe, "%s%s,", pipe, box);
  }
  g_sprintf (pipe, "%s%s>\" ! vpidownload ! fakesink", pipe, box);

  g_value_init (&get_gst_array, GST_TYPE_ARRAY);

  pipeline = test_create_pipeline (pipe);

  tracker = gst_bin_get_by_name (GST_BIN (pipeline), "tracker");

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  g_object_get_property (G_OBJECT (tracker), "boxes", &get_gst_array);

  /* Test if other boxes were discarded */
  fail_unless_equals_int (gst_value_array_get_size (&get_gst_array), MAX_BOXES);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
c_array_to_gst_array (GValue * gst_array, gint c_array[][NUMBER_PARAMS],
    guint rows, guint cols)
{
  GValue row = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (gst_array);
  g_return_if_fail (c_array);

  for (i = 0; i < rows; i++) {
    g_value_init (&row, GST_TYPE_ARRAY);

    for (j = 0; j < cols; j++) {
      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, c_array[i][j]);
      gst_value_array_append_value (&row, &value);
      g_value_unset (&value);
    }

    gst_value_array_append_value (gst_array, &row);
    g_value_unset (&row);
  }
}

static void
compare_c_array_with_gst_array (GValue * gst_array,
    gint c_array[][NUMBER_PARAMS], gint rows)
{
  const GValue *row = NULL;
  gint value = 0;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (gst_array);
  g_return_if_fail (c_array);

  /* Verify size of arrays is the same */
  fail_unless_equals_int (gst_value_array_get_size (gst_array), rows);
  fail_unless_equals_int (gst_value_array_get_size (gst_value_array_get_value
          (gst_array, 0)), NUMBER_PARAMS);

  for (i = 0; i < rows; i++) {
    row = gst_value_array_get_value (gst_array, i);
    for (j = 0; j < NUMBER_PARAMS; j++) {
      value = g_value_get_int (gst_value_array_get_value (row, j));
      fail_unless_equals_int (value, c_array[i][j]);
    }
  }
}

static void
redefine_boxes_on_the_fly (gint set_array[][NUMBER_PARAMS],
    gint get_array[][NUMBER_PARAMS], gint set_boxes, gint get_boxes)
{
  GstElement *pipeline = NULL;

  GstElement *tracker = NULL;
  GValue set_gst_array = G_VALUE_INIT;
  GValue get_gst_array = G_VALUE_INIT;

  g_value_init (&set_gst_array, GST_TYPE_ARRAY);
  g_value_init (&get_gst_array, GST_TYPE_ARRAY);

  c_array_to_gst_array (&set_gst_array, set_array, set_boxes, NUMBER_PARAMS);
  c_array_to_gst_array (&get_gst_array, get_array, get_boxes, NUMBER_PARAMS);

  pipeline =
      test_create_pipeline (test_pipes[TEST_REDEFINING_BOXES_ON_THE_FLY]);

  tracker = gst_bin_get_by_name (GST_BIN (pipeline), "tracker");

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  /* Test to process the old boxes and the new boxes for a while */
  g_usleep (SLEEP_TIME);

  g_object_set_property (G_OBJECT (tracker), "boxes", &set_gst_array);
  /* Test if what gst gets is what was expected */
  g_object_get_property (G_OBJECT (tracker), "boxes", &get_gst_array);
  compare_c_array_with_gst_array (&get_gst_array, get_array, get_boxes);

  g_usleep (SLEEP_TIME);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
}

static void
append_boxes_on_the_fly (gint set_array[][NUMBER_PARAMS],
    gint get_array[][NUMBER_PARAMS], gint set_boxes, gint get_boxes)
{
  GstElement *pipeline = NULL;
  GstElement *tracker = NULL;
  GValue get_gst_array = G_VALUE_INIT;
  gint i = 0;

  g_value_init (&get_gst_array, GST_TYPE_ARRAY);

  pipeline =
      test_create_pipeline (test_pipes[TEST_REDEFINING_BOXES_ON_THE_FLY]);

  tracker = gst_bin_get_by_name (GST_BIN (pipeline), "tracker");

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_PLAYING),
      GST_STATE_CHANGE_ASYNC);
  fail_unless_equals_int (gst_element_get_state (pipeline, NULL, NULL, -1),
      GST_STATE_CHANGE_SUCCESS);

  for (i = 0; i < set_boxes; i++) {
    g_signal_emit_by_name (tracker, "new-box", set_array[i][0], set_array[i][1],
        set_array[i][2], set_array[i][3]);
  }

  g_object_get_property (G_OBJECT (tracker), "boxes", &get_gst_array);
  /* Test if what gst gets is what was expected */
  compare_c_array_with_gst_array (&get_gst_array, get_array, get_boxes);

  fail_unless_equals_int (gst_element_set_state (pipeline, GST_STATE_NULL),
      GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (pipeline);
}

GST_START_TEST (test_append_boxes_on_the_fly)
{
  gint set_array[3][NUMBER_PARAMS] = { {613, 332, 34, 23}
  , {669, 329, 50, 80}          /* This one should be discarded */
  , {790, 376, 41, 22}
  };
  gint get_array[4][NUMBER_PARAMS] = { {613, 332, 23, 23}
  , {669, 329, 30, 29}
  , {613, 332, 34, 23}
  , {790, 376, 41, 22}
  };

  append_boxes_on_the_fly (set_array, get_array, 3, 4);
}

GST_END_TEST;

GST_START_TEST (test_redefine_to_less_boxes_on_the_fly)
{
  gint set_array[1][NUMBER_PARAMS] = { {613, 372, 53, 23}
  };
  redefine_boxes_on_the_fly (set_array, set_array, 1, 1);
}

GST_END_TEST;

GST_START_TEST (test_redefine_to_more_boxes_on_the_fly)
{
  gint set_array[5][NUMBER_PARAMS] = { {613, 332, 23, 23}
  , {669, 329, 50, 30}
  , {790, 376, 41, 22}
  , {669, 329, 30, 30}
  , {669, 329, 50, 30}
  };
  redefine_boxes_on_the_fly (set_array, set_array, 5, 5);
}

GST_END_TEST;

GST_START_TEST (test_redefine_and_discard_boxes_on_the_fly)
{
  gint set_array[5][NUMBER_PARAMS] = { {613, 332, 80, 23}
  , {669, 329, 50, 30}
  , {790, 376, 41, 90}
  , {669, 329, 60, 30}
  , {669, 329, 50, 1000}
  };
  gint get_array[2][NUMBER_PARAMS] = { {669, 329, 50, 30}
  , {669, 329, 60, 30}
  };
  /* Here some boxes have invalid dimensions, therefore set and get arrays
     are different */
  redefine_boxes_on_the_fly (set_array, get_array, 5, 2);
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
  tcase_add_test (tc, test_discard_when_more_than_64_box_provided);
  tcase_add_test (tc, test_redefine_to_more_boxes_on_the_fly);
  tcase_add_test (tc, test_redefine_to_less_boxes_on_the_fly);
  tcase_add_test (tc, test_redefine_and_discard_boxes_on_the_fly);
  tcase_add_test (tc, test_append_boxes_on_the_fly);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_klt_tracker);
