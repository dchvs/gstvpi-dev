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

#ifndef __GST_VPI_H__
#define __GST_VPI_H__

#include <gst/video/video.h>
#include <vpi/Image.h>

G_BEGIN_DECLS

VPIImageFormat gst_vpi_video_to_image_format (GstVideoFormat video_format);

GstVideoFormat gst_vpi_image_to_video_format (VPIImageFormat image_format);

#define VPI_BOUNDARY_CONDS_ENUM (vpi_boundary_cond_enum_get_type ())
    GType vpi_boundary_cond_enum_get_type (void);

#define VPI_INTERPOLATORS_ENUM (vpi_interpolator_enum_get_type ())
    GType vpi_interpolator_enum_get_type (void);

G_END_DECLS

#endif // __GST_VPI_H__
