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

#include "gstvpiharrisdetector.h"

#include <gst/gst.h>
#include <vpi/algo/HarrisCornerDetector.h>
#include <vpi/Array.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_harris_detector_debug_category);
#define GST_CAT_DEFAULT gst_vpi_harris_detector_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8 }")

/* PVA backend only allows 8192 */
#define VPI_ARRAY_CAPACITY 8192

struct _GstVpiHarrisDetector
{
  GstVpiFilter parent;
  VPIArray keypoints;
  VPIArray scores;
  VPIHarrisCornerDetectorParams harris_params;
  VPIPayload harris;
};

/* prototypes */
static gboolean gst_vpi_harris_detector_start (GstVpiFilter * filter,
    GstVideoInfo * in_info, GstVideoInfo * out_info);
static GstFlowReturn gst_vpi_harris_detector_transform_image_ip (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * frame);
static gboolean gst_vpi_harris_detector_stop (GstBaseTransform * trans);
static void gst_vpi_harris_detector_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_harris_detector_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_harris_detector_finalize (GObject * object);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiHarrisDetector, gst_vpi_harris_detector,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_harris_detector_debug_category,
        "vpiharrisdetector", 0,
        "debug category for vpiharrisdetector element"));

static void
gst_vpi_harris_detector_class_init (GstVpiHarrisDetectorClass * klass)
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
      "VPI Harris Corner Detector", "Filter/Video",
      "VPI based Harris corner detector.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_harris_detector_start);
  vpi_filter_class->transform_image_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_harris_detector_transform_image_ip);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_harris_detector_stop);
  gobject_class->set_property = gst_vpi_harris_detector_set_property;
  gobject_class->get_property = gst_vpi_harris_detector_get_property;
  gobject_class->finalize = gst_vpi_harris_detector_finalize;
}

static void
gst_vpi_harris_detector_init (GstVpiHarrisDetector * self)
{
  self->keypoints = NULL;
  self->scores = NULL;
  self->harris = NULL;
  /* TODO: Expose these as properties */
  self->harris_params.gradientSize = 5;
  self->harris_params.blockSize = 5;
  self->harris_params.strengthThresh = 20;
  self->harris_params.sensitivity = 0.01;
  /* PVA backend only allows 8 */
  self->harris_params.minNMSDistance = 8;
}

static gboolean
gst_vpi_harris_detector_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiHarrisDetector *self = GST_VPI_HARRIS_DETECTOR (filter);
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  guint width = 0;
  guint height = 0;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  GST_DEBUG_OBJECT (self, "start");

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY, VPI_ARRAY_TYPE_KEYPOINT,
      VPI_BACKEND_ALL, &self->keypoints);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create keypoints array."), ("%s",
            vpiStatusGetName (status)));
    ret = FALSE;
    goto out;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY, VPI_ARRAY_TYPE_U32,
      VPI_BACKEND_ALL, &self->scores);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create scores array."), ("%s", vpiStatusGetName (status)));
    ret = FALSE;
    goto free_keypoints_array;
  }

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);

  status = vpiCreateHarrisCornerDetector (VPI_BACKEND_CUDA, width, height,
      &self->harris);

  goto out;

free_keypoints_array:
  vpiArrayDestroy (self->keypoints);
  self->keypoints = NULL;

out:
  return ret;
}

static GstFlowReturn
gst_vpi_harris_detector_transform_image_ip (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * frame)
{
  GstVpiHarrisDetector *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->image, GST_FLOW_ERROR);

  self = GST_VPI_HARRIS_DETECTOR (filter);

  GST_LOG_OBJECT (self, "Transform image ip");

  GST_OBJECT_LOCK (self);

  vpiSubmitHarrisCornerDetector (stream, self->harris, frame->image,
      self->keypoints, self->scores, &self->harris_params);
  vpiStreamSync (stream);

  GST_OBJECT_UNLOCK (self);

  return ret;
}

void
gst_vpi_harris_detector_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiHarrisDetector *self = GST_VPI_HARRIS_DETECTOR (object);

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
gst_vpi_harris_detector_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiHarrisDetector *self = GST_VPI_HARRIS_DETECTOR (object);

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
gst_vpi_harris_detector_stop (GstBaseTransform * trans)
{
  GstVpiHarrisDetector *self = GST_VPI_HARRIS_DETECTOR (trans);
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_harris_detector_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");

  GST_OBJECT_LOCK (self);

  vpiArrayDestroy (self->keypoints);
  self->keypoints = NULL;

  vpiArrayDestroy (self->scores);
  self->scores = NULL;

  vpiPayloadDestroy (self->harris);
  self->harris = NULL;

  GST_OBJECT_UNLOCK (self);

  return ret;
}

void
gst_vpi_harris_detector_finalize (GObject * object)
{
  GstVpiHarrisDetector *self = GST_VPI_HARRIS_DETECTOR (object);

  GST_DEBUG_OBJECT (self, "finalize");

  G_OBJECT_CLASS (gst_vpi_harris_detector_parent_class)->finalize (object);
}
