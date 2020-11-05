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

#include "gstvpiundistort.h"

#include <gst/gst.h>

#include <vpi/algo/Remap.h>
#include <vpi/LensDistortionModels.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_undistort_debug_category);
#define GST_CAT_DEFAULT gst_vpi_undistort_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "NV12")

struct _GstVpiUndistort
{
  GstVpiFilter parent;
  VPIPayload warp;
};

/* prototypes */
static GstFlowReturn gst_vpi_undistort_transform_image (GstVpiFilter *
    filter, VPIStream stream, VPIImage in_image, VPIImage out_image);
static gboolean gst_vpi_undistort_start (GstVpiFilter * self, GstVideoInfo *
    in_info, GstVideoInfo * out_info);
static gboolean gst_vpi_undistort_stop (GstBaseTransform * trans);
static void gst_vpi_undistort_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_undistort_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_undistort_finalize (GObject * object);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiUndistort, gst_vpi_undistort,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_undistort_debug_category, "vpiundistort",
        0, "debug category for vpiundistort element"));

static void
gst_vpi_undistort_class_init (GstVpiUndistortClass * klass)
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
      "VPI Undistort", "Filter/Video",
      "VPI based camera lens undistort element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_undistort_start);
  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_undistort_transform_image);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_undistort_stop);
  gobject_class->set_property = gst_vpi_undistort_set_property;
  gobject_class->get_property = gst_vpi_undistort_get_property;
  gobject_class->finalize = gst_vpi_undistort_finalize;
}

static void
gst_vpi_undistort_init (GstVpiUndistort * self)
{
  self->warp = NULL;
}

static gboolean
gst_vpi_undistort_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiUndistort *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  VPIFisheyeLensDistortionModel fisheye = { 0 };
  VPIWarpMap map = { 0 };
  guint width = 0;
  guint height = 0;
  /* TODO: Expose this parameters as element properties */
  gdouble sensor_width = 22.2;
  gdouble focal_length = 7.5;
  gdouble f = 0;
  VPICameraIntrinsic intrinsic = { 0 };
  VPICameraExtrinsic extrinsic = { {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0}
  };

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_UNDISTORT (filter);

  GST_DEBUG_OBJECT (self, "start");

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);

  f = focal_length * width / sensor_width;
  intrinsic[0][0] = intrinsic[1][1] = f;
  intrinsic[0][2] = width / 2.0;
  intrinsic[1][2] = height / 2.0;

  map.grid.numHorizRegions = 1;
  map.grid.numVertRegions = 1;
  map.grid.regionWidth[0] = width;
  map.grid.regionHeight[0] = height;
  map.grid.horizInterval[0] = 1;
  map.grid.vertInterval[0] = 1;
  vpiWarpMapAllocData (&map);

  fisheye.mapping = VPI_FISHEYE_EQUIDISTANT;
  fisheye.k1 = -0.126;
  fisheye.k2 = 0.004;
  fisheye.k3 = 0;
  fisheye.k4 = 0;

  status = vpiWarpMapGenerateFromFisheyeLensDistortionModel (intrinsic,
      extrinsic, intrinsic, &fisheye, &map);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not generate warp map."), (NULL));
    ret = FALSE;
    goto out;
  }

  status = vpiCreateRemap (VPI_BACKEND_CUDA, &map, &self->warp);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create payload."), (NULL));
    ret = FALSE;
  }

out:
  vpiWarpMapFreeData (&map);
  return ret;
}

static GstFlowReturn
gst_vpi_undistort_transform_image (GstVpiFilter * filter, VPIStream stream,
    VPIImage in_image, VPIImage out_image)
{
  GstVpiUndistort *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_UNDISTORT (filter);

  GST_LOG_OBJECT (self, "Transform image");

  status = vpiSubmitRemap (stream, self->warp, in_image, out_image,
      VPI_INTERP_LINEAR, VPI_BOUNDARY_COND_ZERO);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

void
gst_vpi_undistort_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_undistort_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
gst_vpi_undistort_stop (GstBaseTransform * trans)
{
  GstVpiUndistort *self = GST_VPI_UNDISTORT (trans);
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_undistort_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");

  vpiPayloadDestroy (self->warp);
  self->warp = NULL;

  return ret;
}

void
gst_vpi_undistort_finalize (GObject * object)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "finalize");

  G_OBJECT_CLASS (gst_vpi_undistort_parent_class)->finalize (object);
}
