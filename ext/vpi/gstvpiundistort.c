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

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 800
#define DEFAULT_SENSOR_WIDTH 22.2
#define DEFAULT_FOCAL_LENGTH 7.5
#define DEFAULT_F_PARAMETER (DEFAULT_FOCAL_LENGTH * DEFAULT_WIDTH / DEFAULT_SENSOR_WIDTH)

#define DEFAULT_PROP_EXTRINSIC_MATRIX { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0} }
#define DEFAULT_PROP_INTRINSIC_MATRIX { {DEFAULT_F_PARAMETER, 0.0, DEFAULT_WIDTH / 2.0}, {0.0, DEFAULT_F_PARAMETER, DEFAULT_HEIGHT / 2.0} }

struct _GstVpiUndistort
{
  GstVpiFilter parent;
  VPIPayload warp;
  VPICameraExtrinsic extrinsic;
  VPICameraIntrinsic intrinsic;
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
  PROP_0,
  PROP_EXTRINSIC_MATRIX,
  PROP_INTRINSIC_MATRIX
};

enum
{
  EXTRINSIC,
  INTRINSIC
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

  g_object_class_install_property (gobject_class, PROP_EXTRINSIC_MATRIX,
      gst_param_spec_array ("extrinsic",
          "Extrinsic calibration matrix",
          "3x4 matrix resulting of concatenation of 3x3 rotation matrix with "
          "3x1 vector containing the position of the origin world coordinate "
          "system expressed in coordinates of camera-centered system.\n"
          "Example: <<1.0,0.0,0.0,0.0>,<0.0,1.0,0.0,0.0>,<0.0,0.0,1.0,0.0>>",
          gst_param_spec_array ("e-matrix-rows", "rows", "rows",
              g_param_spec_double ("e-matrix-cols", "cols", "cols",
                  -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTRINSIC_MATRIX,
      gst_param_spec_array ("intrinsic",
          "Intrinsic calibration matrix",
          "2x3 matrix with the first row containing the focal length in pixels "
          "in x, the skew and the principal point in x and in the second row 0,"
          " the focal length in pixels in y and principal point in y.\n"
          "Example: <<fx,s,cx>,<0.0,fy,cy>>",
          gst_param_spec_array ("i-matrix-rows", "rows", "rows",
              g_param_spec_double ("i-matrix-cols", "cols", "cols",
                  0, G_MAXDOUBLE, 0,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_undistort_init (GstVpiUndistort * self)
{
  VPICameraExtrinsic extrinsic = DEFAULT_PROP_EXTRINSIC_MATRIX;
  VPICameraIntrinsic intrinsic = DEFAULT_PROP_INTRINSIC_MATRIX;

  self->warp = NULL;
  memcpy (&self->extrinsic, &extrinsic, sizeof (extrinsic));
  memcpy (&self->intrinsic, &intrinsic, sizeof (intrinsic));
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

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_UNDISTORT (filter);

  GST_DEBUG_OBJECT (self, "start");

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);

  map.grid.numHorizRegions = 1;
  map.grid.numVertRegions = 1;
  map.grid.regionWidth[0] = width;
  map.grid.regionHeight[0] = height;
  map.grid.horizInterval[0] = 1;
  map.grid.vertInterval[0] = 1;
  vpiWarpMapAllocData (&map);

  /* TODO: Expose this parameters as element properties */
  fisheye.mapping = VPI_FISHEYE_EQUIDISTANT;
  fisheye.k1 = -0.126;
  fisheye.k2 = 0.004;
  fisheye.k3 = 0;
  fisheye.k4 = 0;

  /* TODO: Adjust calibration matrices if the default were used */
  status = vpiWarpMapGenerateFromFisheyeLensDistortionModel (self->intrinsic,
      self->extrinsic, self->intrinsic, &fisheye, &map);

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

static void
gst_vpi_undistort_convert_gst_array_to_calib_matrix (GstVpiUndistort * self,
    const GValue * array, guint matrix_type)
{
  guint rows = 0;
  guint cols = 0;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (self);
  g_return_if_fail (array);

  rows = gst_value_array_get_size (array);
  cols = gst_value_array_get_size (gst_value_array_get_value (array, 0));

  if (EXTRINSIC == matrix_type && 3 == rows && 4 == cols) {
    for (i = 0; i < rows; i++) {
      const GValue *row = gst_value_array_get_value (array, i);
      for (j = 0; j < cols; j++) {
        self->extrinsic[i][j] =
            g_value_get_double (gst_value_array_get_value (row, j));
      }
    }
  } else if (INTRINSIC == matrix_type && 2 == rows && 3 == cols) {
    for (i = 0; i < rows; i++) {
      const GValue *row = gst_value_array_get_value (array, i);
      for (j = 0; j < cols; j++) {
        self->intrinsic[i][j] =
            g_value_get_double (gst_value_array_get_value (row, j));
      }
    }
  } else {
    GST_WARNING_OBJECT (self, "Invalid %s matrix dimensions. Using default.",
        matrix_type == EXTRINSIC ? "extrinsic" : "intrinsic");
  }
}

static void
gst_vpi_undistort_convert_calib_matrix_to_gst_array (GstVpiUndistort * self,
    GValue * array, guint matrix_type)
{
  GValue row = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint rows = 0;
  guint cols = 0;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (self);
  g_return_if_fail (array);

  if (EXTRINSIC == matrix_type) {
    rows = 3;
    cols = 4;

    for (i = 0; i < rows; i++) {
      g_value_init (&row, GST_TYPE_ARRAY);

      for (j = 0; j < cols; i++) {
        g_value_init (&value, G_TYPE_DOUBLE);
        g_value_set_double (&value, self->extrinsic[i][j]);
        gst_value_array_append_value (&row, &value);
        g_value_unset (&value);
      }
    }
    gst_value_array_append_value (array, &row);
    g_value_unset (&row);
  } else if (INTRINSIC == matrix_type) {
    rows = 2;
    cols = 3;

    for (i = 0; i < rows; i++) {
      g_value_init (&row, GST_TYPE_ARRAY);

      for (j = 0; j < cols; i++) {
        g_value_init (&value, G_TYPE_DOUBLE);
        g_value_set_double (&value, self->intrinsic[i][j]);
        gst_value_array_append_value (&row, &value);
        g_value_unset (&value);
      }
    }
    gst_value_array_append_value (array, &row);
    g_value_unset (&row);
  }
}

void
gst_vpi_undistort_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *self = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_EXTRINSIC_MATRIX:
      gst_vpi_undistort_convert_gst_array_to_calib_matrix (self, value,
          EXTRINSIC);
      break;
    case PROP_INTRINSIC_MATRIX:
      gst_vpi_undistort_convert_gst_array_to_calib_matrix (self, value,
          INTRINSIC);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_undistort_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *self = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_EXTRINSIC_MATRIX:
      gst_vpi_undistort_convert_calib_matrix_to_gst_array (self, value,
          EXTRINSIC);
      break;
    case PROP_INTRINSIC_MATRIX:
      gst_vpi_undistort_convert_calib_matrix_to_gst_array (self, value,
          INTRINSIC);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
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
