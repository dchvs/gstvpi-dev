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
static gboolean gst_vpi_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_vpi_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);

static void
gst_vpi_buffer_pool_class_init (GstVpiBufferPoolClass * klass)
{
  GstBufferPoolClass *buffer_pool_class = GST_BUFFER_POOL_CLASS (klass);

  buffer_pool_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_vpi_buffer_pool_alloc_buffer);
  buffer_pool_class->set_config =
      GST_DEBUG_FUNCPTR (gst_vpi_buffer_pool_set_config);
}

static void
gst_vpi_buffer_pool_init (GstVpiBufferPool * self)
{
  GST_INFO_OBJECT (self, "New CUDA buffer pool");
}

static gboolean
gst_vpi_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstVpiBufferPool *self = GST_VPI_BUFFER_POOL (pool);
  GstCaps *caps = NULL;
  guint size = 0;
  guint min_buffers = 0;
  guint max_buffers = 0;

  if (!GST_BUFFER_POOL_CLASS (gst_vpi_buffer_pool_parent_class)->set_config
      (pool, config)) {
    return FALSE;
  }

  gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
      &max_buffers);

  return gst_video_info_from_caps (&self->video_info, caps);
}

static GstFlowReturn
gst_vpi_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstVpiBufferPool *self = GST_VPI_BUFFER_POOL (pool);
  GstFlowReturn ret = GST_FLOW_ERROR;

  GST_DEBUG_OBJECT (self, "Creating VPI Buffer Pool");

  ret =
      GST_BUFFER_POOL_CLASS (gst_vpi_buffer_pool_parent_class)->alloc_buffer
      (pool, buffer, params);
  if (ret == GST_FLOW_ERROR)
    goto out;

  GST_DEBUG_OBJECT (self, "Adding VpiMeta to buffer");

  if (gst_buffer_add_vpi_meta (*buffer, &(self->video_info)) == NULL) {
    GST_ERROR_OBJECT (self, "Could not add VpiMeta to buffer.");
    ret = GST_FLOW_ERROR;
    goto out;
  };

out:
  return ret;
}
