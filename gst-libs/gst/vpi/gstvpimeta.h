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

#ifndef __GST_VPI_META_H__
#define __GST_VPI_META_H__

#include <gst/gst.h>
#include <vpi/Image.h>
#include <gst/video/video.h>

G_BEGIN_DECLS 

#define GST_VPI_META_API_TYPE (gst_vpi_meta_api_get_type())
#define GST_VPI_META_INFO  (gst_vpi_meta_get_info())

typedef struct _GstVpiMeta GstVpiMeta;

/**
 * gst_buffer_add_vpi_meta
 * @buffer: (in) (transfer none) a #GstBuffer
 * @video_info: (in) (transfer none) a #GstVideoInfo
 *
 * Attaches GstVpiMeta metadata to @buffer with
 * the given parameters.
 *
 * Returns: (transfer none): the #GstVpiMeta on @buffer.
 */
GstVpiMeta * gst_buffer_add_vpi_meta (GstBuffer * buffer, GstVideoInfo * video_info);

GType gst_vpi_meta_api_get_type (void);
const GstMetaInfo *gst_vpi_meta_get_info (void);

G_END_DECLS

#endif // __GST_VPI_META_H__
