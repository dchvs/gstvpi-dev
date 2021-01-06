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

#ifndef _GST_VPI_HARRIS_DETECTOR_H_
#define _GST_VPI_HARRIS_DETECTOR_H_

#include <gst-libs/gst/vpi/gstvpifilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VPI_HARRIS_DETECTOR (gst_vpi_harris_detector_get_type())
G_DECLARE_FINAL_TYPE(GstVpiHarrisDetector, gst_vpi_harris_detector, GST, VPI_HARRIS_DETECTOR, GstVpiFilter)

G_END_DECLS

#endif
