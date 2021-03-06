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

#ifndef __GST_CUDA_BUFFER_POOL_H__
#define __GST_CUDA_BUFFER_POOL_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_CUDA_TYPE_BUFFER_POOL gst_cuda_buffer_pool_get_type ()

/**
 * GstCudaBufferPool:
 *
 * The opaque #GstCudaBufferPool data structure.
 */
G_DECLARE_DERIVABLE_TYPE(GstCudaBufferPool, gst_cuda_buffer_pool, GST_CUDA, BUFFER_POOL, GstBufferPool);

struct _GstCudaBufferPoolClass
{
  GstBufferPoolClass parent_class;

  gboolean (*add_meta) (GstCudaBufferPool * self, GstBuffer * buffer);
};

G_END_DECLS

#endif // __GST_CUDA_BUFFER_POOL_H__
