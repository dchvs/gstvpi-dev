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

#ifndef _GST_VPI_FILTER_H_
#define _GST_VPI_FILTER_H_

#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>
#include <vpi/Image.h>
#include <vpi/Stream.h>

#include "gstvpimeta.h"

G_BEGIN_DECLS

#define GST_TYPE_VPI_FILTER   (gst_vpi_filter_get_type())
G_DECLARE_DERIVABLE_TYPE (GstVpiFilter, gst_vpi_filter, GST,
    VPI_FILTER, GstVideoFilter)

struct _GstVpiFilterClass
{
  GstVideoFilterClass parent_class;

  gboolean (*start) (GstVpiFilter *self, GstVideoInfo *in_info, GstVideoInfo
                     *out_info);
  GstFlowReturn (*transform_image) (GstVpiFilter *self, VPIStream stream,
                                    VpiFrame *in_frame, VpiFrame *out_frame);
  GstFlowReturn (*transform_image_ip) (GstVpiFilter *self, VPIStream stream,
                                       VpiFrame *frame);
};

G_END_DECLS

#endif
