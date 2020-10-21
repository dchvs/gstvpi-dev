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

#include "gstcudabufferpool.h"

#include "gstcudaallocator.h"
#include "gstcudameta.h"

/**
 * SECTION:gstcudabufferpool
 * @short_description: GStreamer buffer pool for GstCuda based elements
 *
 * This class implements a GStreamer standard buffer pool for GstCuda
 * based elements. By using HW accelerated buffers in unified memory
 * space data is accessible in both the GPU and CPU. Furthermore, if
 * scheduled correctly, this allows for high performance zero memory
 * copy pipelines. 
 */

GST_DEBUG_CATEGORY_STATIC (gst_cuda_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_cuda_buffer_pool_debug_category

typedef struct _GstCudaBufferPoolPrivate GstCudaBufferPoolPrivate;

struct _GstCudaBufferPoolPrivate
{
  GstCudaAllocator *allocator;
  GstVideoInfo caps_info;
  gboolean needs_video_meta;
};

G_DEFINE_TYPE_WITH_CODE (GstCudaBufferPool, gst_cuda_buffer_pool,
    GST_TYPE_BUFFER_POOL,
    GST_DEBUG_CATEGORY_INIT (gst_cuda_buffer_pool_debug_category,
        "cudabufferpool", 0, "debug category for cuda buffer pool class"));

/* prototypes */
static gboolean gst_cuda_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_cuda_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static void gst_cuda_buffer_pool_finalize (GObject * object);

static void
gst_cuda_buffer_pool_class_init (GstCudaBufferPoolClass * klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bp_class = GST_BUFFER_POOL_CLASS (klass);

  g_type_class_add_private (o_class, sizeof (GstCudaBufferPoolPrivate));

  o_class->finalize = gst_cuda_buffer_pool_finalize;

  bp_class->set_config = GST_DEBUG_FUNCPTR (gst_cuda_buffer_pool_set_config);
  bp_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_cuda_buffer_pool_alloc_buffer);
}

static void
gst_cuda_buffer_pool_init (GstCudaBufferPool * self)
{
  GstCudaBufferPoolPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_CUDA_TYPE_BUFFER_POOL,
      GstCudaBufferPoolPrivate);

  GST_INFO_OBJECT (self, "New CUDA buffer pool");

  priv->allocator = g_object_new (GST_CUDA_TYPE_ALLOCATOR, NULL);
  priv->needs_video_meta = FALSE;
}

static gboolean
gst_cuda_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL (pool);
  GstCudaBufferPoolPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_CUDA_TYPE_BUFFER_POOL,
      GstCudaBufferPoolPrivate);
  guint size = 0;
  GstCaps *caps = NULL;
  guint min_buffers = 0;
  guint max_buffers = 0;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers)) {
    GST_ERROR_OBJECT (self, "Error getting parameters from buffer pool config");
    goto error;
  }

  if (NULL == caps) {
    GST_ERROR_OBJECT (self, "No caps on the buffer pool configuration");
    goto error;
  }

  if (!gst_video_info_from_caps (&priv->caps_info, caps)) {
    GST_ERROR_OBJECT (self, "Unable to parse caps info");
    goto error;
  }

  GST_DEBUG_OBJECT (self,
      "Setting CUDA pool configuration with caps %" GST_PTR_FORMAT
      " and size %lu", caps, priv->caps_info.size);

  priv->needs_video_meta =
      gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);
  GST_DEBUG_OBJECT (self, "The client needs video meta: %s",
      priv->needs_video_meta ? "TRUE" : "FALSE");

  return
      GST_BUFFER_POOL_CLASS (gst_cuda_buffer_pool_parent_class)->set_config
      (pool, config);

error:
  return FALSE;
}

static GstFlowReturn
gst_cuda_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL (pool);
  GstCudaBufferPoolPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_CUDA_TYPE_BUFFER_POOL,
      GstCudaBufferPoolPrivate);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstBuffer *outbuf = NULL;
  GstMemory *outmem = NULL;

  GST_DEBUG_OBJECT (self, "Allocating cuda buffer");

  outmem =
      gst_allocator_alloc (GST_ALLOCATOR (priv->allocator),
      priv->caps_info.size, NULL);
  if (!outmem) {
    GST_ERROR_OBJECT (self, "Unable to allocate CUDA buffer");
    goto out;
  }

  outbuf = gst_buffer_new ();
  gst_buffer_append_memory (outbuf, outmem);

  if (priv->needs_video_meta) {
    gst_buffer_add_video_meta_full (outbuf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (&priv->caps_info),
        GST_VIDEO_INFO_WIDTH (&priv->caps_info),
        GST_VIDEO_INFO_HEIGHT (&priv->caps_info),
        GST_VIDEO_INFO_N_PLANES (&priv->caps_info), priv->caps_info.offset,
        priv->caps_info.stride);
  }

  gst_buffer_add_cuda_meta (outbuf);

  *buffer = outbuf;
  ret = GST_FLOW_OK;

out:
  return ret;
}

static void
gst_cuda_buffer_pool_finalize (GObject * object)
{
  GstCudaBufferPool *self = GST_CUDA_BUFFER_POOL (object);
  GstCudaBufferPoolPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_CUDA_TYPE_BUFFER_POOL,
      GstCudaBufferPoolPrivate);

  GST_DEBUG_OBJECT (self, "Finalizing CUDA buffer pool");

  g_clear_object (&priv->allocator);

  G_OBJECT_CLASS (gst_cuda_buffer_pool_parent_class)->finalize (object);
}
