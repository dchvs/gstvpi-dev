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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstvpidownload.h"
#include "gstvpigaussianfilter.h"
#include "gstvpiharrisdetector.h"
#include "gstvpiklttracker.h"
#include "gstvpiundistort.h"
#include "gstvpiupload.h"

static gboolean
vpi_init (GstPlugin * vpi)
{
  gboolean ret = FALSE;

  if (!gst_element_register (vpi, "vpidownload", GST_RANK_NONE,
          GST_TYPE_VPI_DOWNLOAD)) {
    GST_ERROR ("Failed to register vpidownload");
    goto out;
  }

  if (!gst_element_register (vpi, "vpigaussianfilter", GST_RANK_NONE,
          GST_TYPE_VPI_GAUSSIAN_FILTER)) {
    GST_ERROR ("Failed to register vpigaussianfilter");
    goto out;
  }

  if (!gst_element_register (vpi, "vpiharrisdetector", GST_RANK_NONE,
          GST_TYPE_VPI_HARRIS_DETECTOR)) {
    GST_ERROR ("Failed to register vpiharrisdetector");
    goto out;
  }

  if (!gst_element_register (vpi, "vpiklttracker", GST_RANK_NONE,
          GST_TYPE_VPI_KLT_TRACKER)) {
    GST_ERROR ("Failed to register vpiklttracker");
    goto out;
  }

  if (!gst_element_register (vpi, "vpiundistort", GST_RANK_NONE,
          GST_TYPE_VPI_UNDISTORT)) {
    GST_ERROR ("Failed to register vpiundistort");
    goto out;
  }

  if (!gst_element_register (vpi, "vpiupload", GST_RANK_NONE,
          GST_TYPE_VPI_UPLOAD)) {
    GST_ERROR ("Failed to register vpiupload");
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

#ifndef VERSION
#define VERSION "0.1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "gst-vpi"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst-vpi"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://www.ridgerun.com"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vpi,
    "GStreamer plugin for the NVIDIA VPI framework",
    vpi_init, VERSION, "Proprietary", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
