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

#include "gstvpibufferpool.h"

#include <gst/video/video.h>

#include "gstcudabufferpool.h"
#include "gstvpimeta.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_vpi_buffer_pool_debug_category

struct _GstVpiBufferPool
{
  GstCudaBufferPool base;

  GstVideoInfo video_info;
};

G_DEFINE_TYPE_WITH_CODE (GstVpiBufferPool, gst_vpi_buffer_pool,
    GST_CUDA_TYPE_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_buffer_pool_debug_category,
        "vpibufferpool", 0, "debug category for vpi buffer pool class"));

/* prototypes */
static gboolean gst_vpi_buffer_pool_set_config (GstCudaBufferPool * pool,
    GstStructure * config);
static gboolean gst_vpi_buffer_pool_add_meta (GstCudaBufferPool * cuda_pool,
    GstBuffer * buffer);

static void
gst_vpi_buffer_pool_class_init (GstVpiBufferPoolClass * klass)
{
  GstCudaBufferPoolClass *cuda_pool_class = GST_CUDA_BUFFER_POOL_CLASS (klass);

  cuda_pool_class->set_config =
      GST_DEBUG_FUNCPTR (gst_vpi_buffer_pool_set_config);
  cuda_pool_class->add_meta = GST_DEBUG_FUNCPTR (gst_vpi_buffer_pool_add_meta);
}

static void
gst_vpi_buffer_pool_init (GstVpiBufferPool * self)
{
  GST_INFO_OBJECT (self, "New VPI buffer pool");
}

static gboolean
gst_vpi_buffer_pool_set_config (GstCudaBufferPool * pool, GstStructure * config)
{
  GstVpiBufferPool *self = GST_VPI_BUFFER_POOL (pool);
  GstCaps *caps = NULL;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL)) {
    GST_ERROR_OBJECT (self, "Error getting parameters from buffer pool config");
    return FALSE;
  }

  return gst_video_info_from_caps (&self->video_info, caps);
}

static gboolean
gst_vpi_buffer_pool_add_meta (GstCudaBufferPool * cuda_pool, GstBuffer * buffer)
{
  GstVpiBufferPool *self = GST_VPI_BUFFER_POOL (cuda_pool);

  GST_INFO_OBJECT (self, "Adding VPI meta to the buffer");

  if (gst_buffer_add_vpi_meta (buffer, &(self->video_info)) == NULL) {
    GST_ERROR_OBJECT (self, "Failed to add VpiMeta to buffer.");
    return FALSE;
  };

  return TRUE;
}
