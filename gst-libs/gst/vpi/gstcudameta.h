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

#ifndef __GST_CUDA_META_H__
#define __GST_CUDA_META_H__

#include <gst/gst.h>

G_BEGIN_DECLS 

#define GST_CUDA_META_API_TYPE (gst_cuda_meta_api_get_type())
#define GST_CUDA_META_INFO  (gst_cuda_meta_get_info())

typedef struct _GstCudaMeta GstCudaMeta;

/**
 * GstCudaMeta:
 * @meta: parent #GstMeta
 *
 * Extra buffer metadata describing CUDA properties
 */
struct _GstCudaMeta {
  GstMeta meta;
};


/**
 * gst_buffer_add_cuda_meta
 * @buffer: (in) (transfer none) a #GstBuffer
 *
 * Attaches GstCudaMeta metadata to @buffer with
 * the given parameters.
 *
 * Returns: (transfer none): the #GstCudaMeta on @buffer.
 */
GstCudaMeta * gst_buffer_add_cuda_meta (GstBuffer * buffer);

GType gst_cuda_meta_api_get_type (void);
const GstMetaInfo *gst_cuda_meta_get_info (void);

G_END_DECLS

#endif // __GST_CUDA_META_H__
