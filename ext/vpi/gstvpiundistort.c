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

#include <glib/gprintf.h>
#include <gst/gst.h>
#include <vpi/algo/Remap.h>
#include <vpi/LensDistortionModels.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_undistort_debug_category);
#define GST_CAT_DEFAULT gst_vpi_undistort_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "NV12")

#define ROWS_INTRINSIC 2
#define COLS_INTRINSIC 3
#define ROWS_EXTRINSIC 3
#define COLS_EXTRINSIC 4

#define NUM_COEFFICIENTS 8
#define DEFAULT_SENSOR_WIDTH 22.2
#define DEFAULT_FOCAL_LENGTH 7.5

#define DEFAULT_PROP_COEF_MIN -G_MAXDOUBLE
#define DEFAULT_PROP_COEF_MAX G_MAXDOUBLE

#define DEFAULT_PROP_EXTRINSIC_MATRIX { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0} }
#define DEFAULT_PROP_INTRINSIC_MATRIX { 0 }
#define DEFAULT_PROP_INTERPOLATOR VPI_INTERP_CATMULL_ROM
#define DEFAULT_PROP_DISTORTION_MODEL FISHEYE
#define DEFAULT_PROP_FISHEYE_MAPPING VPI_FISHEYE_EQUIDISTANT
#define DEFAULT_PROP_COEF 0.0


struct _GstVpiUndistort
{
  GstVpiFilter parent;
  VPIPayload warp;
  VPICameraExtrinsic extrinsic;
  VPICameraIntrinsic intrinsic;
  gboolean set_intrinsic_matrix;
  gint interpolator;
  gint distortion_model;
  gint fisheye_mapping;
  gdouble coefficients[NUM_COEFFICIENTS];
};

/* prototypes */
static GstFlowReturn gst_vpi_undistort_transform_image (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame);
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
  PROP_INTRINSIC_MATRIX,
  PROP_INTERPOLATOR,
  PROP_DISTORTION_MODEL,
  PROP_FISHEYE_MAPPING,
  PROP_K1,
  PROP_K2,
  PROP_K3,
  PROP_K4,
  PROP_K5,
  PROP_K6,
  PROP_P1,
  PROP_P2
};

enum
{
  EXTRINSIC,
  INTRINSIC
};

enum
{
  FISHEYE,
  POLYNOMIAL
};

enum
{
  K1,
  K2,
  K3,
  K4,
  K5,
  K6,
  P1,
  P2
};

GType
vpi_distortion_model_enum_get_type (void)
{
  static GType vpi_distortion_model_enum_type = 0;
  static const GEnumValue values[] = {
    {FISHEYE, "Fisheye distortion model",
        "fisheye"},
    {POLYNOMIAL, "Polynomial (Brown-Conrady) distortion model",
        "polynomial"},
    {0, NULL, NULL}
  };

  if (!vpi_distortion_model_enum_type) {
    vpi_distortion_model_enum_type =
        g_enum_register_static ("VpiDistortionModel", values);
  }

  return vpi_distortion_model_enum_type;
}

GType
vpi_fisheye_mapping_enum_get_type (void)
{
  static GType vpi_fisheye_mapping_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_FISHEYE_EQUIDISTANT, "Specifies the equidistant fisheye mapping.",
        "equidistant"},
    {VPI_FISHEYE_EQUISOLID, "Specifies the equisolid fisheye mapping.",
        "equisolid"},
    {VPI_FISHEYE_ORTHOGRAPHIC, "Specifies the ortographic fisheye mapping.",
        "ortographic"},
    {VPI_FISHEYE_STEREOGRAPHIC, "Specifies the stereographic fisheye mapping.",
        "stereographic"},
    {0, NULL, NULL}
  };

  if (!vpi_fisheye_mapping_enum_type) {
    vpi_fisheye_mapping_enum_type =
        g_enum_register_static ("VpiFisheyeMapping", values);
  }

  return vpi_fisheye_mapping_enum_type;
}

GType
vpi_interpolator_enum_get_type (void)
{
  static GType vpi_interpolator_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_INTERP_NEAREST, "Nearest neighbor interpolation.",
        "nearest"},
    {VPI_INTERP_LINEAR, "Fast linear interpolation.",
        "linear"},
    {VPI_INTERP_CATMULL_ROM, "Fast Catmull-Rom cubic interpolation.",
        "catmull"},
    {0, NULL, NULL}
  };

  if (!vpi_interpolator_enum_type) {
    vpi_interpolator_enum_type =
        g_enum_register_static ("VpiInterpolator", values);
  }

  return vpi_interpolator_enum_type;
}

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
          "Usage example: <<1.0,0.0,0.0,0.0>,<0.0,1.0,0.0,0.0>,<0.0,0.0,1.0,0.0>>",
          gst_param_spec_array ("e-matrix-rows", "rows", "rows",
              g_param_spec_double ("e-matrix-cols", "cols", "cols",
                  -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTRINSIC_MATRIX,
      gst_param_spec_array ("intrinsic",
          "Intrinsic calibration matrix",
          "2x3 matrix with the parameters: [[fx,s,cx],[0,fy,cy]] where:\n"
          "fx, fy: focal length in pixels in x and y direction.\ns: skew\n"
          "cx, cy: the principal point in x and y direction.\nIf not provided,"
          " a default calibration matrix will be created.\n"
          "Usage example: <<fx,s,cx>,<0.0,fy,cy>>",
          gst_param_spec_array ("i-matrix-rows", "rows", "rows",
              g_param_spec_double ("i-matrix-cols", "cols", "cols",
                  0, G_MAXDOUBLE, 0,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTERPOLATOR,
      g_param_spec_enum ("interpolator", "Interpolation method",
          "Interpolation method to be used.",
          VPI_INTERPOLATORS_ENUM, DEFAULT_PROP_INTERPOLATOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DISTORTION_MODEL,
      g_param_spec_enum ("model", "Distortion model",
          "Type of distortion model to use.",
          VPI_DISTORTION_MODELS_ENUM, DEFAULT_PROP_DISTORTION_MODEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FISHEYE_MAPPING,
      g_param_spec_enum ("mapping",
          "Fisheye mapping. Only if model is fisheye.",
          "Type of fisheye lens mapping types.", VPI_FISHEYE_MAPPINGS_ENUM,
          DEFAULT_PROP_FISHEYE_MAPPING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K1,
      g_param_spec_double ("k1", "Distortion coefficient",
          "Distortion coefficient 1 of the chosen distortion model (fisheye "
          "or polynomial).",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K2,
      g_param_spec_double ("k2", "Distortion coefficient",
          "Distortion coefficient 2 of the chosen distortion model (fisheye "
          "or polynomial).",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K3,
      g_param_spec_double ("k3", "Distortion coefficient",
          "Distortion coefficient 3 of the chosen distortion model (fisheye "
          "or polynomial).",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K4,
      g_param_spec_double ("k4", "Distortion coefficient",
          "Distortion coefficient 4 of the chosen distortion model (fisheye "
          "or polynomial).",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K5,
      g_param_spec_double ("k5", "Distortion coefficient",
          "Distortion coefficient 5. Only for polynomial model.",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_K6,
      g_param_spec_double ("k6", "Distortion coefficient",
          "Distortion coefficient 6. Only for polynomial model.",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_P1,
      g_param_spec_double ("p1", "Tangential distortion coefficient",
          "Tangential distortion coefficient 1. Only for polynomial model.",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_P2,
      g_param_spec_double ("p2", "Tangential distortion coefficient",
          "Tangential distortion coefficient 2. Only for polynomial model.",
          DEFAULT_PROP_COEF_MIN, DEFAULT_PROP_COEF_MAX, DEFAULT_PROP_COEF,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_undistort_init (GstVpiUndistort * self)
{
  VPICameraExtrinsic extrinsic = DEFAULT_PROP_EXTRINSIC_MATRIX;
  VPICameraIntrinsic intrinsic = DEFAULT_PROP_INTRINSIC_MATRIX;
  gdouble coefficients[NUM_COEFFICIENTS] = { 0 };

  self->warp = NULL;
  self->set_intrinsic_matrix = FALSE;
  self->interpolator = DEFAULT_PROP_INTERPOLATOR;
  self->distortion_model = DEFAULT_PROP_DISTORTION_MODEL;
  self->fisheye_mapping = DEFAULT_PROP_FISHEYE_MAPPING;
  memcpy (&self->extrinsic, &extrinsic, sizeof (extrinsic));
  memcpy (&self->intrinsic, &intrinsic, sizeof (intrinsic));
  memcpy (&self->coefficients, &coefficients, sizeof (coefficients));
}

static void
append_string_format (gchar ** dest, const char *format, ...)
{

  va_list args;
  gint length = 0;
  gchar *new_string = NULL;

  g_return_if_fail (dest);
  g_return_if_fail (format);

  va_start (args, format);

  length = g_vsnprintf (NULL, 0, format, args) + 1;
  new_string = g_malloc (length);
  g_vsprintf (new_string, format, args);

  if (!*dest) {
    *dest = g_malloc (length);
    g_strlcpy (*dest, new_string, length);
  } else {
    length = length + strlen (*dest);
    *dest = g_realloc (*dest, length);
    g_strlcat (*dest, new_string, length);
  }

  va_end (args);
  g_free (new_string);
}

static gchar *
c_array_to_string (float *c_array, guint rows, guint cols)
{
  gchar *string = NULL;
  guint i = 0;
  guint j = 0;

  g_return_val_if_fail (c_array, NULL);

  append_string_format (&string, "< ");
  for (i = 0; i < rows; i++) {
    append_string_format (&string, "< ");
    for (j = 0; j < cols; j++) {
      append_string_format (&string, "%.3f ", c_array[i * cols + j]);
    }
    append_string_format (&string, "> ");
  }
  append_string_format (&string, ">");

  return string;
}

static void
gst_vpi_undistort_summarize_properties (GstVpiUndistort * self)
{
  gchar *summary = NULL;
  gchar *intrinsic_str = NULL;
  gchar *extrinsic_str = NULL;
  float *intrinsic = NULL;
  float *extrinsic = NULL;

  g_return_if_fail (self);

  intrinsic = g_malloc (sizeof (self->intrinsic));
  memcpy (intrinsic, &self->intrinsic, sizeof (self->intrinsic));
  intrinsic_str = c_array_to_string (intrinsic, ROWS_INTRINSIC, COLS_INTRINSIC);
  extrinsic = g_malloc (sizeof (self->extrinsic));
  memcpy (extrinsic, &self->extrinsic, sizeof (self->extrinsic));
  extrinsic_str = c_array_to_string (extrinsic, ROWS_EXTRINSIC, COLS_EXTRINSIC);

  append_string_format (&summary, "\nProperties summary:\nextrinsic=%s\n"
      "intrinsic=%s\ninterpolator=%d\nmodel=%d\nk1=%f\nk2=%f\nk3=%f\nk4=%f\n",
      extrinsic_str, intrinsic_str, self->interpolator, self->distortion_model,
      self->coefficients[K1], self->coefficients[K2], self->coefficients[K3],
      self->coefficients[K4]);

  if (self->distortion_model == POLYNOMIAL) {
    append_string_format (&summary, "k5=%f\nk6=%f\np1=%f\np2=%f\n",
        self->coefficients[K5], self->coefficients[K6], self->coefficients[P1],
        self->coefficients[P2]);
  } else {
    append_string_format (&summary, "mapping=%d", self->fisheye_mapping);
  }

  GST_INFO_OBJECT (self, "%s", summary);

  g_free (intrinsic);
  g_free (intrinsic_str);
  g_free (extrinsic);
  g_free (extrinsic_str);
  g_free (summary);
}

static gboolean
gst_vpi_undistort_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiUndistort *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  VPIWarpMap map = { 0 };
  guint width = 0;
  guint height = 0;
  gint backend = VPI_BACKEND_INVALID;

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

  /* Create default intrinsic matrix if not provided by user */
  if (!self->set_intrinsic_matrix) {
    gdouble f = DEFAULT_FOCAL_LENGTH * width / DEFAULT_SENSOR_WIDTH;
    float intrinsic[ROWS_INTRINSIC * COLS_INTRINSIC] = { f, 0, width / 2.0, 0,
      f, height / 2.0
    };
    gchar *intrinsic_str = NULL;

    memcpy (&self->intrinsic, &intrinsic, sizeof (intrinsic));
    intrinsic_str = c_array_to_string (intrinsic, ROWS_INTRINSIC,
        COLS_INTRINSIC);
    GST_WARNING_OBJECT (self,
        "Calibration matrix not set. Using default matrix %s.", intrinsic_str);
    g_free (intrinsic_str);
  }

  if (self->distortion_model == FISHEYE) {
    VPIFisheyeLensDistortionModel fisheye = { self->fisheye_mapping,
      self->coefficients[K1], self->coefficients[K2], self->coefficients[K3],
      self->coefficients[K4]
    };
    status = vpiWarpMapGenerateFromFisheyeLensDistortionModel (self->intrinsic,
        self->extrinsic, self->intrinsic, &fisheye, &map);

  } else {
    VPIPolynomialLensDistortionModel polynomial = { self->coefficients[K1],
      self->coefficients[K2], self->coefficients[K3], self->coefficients[K4],
      self->coefficients[K5], self->coefficients[K6], self->coefficients[P1],
      self->coefficients[P2]
    };
    status =
        vpiWarpMapGenerateFromPolynomialLensDistortionModel (self->intrinsic,
        self->extrinsic, self->intrinsic, &polynomial, &map);
  }

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not generate warp map."), (NULL));
    ret = FALSE;
    goto out;
  }

  backend = gst_vpi_filter_get_backend (filter);
  status = vpiCreateRemap (backend, &map, &self->warp);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create payload."), (NULL));
    ret = FALSE;
  }

out:
  vpiWarpMapFreeData (&map);
  gst_vpi_undistort_summarize_properties (self);
  return ret;
}

static GstFlowReturn
gst_vpi_undistort_transform_image (GstVpiFilter * filter, VPIStream stream,
    VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiUndistort *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_UNDISTORT (filter);

  GST_LOG_OBJECT (self, "Transform image");

  status =
      vpiSubmitRemap (stream, self->warp, in_frame->image, out_frame->image,
      self->interpolator, VPI_BOUNDARY_COND_ZERO);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

static float *
gst_array_to_c_array (const GValue * gst_array, guint * rows, guint * cols)
{
  const GValue *row = NULL;
  float *c_array = NULL;
  float value = 0;
  guint i = 0;
  guint j = 0;

  g_return_val_if_fail (gst_array, NULL);
  g_return_val_if_fail (rows, NULL);
  g_return_val_if_fail (cols, NULL);

  *rows = gst_value_array_get_size (gst_array);
  *cols = gst_value_array_get_size (gst_value_array_get_value (gst_array, 0));

  c_array = g_malloc (*rows * *cols * sizeof (*c_array));

  for (i = 0; i < *rows; i++) {
    row = gst_value_array_get_value (gst_array, i);
    for (j = 0; j < *cols; j++) {
      value = g_value_get_double (gst_value_array_get_value (row, j));
      c_array[i * *cols + j] = value;
    }
  }
  return c_array;
}

static gboolean
gst_vpi_undistort_set_calibration_matrix (GstVpiUndistort * self,
    const GValue * array, guint matrix_type)
{
  float *matrix = NULL;
  guint rows = 0;
  guint cols = 0;
  gboolean ret = TRUE;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (array, FALSE);

  matrix = gst_array_to_c_array (array, &rows, &cols);

  g_return_val_if_fail (matrix, FALSE);

  if (EXTRINSIC == matrix_type && ROWS_EXTRINSIC == rows
      && COLS_EXTRINSIC == cols) {
    memcpy (&self->extrinsic, matrix, rows * cols * sizeof (*matrix));

  } else if (INTRINSIC == matrix_type && ROWS_INTRINSIC == rows
      && COLS_INTRINSIC == cols) {
    memcpy (&self->intrinsic, matrix, rows * cols * sizeof (*matrix));

  } else {
    ret = FALSE;
    GST_WARNING_OBJECT (self, "Invalid %dx%d dimensions for %s matrix.", rows,
        cols, matrix_type == EXTRINSIC ? "extrinsic 3x4" : "intrinsic 2x3");
  }

  g_free (matrix);

  return ret;
}

static void
c_array_to_gst_array (GValue * gst_array, const float *c_array, guint rows,
    guint cols)
{
  GValue row = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (gst_array);
  g_return_if_fail (c_array);

  for (i = 0; i < rows; i++) {
    g_value_init (&row, GST_TYPE_ARRAY);

    for (j = 0; j < cols; j++) {
      g_value_init (&value, G_TYPE_DOUBLE);
      g_value_set_double (&value, c_array[i * cols + j]);
      gst_value_array_append_value (&row, &value);
      g_value_unset (&value);
    }

    gst_value_array_append_value (gst_array, &row);
    g_value_unset (&row);
  }
}

static void
gst_vpi_undistort_get_calibration_matrix (GstVpiUndistort * self,
    GValue * array, guint matrix_type, guint rows, guint cols)
{
  float *matrix = NULL;

  g_return_if_fail (self);
  g_return_if_fail (array);

  if (EXTRINSIC == matrix_type) {
    matrix = g_malloc (rows * cols * sizeof (*matrix));
    memcpy (matrix, &self->extrinsic, sizeof (self->extrinsic));
  } else {
    matrix = g_malloc (rows * cols * sizeof (*matrix));
    memcpy (matrix, &self->intrinsic, sizeof (self->intrinsic));
  }
  c_array_to_gst_array (array, matrix, rows, cols);

  g_free (matrix);
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
      gst_vpi_undistort_set_calibration_matrix (self, value, EXTRINSIC);
      break;
    case PROP_INTRINSIC_MATRIX:
      self->set_intrinsic_matrix =
          gst_vpi_undistort_set_calibration_matrix (self, value, INTRINSIC);
      break;
    case PROP_INTERPOLATOR:
      self->interpolator = g_value_get_enum (value);
      break;
    case PROP_DISTORTION_MODEL:
      self->distortion_model = g_value_get_enum (value);
      break;
    case PROP_FISHEYE_MAPPING:
      self->fisheye_mapping = g_value_get_enum (value);
      break;
    case PROP_K1:
    case PROP_K2:
    case PROP_K3:
    case PROP_K4:
    case PROP_K5:
    case PROP_K6:
    case PROP_P1:
    case PROP_P2:
    {
      gint idx = property_id - PROP_K1;
      self->coefficients[idx] = g_value_get_double (value);
      break;
    }
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
      gst_vpi_undistort_get_calibration_matrix (self, value, EXTRINSIC,
          ROWS_EXTRINSIC, COLS_EXTRINSIC);
      break;
    case PROP_INTRINSIC_MATRIX:
      gst_vpi_undistort_get_calibration_matrix (self, value, INTRINSIC,
          ROWS_INTRINSIC, COLS_INTRINSIC);
      break;
    case PROP_INTERPOLATOR:
      g_value_set_enum (value, self->interpolator);
      break;
    case PROP_DISTORTION_MODEL:
      g_value_set_enum (value, self->distortion_model);
      break;
    case PROP_FISHEYE_MAPPING:
      g_value_set_enum (value, self->fisheye_mapping);
      break;
    case PROP_K1:
    case PROP_K2:
    case PROP_K3:
    case PROP_K4:
    case PROP_K5:
    case PROP_K6:
    case PROP_P1:
    case PROP_P2:
    {
      gint idx = property_id - PROP_K1;
      self->coefficients[idx] = g_value_get_double (value);
      g_value_set_double (value, self->coefficients[idx]);
      break;
    }
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
