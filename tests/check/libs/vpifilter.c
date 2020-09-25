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

GST_START_TEST (test_dummy)
{
}

GST_END_TEST;

static Suite *
gst_vpi_filter_suite (void)
{
  Suite *suite = suite_create ("vpifilter");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (suite, tc);
  tcase_add_test (tc, test_dummy);

  return suite;
}

GST_CHECK_MAIN (gst_vpi_filter);
