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

G_BEGIN_DECLS

#define GST_TYPE_VPI_FILTER   (gst_vpi_filter_get_type())
#define GST_VPI_FILTER(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VPI_FILTER,GstVpiFilter))
#define GST_VPI_FILTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VPI_FILTER,GstVpiFilterClass))
#define GST_IS_VPI_FILTER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VPI_FILTER))
#define GST_IS_VPI_FILTER_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VPI_FILTER))

typedef struct _GstVpiFilter GstVpiFilter;
typedef struct _GstVpiFilterClass GstVpiFilterClass;

struct _GstVpiFilter
{
  GstVideoFilter base_vpi_filter;

};

struct _GstVpiFilterClass
{
  GstVideoFilterClass base_vpi_filter_class;
};

GType gst_vpi_filter_get_type (void);

G_END_DECLS

#endif
