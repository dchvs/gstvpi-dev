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

#ifndef __GST_VPI_BUFFER_POOL_H__
#define __GST_VPI_BUFFER_POOL_H__

#include <gst/gst.h>

#include "gstcudabufferpool.h"

G_BEGIN_DECLS

#define GST_VPI_TYPE_BUFFER_POOL gst_vpi_buffer_pool_get_type()

/**
 * GstVpiBufferPool:
 *
 * The opaque #GstVpiBufferPool data structure.
 */
G_DECLARE_FINAL_TYPE (GstVpiBufferPool, gst_vpi_buffer_pool, GST_VPI, BUFFER_POOL, GstCudaBufferPool);

G_END_DECLS

#endif // __GST_VPI_BUFFER_POOL_H__
