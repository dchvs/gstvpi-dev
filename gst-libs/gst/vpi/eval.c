/*
 * Copyright (C) 2021 RidgeRun, LLC (http://www.ridgerun.com)
 * All Rights Reserved.
 *
 * The contents of this software are proprietary and confidential to RidgeRun,
 * LLC.  No part of this program may be photocopied, reproduced or translated
 * into another programming language without prior written consent of
 * RidgeRun, LLC.  The user is free to modify the source code after obtaining
 * a software license from RidgeRun.  All source code changes must be provided
 * back to RidgeRun without any encumbrance.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eval.h"

gboolean
eval_end (void)
{
  static gint num_buffers = 0;
  gboolean ret = FALSE;

  if (0 == num_buffers || MAX_BUFFERS < num_buffers) {
    g_print ("                                       \n"
        "                                       \n"
        "*************************************  \n"
        "*** THIS IS AN EVALUATION VERSION ***  \n"
        "*************************************  \n"
        "                                       \n"
        "        Thanks for evaluating          \n"
        "          RidgeRun GstVPI              \n"
        "                                       \n"
        "  This version allows you to use       \n"
        "  %d buffers.                          \n"
        "  Please contact                       \n"
        "  <support@ridgerun.com> to purchase   \n"
        "  the professional version of this     \n"
        "  framework.                           \n"
        "                                       \n"
        "*************************************  \n"
        "*** THIS IS AN EVALUATION VERSION ***  \n"
        "*************************************  \n"
        "                                       \n", MAX_BUFFERS);
  }

  if (MAX_BUFFERS < num_buffers) {
    ret = TRUE;
  } else {
    num_buffers += 1;
  }
  return ret;
}
