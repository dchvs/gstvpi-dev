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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvpiupload.h"

#include <gst/video/video.h>

#include "gst-libs/gst/vpi/gstcudabufferpool.h"
#include "gst-libs/gst/vpi/gstcudameta.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_upload_debug_category);
#define GST_CAT_DEFAULT gst_vpi_upload_debug_category

#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)
#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VPIImage", GST_VIDEO_FORMATS_ALL)

struct _GstVpiUpload
{
  GstBaseTransform parent;
  GstVideoInfo out_caps_info;
  GstVideoInfo in_caps_info;
  GstCudaBufferPool *upstream_buffer_pool;
};

/* prototypes */
static GstCaps *gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_vpi_upload_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_vpi_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static GstFlowReturn gst_vpi_upload_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static void gst_vpi_upload_finalize (GObject * object);

enum
{
  PROP_0
};

/* pad templates */
static GstStaticPadTemplate
    gst_vpi_upload_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_AND_VPIIMAGE_CAPS)
    );

static GstStaticPadTemplate
    gst_vpi_upload_sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS ";" VIDEO_AND_VPIIMAGE_CAPS)
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstVpiUpload, gst_vpi_upload, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_upload_debug_category, "vpiupload", 0,
        "debug category for vpiupload element"));

static void
gst_vpi_upload_class_init (GstVpiUploadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Upload", "Filter/Video", "Uploads data into NVIDIA VPI",
      "Andres Campos <andres.campos@ridgerun.com>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_transform_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_vpi_upload_set_caps);
  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_propose_allocation);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_transform_ip);
  gobject_class->finalize = gst_vpi_upload_finalize;
}

static void
gst_vpi_upload_init (GstVpiUpload * self)
{
  self->upstream_buffer_pool = NULL;
}

static GstCaps *
gst_vpi_upload_transform_downstream_caps (GstVpiUpload * self,
    GstCaps * caps_src)
{
  GstCaps *vpiimage = NULL;
  GstCapsFeatures *vpiimage_feature = NULL;
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  vpiimage = gst_caps_copy (caps_src);
  vpiimage_feature = gst_caps_features_from_string ("memory:VPIImage");

  for (i = 0; i < gst_caps_get_size (vpiimage); i++) {

    /* Add VPIImage to all structures */
    gst_caps_set_features (vpiimage, i,
        gst_caps_features_copy (vpiimage_feature));
  }

  gst_caps_features_free (vpiimage_feature);

  return vpiimage;
}

static GstCaps *
gst_vpi_upload_transform_upstream_caps (GstVpiUpload * self, GstCaps * caps_src)
{
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  /* All the result caps are Linux */
  for (i = 0; i < gst_caps_get_size (caps_src); i++) {
    gst_caps_set_features (caps_src, i, NULL);
  }

  return caps_src;
}

static GstCaps *
gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  GstCaps *given_caps = NULL;
  GstCaps *result = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:caps: %"
      GST_PTR_FORMAT "filter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  given_caps = gst_caps_copy (caps);

  if (direction == GST_PAD_SRC) {
    /* transform caps going upstream */
    result = gst_vpi_upload_transform_upstream_caps (self, given_caps);
  } else if (direction == GST_PAD_SINK) {
    /* transform caps going downstream */
    result = gst_vpi_upload_transform_downstream_caps (self, given_caps);
  } else {
    /* unknown direction */
    GST_ERROR_OBJECT (trans,
        "Cannot transform caps of unknown GstPadDirection");
    goto out;
  }

  if (filter) {
    GstCaps *tmp = result;
    result = gst_caps_intersect (filter, result);
    gst_caps_unref (tmp);
  }

out:
  GST_DEBUG_OBJECT (self, "Transformed caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_vpi_upload_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  gboolean status = FALSE;

  GST_INFO_OBJECT (self, "set_caps");

  status = gst_video_info_from_caps (&self->in_caps_info, incaps);
  if (!status) {
    GST_ERROR ("Unable to get the input caps");
    goto out;
  }

  status = gst_video_info_from_caps (&self->out_caps_info, outcaps);
  if (!status) {
    GST_ERROR ("Unable to get the output caps");
    goto out;
  }

  status = TRUE;

out:
  return status;
}

static gsize
gst_cuda_base_filter_compute_size (GstVpiUpload * self, GstVideoInfo * info)
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

  GST_LOG_OBJECT (self, "Computed buffer size of %" G_GUINT64_FORMAT, size);

  return size;
}

static gboolean
gst_vpi_upload_create_buffer_pool (GstVpiUpload * self,
    GstCudaBufferPool * buffer_pool, GstQuery * query)
{
  gsize size = 0;
  GstStructure *config = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  gboolean need_pool = FALSE;
  gboolean ret = FALSE;

  GST_INFO_OBJECT (self, "Proposing VPI upstream_buffer_pool");

  gst_query_parse_allocation (query, &caps, &need_pool);

  pool = GST_BUFFER_POOL (buffer_pool);
  size = gst_cuda_base_filter_compute_size (self, &self->in_caps_info);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Unable to set pool configuration");
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

/* propose allocation query parameters for input buffers */
static gboolean
gst_vpi_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);

  GST_DEBUG_OBJECT (self, "Proposing upstream allocation");
  if (!self->upstream_buffer_pool) {
    self->upstream_buffer_pool = g_object_new (GST_CUDA_TYPE_BUFFER_POOL, NULL);
  }

  return gst_vpi_upload_create_buffer_pool (self,
      self->upstream_buffer_pool, query);

  return TRUE;
}

static GstFlowReturn
gst_vpi_upload_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMeta *meta = NULL;

  GST_DEBUG_OBJECT (self, "transform_ip");

  meta = gst_buffer_get_meta (buf, GST_CUDA_META_API_TYPE);
  if (meta) {
    GST_DEBUG_OBJECT (self, "Received buffer through proposed allocation.");
  } else {
    GST_ERROR_OBJECT (self,
        "Cannot process buffers that do not use the proposed allocation.");
    ret = GST_FLOW_ERROR;
    goto out;
  }

out:
  return ret;
}

void
gst_vpi_upload_finalize (GObject * object)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_clear_object (&self->upstream_buffer_pool);

  G_OBJECT_CLASS (gst_vpi_upload_parent_class)->finalize (object);
}
