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

#include "gstcudabufferpool.h"
#include "gstcudameta.h"
#include "gstvpifilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_filter_debug_category

typedef struct _GstVpiFilterPrivate GstVpiFilterPrivate;

struct _GstVpiFilterPrivate
{
  GstVideoInfo out_caps_info;
  GstCudaBufferPool *downstream_buffer_pool;
};

static void gst_vpi_filter_finalize (GObject * object);
static gboolean gst_vpi_filter_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);

enum
{
  PROP_0
};

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

  g_type_class_add_private (gobject_class, sizeof (GstVpiFilterPrivate));

  base_transform_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_vpi_filter_decide_allocation);
  gobject_class->finalize = gst_vpi_filter_finalize;
}

static void
gst_vpi_filter_init (GstVpiFilter * self)
{
  GstVpiFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_FILTER,
      GstVpiFilterPrivate);

  priv->downstream_buffer_pool = NULL;
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

  GST_LOG_OBJECT (self, "Computed buffer size of %" G_GUINT64_FORMAT, size);

  return size;
}

static gboolean
gst_vpi_filter_create_buffer_pool (GstVpiFilter * self,
    GstCudaBufferPool * buffer_pool, GstQuery * query)
{
  GstVpiFilterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GST_TYPE_VPI_FILTER, GstVpiFilterPrivate);
  gsize size = 0;
  GstStructure *config = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  gboolean need_pool = FALSE;
  gboolean ret = FALSE;

  GST_INFO_OBJECT (self, "Creating buffer pool");

  gst_query_parse_allocation (query, &caps, &need_pool);

  pool = GST_BUFFER_POOL (buffer_pool);
  size = gst_vpi_filter_compute_size (self, &priv->out_caps_info);

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

static gboolean
gst_vpi_filter_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstVpiFilter *self = GST_VPI_FILTER (trans);
  GstVpiFilterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GST_TYPE_VPI_FILTER, GstVpiFilterPrivate);
  gint npool = 0;

  if (!priv->downstream_buffer_pool) {
    priv->downstream_buffer_pool =
        g_object_new (GST_CUDA_TYPE_BUFFER_POOL, NULL);
  }

  /* We can't use dowstream provided pools, since we need unified memory */
  for (npool = 0; npool < gst_query_get_n_allocation_pools (query); npool++) {
    GstBufferPool *pool;

    gst_query_parse_nth_allocation_pool (query, npool, &pool, NULL, NULL, NULL);
    GST_INFO_OBJECT (self, "Discarding downstream pool \"%s\"",
        GST_OBJECT_NAME (pool));
    gst_object_unref (pool);

    gst_query_remove_nth_allocation_pool (query, npool);
  }

  return gst_vpi_filter_create_buffer_pool (self,
      priv->downstream_buffer_pool, query);
}

static void
gst_vpi_filter_finalize (GObject * object)
{
  GstVpiFilterPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (object,
      GST_TYPE_VPI_FILTER, GstVpiFilterPrivate);

  GST_DEBUG_OBJECT (object, "Finalize VPI filter");

  g_clear_object (&priv->downstream_buffer_pool);

  G_OBJECT_CLASS (gst_vpi_filter_parent_class)->finalize (object);
}
