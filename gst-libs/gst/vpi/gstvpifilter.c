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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvpifilter.h"

#include <cuda_runtime.h>

#include "eval.h"
#include "gstcudameta.h"
#include "gstvpibufferpool.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_filter_debug_category

#define VPI_BACKEND_ENUM (vpi_backend_enum_get_type ())
GType vpi_backend_enum_get_type (void);

GType
vpi_backend_enum_get_type (void)
{
  static GType vpi_backend_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_BACKEND_CPU, "CPU Backend", "cpu"},
    {VPI_BACKEND_CUDA, "CUDA Backend", "cuda"},
    {VPI_BACKEND_PVA, "PVA Backend (Xavier only)", "pva"},
    {VPI_BACKEND_VIC, "VIC Backend", "vic"},
    {0, NULL, NULL}
  };

  if (!vpi_backend_enum_type) {
    vpi_backend_enum_type = g_enum_register_static ("VpiBackend", values);
  }

  return vpi_backend_enum_type;
}

typedef struct _GstVpiFilterPrivate GstVpiFilterPrivate;

struct _GstVpiFilterPrivate
{
  GstVpiBufferPool *downstream_buffer_pool;
  VPIStream vpi_stream;
  cudaStream_t cuda_stream;
  gint backend;
};

static GstFlowReturn gst_vpi_filter_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstFlowReturn gst_vpi_filter_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);
static gboolean gst_vpi_filter_set_info (GstVideoFilter * filter, GstCaps *
    incaps, GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static gboolean gst_vpi_filter_start (GstBaseTransform * trans);
static gboolean gst_vpi_filter_stop (GstBaseTransform * trans);
static gboolean gst_vpi_filter_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static GstFlowReturn gst_vpi_filter_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static GstFlowReturn gst_vpi_filter_prepare_output_buffer_ip (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static void gst_vpi_filter_finalize (GObject * object);
static void gst_vpi_filter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_filter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_BACKEND,
};

#define PROP_BACKEND_DEFAULT VPI_BACKEND_CUDA

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstVpiFilter, gst_vpi_filter, GST_TYPE_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_filter_debug_category, "vpifilter", 0,
        "debug category for vpifilter base class"));

static void
gst_vpi_filter_class_init (GstVpiFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (GstVpiFilterPrivate));

  video_filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_vpi_filter_transform_frame);
  video_filter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_filter_transform_frame_ip);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_vpi_filter_set_info);
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_vpi_filter_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_filter_stop);
  base_transform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_vpi_filter_decide_allocation);
  base_transform_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_vpi_filter_prepare_output_buffer);
  gobject_class->finalize = gst_vpi_filter_finalize;
  gobject_class->set_property = gst_vpi_filter_set_property;
  gobject_class->get_property = gst_vpi_filter_get_property;

  g_object_class_install_property (gobject_class, PROP_BACKEND,
      g_param_spec_enum ("backend", "VPI Backend",
          "Backend to use to execute VPI algorithms.",
          VPI_BACKEND_ENUM, PROP_BACKEND_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_filter_init (GstVpiFilter * self)
{
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  priv->downstream_buffer_pool = NULL;
  priv->vpi_stream = NULL;
  priv->cuda_stream = NULL;
  priv->backend = PROP_BACKEND_DEFAULT;
}

static gboolean
gst_vpi_filter_start (GstBaseTransform * trans)
{
  GstVpiFilter *self = GST_VPI_FILTER (trans);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_GET_CLASS (self);
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);
  gboolean ret = TRUE;
  VPIStatus vpi_status = VPI_SUCCESS;
  cudaError_t cuda_status = cudaSuccess;

  GST_DEBUG_OBJECT (self, "start");

  if (!vpi_filter_class->transform_image
      && !vpi_filter_class->transform_image_ip) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Subclass did not implement transform_image nor transform_image_ip."),
        (NULL));
    ret = FALSE;
    goto out;
  }

  if (vpi_filter_class->transform_image_ip) {
    gst_base_transform_set_in_place (trans, TRUE);
    gst_base_transform_set_passthrough (trans, TRUE);
  }

  cuda_status = cudaStreamCreate (&priv->cuda_stream);
  if (cudaSuccess != cuda_status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create CUDA stream."), (NULL));
    ret = FALSE;
    goto out;
  }

  vpi_status = vpiStreamCreate (VPI_BACKEND_ALL, &priv->vpi_stream);
  if (VPI_SUCCESS != vpi_status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create VPI stream."), (NULL));
    ret = FALSE;
    goto free_cuda_stream;
  }

  vpi_status = vpiStreamCreateCudaStreamWrapper (priv->cuda_stream,
      VPI_BACKEND_ALL, &priv->vpi_stream);
  if (VPI_SUCCESS != vpi_status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not wrap CUDA stream."), (NULL));
    ret = FALSE;
    goto free_vpi_stream;
  }

  goto out;

free_vpi_stream:
  vpiStreamDestroy (priv->vpi_stream);
  priv->vpi_stream = NULL;

free_cuda_stream:
  cudaStreamDestroy (priv->cuda_stream);
  priv->cuda_stream = NULL;

out:
  return ret;
}

static gboolean
gst_vpi_filter_set_info (GstVideoFilter * filter, GstCaps *
    incaps, GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstVpiFilter *self = GST_VPI_FILTER (filter);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_GET_CLASS (self);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (self, "set_info");

  if (vpi_filter_class->start) {
    /* Call child class start method when caps are already known */
    ret = vpi_filter_class->start (self, in_info, out_info);
  }

  return ret;
}

static void
gst_vpi_filter_attach_mem_to_stream (GstVpiFilter * self, cudaStream_t stream,
    void *mem, gint attach_flag)
{
  cudaError_t cuda_status = cudaSuccess;

  g_return_if_fail (self);
  g_return_if_fail (mem);

  cuda_status = cudaStreamAttachMemAsync (stream, mem, 0, attach_flag);

  cudaStreamSynchronize (stream);

  if (cudaSuccess != cuda_status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not attach buffer to CUDA %s stream. Error: %s",
            stream == NULL ? "global" : "custom",
            cudaGetErrorString (cuda_status)), (NULL));
  }
}

#ifdef EVAL
static GstFlowReturn
gst_vpi_filter_check_eval (GstVpiFilter * self)
{
  GstFlowReturn ret = GST_FLOW_OK;
  g_return_val_if_fail (self, GST_FLOW_ERROR);

  if (eval_end ()) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Evaluation version finished."), (NULL));
    ret = GST_FLOW_EOS;
  }
  return ret;
}
#endif

static GstFlowReturn
gst_vpi_filter_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe)
{
  GstVpiFilter *self = NULL;
  GstVpiFilterClass *vpi_filter_class = NULL;
  GstVpiFilterPrivate *priv = NULL;
  GstVpiMeta *in_vpi_meta = NULL;
  GstVpiMeta *out_vpi_meta = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (NULL != filter, GST_FLOW_ERROR);
  g_return_val_if_fail (NULL != inframe, GST_FLOW_ERROR);
  g_return_val_if_fail (NULL != outframe, GST_FLOW_ERROR);

  self = GST_VPI_FILTER (filter);

  GST_LOG_OBJECT (self, "Transform frame");

  vpi_filter_class = GST_VPI_FILTER_GET_CLASS (self);
  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  g_return_val_if_fail (vpi_filter_class->transform_image, GST_FLOW_ERROR);

  in_vpi_meta =
      ((GstVpiMeta *) gst_buffer_get_meta (inframe->buffer,
          GST_VPI_META_API_TYPE));
  out_vpi_meta =
      ((GstVpiMeta *) gst_buffer_get_meta (outframe->buffer,
          GST_VPI_META_API_TYPE));

  if (in_vpi_meta && out_vpi_meta) {

    gst_vpi_filter_attach_mem_to_stream (self, priv->cuda_stream,
        inframe->map->data, cudaMemAttachSingle);
    gst_vpi_filter_attach_mem_to_stream (self, priv->cuda_stream,
        outframe->map->data, cudaMemAttachSingle);

    ret = vpi_filter_class->transform_image (self, priv->vpi_stream,
        &in_vpi_meta->vpi_frame, &out_vpi_meta->vpi_frame);

    vpiStreamSync (priv->vpi_stream);

    /* Attach memory to global stream to detach it from CUDA stream */
    gst_vpi_filter_attach_mem_to_stream (self, NULL, inframe->map->data,
        cudaMemAttachHost);
    gst_vpi_filter_attach_mem_to_stream (self, NULL, outframe->map->data,
        cudaMemAttachHost);

    if (GST_FLOW_OK != ret) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Child VPI element processing failed."), (NULL));
    }

  } else {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Cannot process buffers that do not contain the VPI meta."), (NULL));
    ret = GST_FLOW_ERROR;
  }
#ifdef EVAL
  ret = gst_vpi_filter_check_eval (self);
#endif

  return ret;
}

static GstFlowReturn
gst_vpi_filter_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame)
{
  GstVpiFilter *self = NULL;
  GstVpiFilterClass *vpi_filter_class = NULL;
  GstVpiFilterPrivate *priv = NULL;
  GstVpiMeta *vpi_meta = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (NULL != filter, GST_FLOW_ERROR);
  g_return_val_if_fail (NULL != frame, GST_FLOW_ERROR);

  self = GST_VPI_FILTER (filter);

  GST_LOG_OBJECT (self, "Transform frame ip");

  vpi_filter_class = GST_VPI_FILTER_GET_CLASS (self);
  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  g_return_val_if_fail (vpi_filter_class->transform_image_ip, GST_FLOW_ERROR);

  vpi_meta =
      ((GstVpiMeta *) gst_buffer_get_meta (frame->buffer,
          GST_VPI_META_API_TYPE));

  if (vpi_meta) {
    gst_vpi_filter_attach_mem_to_stream (self, priv->cuda_stream,
        frame->map->data, cudaMemAttachSingle);

    ret = vpi_filter_class->transform_image_ip (self, priv->vpi_stream,
        &vpi_meta->vpi_frame);

    vpiStreamSync (priv->vpi_stream);

    /* Attach memory to global stream to detach it from CUDA stream */
    gst_vpi_filter_attach_mem_to_stream (self, NULL, frame->map->data,
        cudaMemAttachHost);

    if (GST_FLOW_OK != ret) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Child VPI element processing failed."), (NULL));
    }

  } else {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Cannot process buffers that do not contain the VPI meta."), (NULL));
    ret = GST_FLOW_ERROR;
  }
#ifdef EVAL
  ret = gst_vpi_filter_check_eval (self);
#endif

  return ret;
}

static gsize
gst_vpi_filter_compute_size (GstVpiFilter * self, GstVideoInfo * info)
{
  gsize size = 0;

  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (info, 0);

  if (info->size) {
    size = info->size;
  } else {
    size = (info->stride[0] * info->height) +
        (info->stride[1] * info->height / 2) +
        (info->stride[2] * info->height / 2);
  }

  GST_INFO_OBJECT (self, "Computed buffer size of %" G_GUINT64_FORMAT, size);

  return size;
}

static gboolean
gst_vpi_filter_create_buffer_pool (GstVpiFilter * self,
    GstVpiBufferPool * buffer_pool, GstQuery * query)
{
  GstVideoFilter *video_filter = GST_VIDEO_FILTER (self);
  gsize size = 0;
  GstStructure *config = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  gboolean need_pool = FALSE;
  gboolean ret = FALSE;

  GST_INFO_OBJECT (self, "Creating downstream buffer pool");

  gst_query_parse_allocation (query, &caps, &need_pool);

  pool = GST_BUFFER_POOL (buffer_pool);
  size = gst_vpi_filter_compute_size (self, &video_filter->out_info);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ELEMENT_ERROR (self, RESOURCE, FAILED,
        ("Unable to set pool configuration."), (NULL));
    goto out;
  }

  gst_query_add_allocation_pool (query,
      GST_BUFFER_POOL (buffer_pool), size, 2, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_CUDA_META_API_TYPE, NULL);

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_vpi_filter_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstVpiFilter *self = GST_VPI_FILTER (trans);
  GstVpiFilterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GST_TYPE_VPI_FILTER, GstVpiFilterPrivate);
  gint i = 0;

  GST_INFO_OBJECT (trans, "Deciding allocation");

  if (!priv->downstream_buffer_pool) {
    priv->downstream_buffer_pool =
        g_object_new (GST_VPI_TYPE_BUFFER_POOL, NULL);
  }

  /* We can't use dowstream provided pools, since we need unified memory */
  for (i = 0; i < gst_query_get_n_allocation_pools (query); i++) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, i, &pool, NULL, NULL, NULL);
    GST_INFO_OBJECT (self, "Discarding downstream pool \"%s\"",
        GST_OBJECT_NAME (pool));
    gst_object_unref (pool);

    gst_query_remove_nth_allocation_pool (query, i);
  }

  return gst_vpi_filter_create_buffer_pool (self,
      priv->downstream_buffer_pool, query);
}

static gboolean
gst_vpi_filter_stop (GstBaseTransform * trans)
{
  GstVpiFilter *self = GST_VPI_FILTER (trans);
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (self, "stop");

  vpiStreamDestroy (priv->vpi_stream);
  priv->vpi_stream = NULL;

  cudaStreamDestroy (priv->cuda_stream);
  priv->cuda_stream = NULL;

  return ret;
}

static GstFlowReturn
gst_vpi_filter_prepare_output_buffer_ip (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
  g_return_val_if_fail (trans, GST_FLOW_ERROR);
  g_return_val_if_fail (input, GST_FLOW_ERROR);
  g_return_val_if_fail (outbuf, GST_FLOW_ERROR);

  if (!gst_buffer_is_writable (input)) {
    /* Create a subbuffer to allow subclasses to add metas. This wont
       actually copy the data, just the GstBuffer skeleton */
    *outbuf = gst_buffer_copy_region (input, GST_BUFFER_COPY_ALL, 0, -1);
  } else {
    *outbuf = input;
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_vpi_filter_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
  GstVpiFilterClass *klass = GST_VPI_FILTER_GET_CLASS (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;

  if (klass->transform_image_ip && gst_base_transform_is_passthrough (trans)) {
    ret = gst_vpi_filter_prepare_output_buffer_ip (trans, input, outbuf);
  } else {
    ret =
        GST_BASE_TRANSFORM_CLASS
        (gst_vpi_filter_parent_class)->prepare_output_buffer (trans, input,
        outbuf);
  }

  return ret;
}

static void
gst_vpi_filter_finalize (GObject * object)
{
  GstVpiFilterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (object,
      GST_TYPE_VPI_FILTER, GstVpiFilterPrivate);

  GST_INFO_OBJECT (object, "Finalize VPI filter");

  g_clear_object (&priv->downstream_buffer_pool);

  G_OBJECT_CLASS (gst_vpi_filter_parent_class)->finalize (object);
}

static void
gst_vpi_filter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiFilter *self = GST_VPI_FILTER (object);
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_BACKEND:
      priv->backend = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_vpi_filter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiFilter *self = GST_VPI_FILTER (object);
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_BACKEND:
      g_value_set_enum (value, priv->backend);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

VPIBackend
gst_vpi_filter_get_backend (GstVpiFilter * self)
{
  GstVpiFilterPrivate *priv = NULL;
  VPIBackend backend = VPI_BACKEND_INVALID;

  g_return_val_if_fail (self, backend);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  GST_OBJECT_LOCK (self);
  backend = priv->backend;
  GST_OBJECT_UNLOCK (self);

  return backend;
}
