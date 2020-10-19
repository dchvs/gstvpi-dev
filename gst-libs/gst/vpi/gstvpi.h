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

#ifndef __GST_VPI_H__
#define __GST_VPI_H__

#include <gst/video/video.h>
#include <vpi/Image.h>

G_BEGIN_DECLS

VPIImageType gst_vpi_video_format_to_image_type (GstVideoFormat video_format);

GstVideoFormat gst_vpi_image_type_to_video_format (VPIImageType image_type);

G_END_DECLS

#endif // __GST_VPI_H__