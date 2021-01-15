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

GST_DEBUG_CATEGORY_STATIC (gst_vpi_harris_detector_debug_category);
#define GST_CAT_DEFAULT gst_vpi_harris_detector_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8 }")

#define VPI_HARRIS_PARAMS_SIZE_ENUM (vpi_harris_params_size_enum_get_type ())
GType vpi_harris_params_size_enum_get_type (void);

/* PVA backend only allows 8192 */
#define VPI_ARRAY_CAPACITY 8192

/* To use with GstVideoRegionOfInterestMeta */
#define KEYPOINTS_SIZE 0
#define KEYPOINT_META_TYPE "keypoint"

#define HARRIS_PARAMS_SIZE_3 3
#define HARRIS_PARAMS_SIZE_5 5
#define HARRIS_PARAMS_SIZE_7 7

#define DEFAULT_PROP_MIN_NMS_DISTANCE_MIN 0
#define DEFAULT_PROP_MIN_NMS_DISTANCE_MAX G_MAXDOUBLE
#define DEFAULT_PROP_SENSITIVITY_MIN 0
#define DEFAULT_PROP_SENSITIVITY_MAX 1
#define DEFAULT_PROP_STRENGTH_THRESH_MIN 0
#define DEFAULT_PROP_STRENGTH_THRESH_MAX G_MAXDOUBLE

#define DEFAULT_PROP_GRADIENT_SIZE HARRIS_PARAMS_SIZE_5
#define DEFAULT_PROP_BLOCK_SIZE HARRIS_PARAMS_SIZE_5
#define DEFAULT_PROP_MIN_NMS_DISTANCE 8
#define DEFAULT_PROP_SENSITIVITY 0.01
#define DEFAULT_PROP_STRENGTH_THRESH 20

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

enum
{
  PROP_0,
  PROP_GRADIENT_SIZE,
  PROP_BLOCK_SIZE,
  PROP_MIN_NMS_DISTANCE,
  PROP_SENSITIVITY,
  PROP_STRENGTH_THRESH,
};

GType
vpi_harris_params_size_enum_get_type (void)
{
  static GType vpi_harris_params_size_enum_type = 0;
  static const GEnumValue values[] = {
    {HARRIS_PARAMS_SIZE_3, "Gradient/window size of 3",
        "3"},
    {HARRIS_PARAMS_SIZE_5, "Gradient/window size of 5",
        "5"},
    {HARRIS_PARAMS_SIZE_7, "Gradient/window size of 7",
        "7"},
    {0, NULL, NULL}
  };

  if (!vpi_harris_params_size_enum_type) {
    vpi_harris_params_size_enum_type =
        g_enum_register_static ("VpiHarrisParamsSize", values);
  }
  return vpi_harris_params_size_enum_type;
}

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

  g_object_class_install_property (gobject_class, PROP_GRADIENT_SIZE,
      g_param_spec_enum ("gradient-size", "Gradient size",
          "Gradient window size.",
          VPI_HARRIS_PARAMS_SIZE_ENUM, DEFAULT_PROP_GRADIENT_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BLOCK_SIZE,
      g_param_spec_enum ("block-size", "Block size",
          "Block window size used to compute the Harris Corner score.",
          VPI_HARRIS_PARAMS_SIZE_ENUM, DEFAULT_PROP_BLOCK_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MIN_NMS_DISTANCE,
      g_param_spec_double ("nms-distance", "Minimum NMS distance",
          "Non-maximum suppression radius, set to 0 to disable it. For PVA "
          "backend, this must be set to 8.",
          DEFAULT_PROP_MIN_NMS_DISTANCE_MIN, DEFAULT_PROP_MIN_NMS_DISTANCE_MAX,
          DEFAULT_PROP_MIN_NMS_DISTANCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SENSITIVITY,
      g_param_spec_double ("sensitivity", "Sensitivity threshold",
          "Specifies sensitivity threshold from the Harris-Stephens equation.",
          DEFAULT_PROP_SENSITIVITY_MIN, DEFAULT_PROP_SENSITIVITY_MAX,
          DEFAULT_PROP_SENSITIVITY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_STRENGTH_THRESH,
      g_param_spec_double ("strength-thresh", "Strength threshold",
          "Specifies the minimum threshold with which to eliminate Harris "
          "Corner scores.",
          DEFAULT_PROP_STRENGTH_THRESH_MIN, DEFAULT_PROP_STRENGTH_THRESH_MAX,
          DEFAULT_PROP_STRENGTH_THRESH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_harris_detector_init (GstVpiHarrisDetector * self)
{
  self->keypoints = NULL;
  self->scores = NULL;
  self->harris = NULL;
  self->harris_params.gradientSize = DEFAULT_PROP_GRADIENT_SIZE;
  self->harris_params.blockSize = DEFAULT_PROP_BLOCK_SIZE;
  self->harris_params.strengthThresh = DEFAULT_PROP_STRENGTH_THRESH;
  self->harris_params.sensitivity = DEFAULT_PROP_SENSITIVITY;
  /* PVA backend only allows 8 */
  self->harris_params.minNMSDistance = DEFAULT_PROP_MIN_NMS_DISTANCE;
}

static gboolean
gst_vpi_harris_detector_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiHarrisDetector *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  guint width = 0;
  guint height = 0;
  gint backend;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_HARRIS_DETECTOR (filter);

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
  backend = gst_vpi_filter_get_backend (filter);

  status = vpiCreateHarrisCornerDetector (backend, width, height,
      &self->harris);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create Harris corner detector"),
        ("%s", vpiStatusGetName (status)));
    ret = FALSE;
    goto free_scores_array;
  }

  goto out;

free_scores_array:
  vpiArrayDestroy (self->scores);
  self->scores = NULL;

free_keypoints_array:
  vpiArrayDestroy (self->keypoints);
  self->keypoints = NULL;

out:
  return ret;
}

static void
gst_vpi_harris_detector_add_keypoints_meta (GstVpiHarrisDetector * self,
    GstBuffer * buffer)
{
  VPIArrayData out_keypoints_data = { 0 };
  VPIKeypoint *out_keypoints = NULL;
  guint k = 0;
  guint x, y = 0;

  g_return_if_fail (self);
  g_return_if_fail (buffer);

  vpiArrayLock (self->keypoints, VPI_LOCK_READ, &out_keypoints_data);
  out_keypoints = (VPIKeypoint *) out_keypoints_data.data;
  vpiArrayUnlock (self->keypoints);

  for (k = 0; k < out_keypoints_data.size; k++) {
    x = (guint) out_keypoints[k].x;
    y = (guint) out_keypoints[k].y;

    gst_buffer_add_video_region_of_interest_meta (buffer,
        KEYPOINT_META_TYPE, x, y, KEYPOINTS_SIZE, KEYPOINTS_SIZE);
  }
}

static GstFlowReturn
gst_vpi_harris_detector_transform_image_ip (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * frame)
{
  GstVpiHarrisDetector *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIHarrisCornerDetectorParams params = { 0 };

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->image, GST_FLOW_ERROR);

  self = GST_VPI_HARRIS_DETECTOR (filter);

  GST_LOG_OBJECT (self, "Transform image ip");

  GST_OBJECT_LOCK (self);
  params = self->harris_params;
  GST_OBJECT_UNLOCK (self);

  vpiSubmitHarrisCornerDetector (stream, self->harris, frame->image,
      self->keypoints, self->scores, &params);
  vpiStreamSync (stream);

  gst_vpi_harris_detector_add_keypoints_meta (self, frame->buffer);

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
    case PROP_GRADIENT_SIZE:
      self->harris_params.gradientSize = g_value_get_enum (value);
      break;
    case PROP_BLOCK_SIZE:
      self->harris_params.blockSize = g_value_get_enum (value);
      break;
    case PROP_MIN_NMS_DISTANCE:
      self->harris_params.minNMSDistance = g_value_get_double (value);
      break;
    case PROP_SENSITIVITY:
      self->harris_params.sensitivity = g_value_get_double (value);
      break;
    case PROP_STRENGTH_THRESH:
      self->harris_params.strengthThresh = g_value_get_double (value);
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
    case PROP_GRADIENT_SIZE:
      g_value_set_enum (value, self->harris_params.gradientSize);
      break;
    case PROP_BLOCK_SIZE:
      g_value_set_enum (value, self->harris_params.blockSize);
      break;
    case PROP_MIN_NMS_DISTANCE:
      g_value_set_double (value, self->harris_params.minNMSDistance);
      break;
    case PROP_SENSITIVITY:
      g_value_set_double (value, self->harris_params.sensitivity);
      break;
    case PROP_STRENGTH_THRESH:
      g_value_set_double (value, self->harris_params.strengthThresh);
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

  vpiArrayDestroy (self->keypoints);
  self->keypoints = NULL;

  vpiArrayDestroy (self->scores);
  self->scores = NULL;

  vpiPayloadDestroy (self->harris);
  self->harris = NULL;

  return ret;
}
