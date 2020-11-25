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

#include "gstvpiklttracker.h"

#include <gst/gst.h>
#include <vpi/Array.h>
#include <vpi/algo/KLTFeatureTracker.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_klt_tracker_debug_category);
#define GST_CAT_DEFAULT gst_vpi_klt_tracker_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_BE, GRAY16_LE }")

#define VPI_ARRAY_CAPACITY 128

struct _GstVpiKltTracker
{
  GstVpiFilter parent;

  VPIArray input_box_list;
  VPIArray input_pred_list;
  VPIArray output_box_list;
  VPIArray output_pred_list;
  VPIImage template_image;
  VPIKLTFeatureTrackerParams klt_params;
  VPIPayload klt;
};

/* prototypes */
static gboolean gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo
    * in_info, GstVideoInfo * out_info);
static GstFlowReturn gst_vpi_klt_tracker_transform_image (GstVpiFilter *
    filter, VPIStream stream, VPIImage in_image, VPIImage out_image);
static gboolean gst_vpi_klt_tracker_stop (GstBaseTransform * trans);
static void gst_vpi_klt_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_finalize (GObject * object);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiKltTracker, gst_vpi_klt_tracker,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_klt_tracker_debug_category,
        "vpiklttracker", 0, "debug category for vpiklttracker element"));

static void
gst_vpi_klt_tracker_class_init (GstVpiKltTrackerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI KLT Tacker", "Filter/Video",
      "VPI based KLT feature tracker element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_start);
  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_transform_image);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_stop);
  gobject_class->set_property = gst_vpi_klt_tracker_set_property;
  gobject_class->get_property = gst_vpi_klt_tracker_get_property;
  gobject_class->finalize = gst_vpi_klt_tracker_finalize;
}

static void
gst_vpi_klt_tracker_init (GstVpiKltTracker * self)
{
  self->klt_params.numberOfIterationsScaling = 20;
  self->klt_params.nccThresholdUpdate = 0.8;
  self->klt_params.nccThresholdKill = 0.6;
  self->klt_params.nccThresholdStop = 1.0;
  self->klt_params.maxScaleChange = 0.2;
  self->klt_params.maxTranslationChange = 1.5;
  self->klt_params.trackingType = VPI_KLT_INVERSE_COMPOSITIONAL;
}

static gboolean
gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiKltTracker *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  guint width = 0;
  guint height = 0;
  GstVideoFormat format = 0;
  VPIArrayData array_data = { 0 };
  VPIKLTTrackedBoundingBox bbox_array[VPI_ARRAY_CAPACITY] = { 0 };
  VPIHomographyTransform2D pred_array[VPI_ARRAY_CAPACITY] = { 0 };

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_KLT_TRACKER (filter);

  GST_DEBUG_OBJECT (self, "start");

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);
  format = GST_VIDEO_INFO_FORMAT (in_info);

  /*Set one bounding box */

  bbox_array[0].bbox.xform.mat3[0][0] = 1;
  bbox_array[0].bbox.xform.mat3[1][1] = 1;
  bbox_array[0].bbox.xform.mat3[0][2] = 613;
  bbox_array[0].bbox.xform.mat3[1][2] = 332;
  bbox_array[0].bbox.xform.mat3[2][2] = 1;
  bbox_array[0].bbox.width = 23;
  bbox_array[0].bbox.height = 23;
  /* Valid tracking */
  bbox_array[0].trackingStatus = 0;
  /* Must update */
  bbox_array[0].templateStatus = 1;
  /* Identity transformation at the beginning */
  pred_array[0].mat3[0][0] = 1;
  pred_array[0].mat3[1][1] = 1;
  pred_array[0].mat3[2][2] = 1;

  array_data.type = VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX;
  array_data.capacity = VPI_ARRAY_CAPACITY;
  array_data.size = 1;
  array_data.data = &bbox_array[0];

  status = vpiArrayCreateHostMemWrapper (&array_data, VPI_BACKEND_ALL,
      &self->input_box_list);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not wrap bounding boxes into VPIArray."), (NULL));
    ret = FALSE;
    goto out;
  }

  array_data.type = VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D;
  array_data.data = &pred_array[0];

  status = vpiArrayCreateHostMemWrapper (&array_data, VPI_BACKEND_ALL,
      &self->input_pred_list);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not wrap homographies into VPIArray."), (NULL));
    ret = FALSE;
    goto free_in_box_array;
  }

  status = vpiCreateKLTFeatureTracker (VPI_BACKEND_CUDA, width, height,
      gst_vpi_video_to_image_format (format), &self->klt);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create KLT tracker payload."), (NULL));
    ret = FALSE;
    goto free_in_pred_array;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX, VPI_BACKEND_ALL,
      &self->output_box_list);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output bounding box array."), (NULL));
    ret = FALSE;
    goto free_klt;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D, VPI_BACKEND_ALL,
      &self->output_pred_list);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output homographies array."), (NULL));
    ret = FALSE;
    goto free_out_box_array;
  }

  goto out;

free_out_box_array:
  vpiArrayDestroy (self->output_box_list);
  self->output_box_list = NULL;

free_klt:
  vpiPayloadDestroy (self->klt);
  self->klt = NULL;

free_in_pred_array:
  vpiArrayDestroy (self->input_pred_list);
  self->input_pred_list = NULL;

free_in_box_array:
  vpiArrayDestroy (self->input_box_list);
  self->input_box_list = NULL;

out:
  return ret;
}

static GstFlowReturn
gst_vpi_klt_tracker_transform_image (GstVpiFilter * filter, VPIStream stream,
    VPIImage in_image, VPIImage out_image)
{
  GstVpiKltTracker *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_KLT_TRACKER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  return ret;
}


void
gst_vpi_klt_tracker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_klt_tracker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vpi_klt_tracker_stop (GstBaseTransform * trans)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (trans);
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_klt_tracker_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");

  vpiArrayDestroy (self->input_pred_list);
  self->input_pred_list = NULL;

  vpiArrayDestroy (self->input_box_list);
  self->input_box_list = NULL;

  vpiArrayDestroy (self->output_pred_list);
  self->output_pred_list = NULL;

  vpiArrayDestroy (self->output_box_list);
  self->output_box_list = NULL;

  vpiPayloadDestroy (self->klt);
  self->klt = NULL;

  return ret;
}

void
gst_vpi_klt_tracker_finalize (GObject * object)
{
  GstVpiKltTracker *vpi_klt_tracker = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (vpi_klt_tracker, "finalize");

  G_OBJECT_CLASS (gst_vpi_klt_tracker_parent_class)->finalize (object);
}
