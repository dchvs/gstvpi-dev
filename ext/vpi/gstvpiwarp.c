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

#include "gstvpiwarp.h"

#include <math.h>
#include <gst/gst.h>
#include <vpi/algo/PerspectiveWarp.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_warp_debug_category);
#define GST_CAT_DEFAULT gst_vpi_warp_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE, RGB, BGR, RGBx, BGRx, NV12 }")

#define VPI_WARP_FLAGS (vpi_warp_flags_get_type ())
GType vpi_warp_flags_get_type (void);

#define VPI_WARP_DIRECT 0
#define NUM_ROWS_COLS 3

#define DEFAULT_PROP_INTERPOLATOR VPI_INTERP_LINEAR
#define DEFAULT_PROP_WARP_FLAG VPI_WARP_DIRECT
#define DEFAULT_PROP_TRANSFORM { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} }
#define DEFAULT_PROP_DEMO FALSE

struct _GstVpiWarp
{
  GstVpiFilter parent;
  VPIPayload warp;
  VPIPerspectiveTransform transform;
  gint interpolator;
  guint warp_flag;
  gboolean demo;
  gint width;
  gint height;
  gint demo_angle;
};

/* prototypes */
static GstFlowReturn gst_vpi_warp_transform_image (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame);
static gboolean gst_vpi_warp_start (GstVpiFilter * self, GstVideoInfo *
    in_info, GstVideoInfo * out_info);
static gboolean gst_vpi_warp_stop (GstBaseTransform * trans);
static void gst_vpi_warp_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_warp_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_INTERPOLATOR,
  PROP_WARP_FLAG,
  PROP_TRANSFORM,
  PROP_DEMO
};

GType
vpi_warp_flags_get_type (void)
{
  static GType vpi_warp_flags_type = 0;
  static const GFlagsValue values[] = {
    {VPI_WARP_DIRECT, "The provided transformation matrix is direct, so the "
          "algorithm will invert it internally.",
        "direct"},
    {VPI_WARP_INVERSE,
          "The provided transformation matrix is already inverted, so the "
          "algorithm will not invert it.",
        "inverted"},
    {0, NULL, NULL}
  };

  if (!vpi_warp_flags_type) {
    vpi_warp_flags_type = g_flags_register_static ("VpiWarpFlags", values);
  }

  return vpi_warp_flags_type;
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiWarp, gst_vpi_warp,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_warp_debug_category, "vpiwarp",
        0, "debug category for vpiwarp element"));

static void
gst_vpi_warp_class_init (GstVpiWarpClass * klass)
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
      "VPI Warp", "Filter/Video",
      "VPI based perspective warp converter element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_warp_transform_image);
  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_warp_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_warp_stop);
  gobject_class->set_property = gst_vpi_warp_set_property;
  gobject_class->get_property = gst_vpi_warp_get_property;

  g_object_class_install_property (gobject_class, PROP_INTERPOLATOR,
      g_param_spec_enum ("interpolator", "Interpolation method",
          "Interpolation method to be used.",
          VPI_INTERPOLATORS_ENUM, DEFAULT_PROP_INTERPOLATOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WARP_FLAG,
      g_param_spec_flags ("behavior", "Algorithm behavior flag",
          "Flag to modify algorithm behavior.",
          VPI_WARP_FLAGS, DEFAULT_PROP_WARP_FLAG,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TRANSFORM,
      gst_param_spec_array ("transformation",
          "Transformation to be applied",
          "3x3 transformation matrix.\nIf not provided, no transformation "
          "will be performed\n"
          "Usage example: <<1.0,0.0,0.0>,<0.0,1.0,0.0>,<0.0,0.0,1.0>>",
          gst_param_spec_array ("matrix-rows", "rows", "rows",
              g_param_spec_double ("matrix-cols", "cols", "cols",
                  -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DEMO,
      g_param_spec_boolean ("demo", "Demo Mode",
          "Put the element in demo mode to showcase it's capabilities. In this "
          "mode the incoming image will be rotated in the 3D space and projected "
          "back to the image plane. Will override the transformation matrix.",
          DEFAULT_PROP_DEMO,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_warp_init (GstVpiWarp * self)
{
  VPIPerspectiveTransform transform = DEFAULT_PROP_TRANSFORM;

  self->warp = NULL;
  self->interpolator = DEFAULT_PROP_INTERPOLATOR;
  self->warp_flag = DEFAULT_PROP_WARP_FLAG;
  self->demo = DEFAULT_PROP_DEMO;
  self->width = 0;
  self->height = 0;
  self->demo_angle = 0;

  memcpy (&self->transform, &transform, sizeof (transform));
}

static gboolean
gst_vpi_warp_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiWarp *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  gint backend = VPI_BACKEND_INVALID;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_WARP (filter);

  GST_DEBUG_OBJECT (self, "start");

  self->width = GST_VIDEO_INFO_WIDTH (in_info);
  self->height = GST_VIDEO_INFO_HEIGHT (in_info);

  backend = gst_vpi_filter_get_backend (filter);

  status = vpiCreatePerspectiveWarp (backend, &self->warp);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create perspective warp payload"),
        ("%s", vpiStatusGetName (status)));
    ret = FALSE;
  }
  return ret;
}

static void
gst_vpi_transformation_matrix_multiply (gfloat
    result[NUM_ROWS_COLS][NUM_ROWS_COLS], VPIPerspectiveTransform a,
    VPIPerspectiveTransform b)
{
  guint i, j, k = 0;

  g_return_if_fail (result);
  g_return_if_fail (a);
  g_return_if_fail (b);

  for (i = 0; i < NUM_ROWS_COLS; i++) {
    for (j = 0; j < NUM_ROWS_COLS; j++) {
      result[i][j] = 0;
      for (k = 0; k < NUM_ROWS_COLS; k++) {
        result[i][j] += a[i][k] * b[k][j];
      }
    }
  }
}

static gdouble
degrees_to_radians (gdouble degrees)
{
  return degrees * M_PI / 180;
}

static void
gst_vpi_warp_demo (GstVpiWarp * self, gdouble angle, gint width, gint height)
{
  /* Transformation to move image center to origin of coordinate system */
  VPIPerspectiveTransform t_to_origin =
      { {1, 0, -width / 2.0f}, {0, 1, -height / 2.0f}, {0, 0, 1} };
  /* Time dependent transformation taken from VPI sample */
  gdouble t1 = sin (angle) * 0.0005f;
  gdouble t2 = cos (angle) * 0.0005f;
  VPIPerspectiveTransform t_time = { {0.66, 0, 0}, {0, 0.66, 0}, {t1, t2, 1} };
  /* Transformation to move image back to where it was */
  VPIPerspectiveTransform t_back =
      { {1, 0, width / 2.0f}, {0, 1, height / 2.0f}, {0, 0, 1} };
  /* Temporal result */
  VPIPerspectiveTransform tmp = { 0 };

  g_return_if_fail (self);

  /* Apply transformations */
  gst_vpi_transformation_matrix_multiply (tmp, t_time, t_to_origin);

  GST_OBJECT_LOCK (self);
  gst_vpi_transformation_matrix_multiply (self->transform, t_back, tmp);
  GST_OBJECT_UNLOCK (self);

  self->demo_angle = (self->demo_angle + 1) % 360;
}

static GstFlowReturn
gst_vpi_warp_transform_image (GstVpiFilter * filter, VPIStream stream,
    VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiWarp *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  gboolean demo = DEFAULT_PROP_DEMO;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_WARP (filter);

  GST_LOG_OBJECT (self, "Transform image");

  GST_OBJECT_LOCK (self);
  demo = self->demo;
  GST_OBJECT_UNLOCK (self);

  if (demo) {
    gdouble angle = degrees_to_radians (self->demo_angle);
    gst_vpi_warp_demo (self, angle, self->width, self->height);
  }

  GST_OBJECT_LOCK (self);
  status =
      vpiSubmitPerspectiveWarp (stream, self->warp, in_frame->image,
      self->transform, out_frame->image, self->interpolator,
      VPI_BOUNDARY_COND_ZERO, self->warp_flag);
  GST_OBJECT_UNLOCK (self);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

static void
gst_vpi_warp_set_transformation_matrix (GstVpiWarp * self, const GValue * array)
{
  const GValue *row = NULL;
  float value = 0;
  guint rows, cols = 0;
  guint i, j = 0;

  g_return_if_fail (self);
  g_return_if_fail (array);

  rows = gst_value_array_get_size (array);
  cols = gst_value_array_get_size (gst_value_array_get_value (array, 0));

  if (NUM_ROWS_COLS == rows && NUM_ROWS_COLS == cols) {

    for (i = 0; i < rows; i++) {
      row = gst_value_array_get_value (array, i);
      for (j = 0; j < cols; j++) {
        value = g_value_get_double (gst_value_array_get_value (row, j));
        self->transform[i][j] = value;
      }
    }
  } else {
    GST_WARNING_OBJECT (self, "Matrix must be of 3x3. Received %dx%d. Not "
        "changing transformation matrix", rows, cols);
  }
}

void
gst_vpi_warp_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiWarp *self = GST_VPI_WARP (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_INTERPOLATOR:
      self->interpolator = g_value_get_enum (value);
      break;
    case PROP_WARP_FLAG:
      self->warp_flag = g_value_get_flags (value);
      break;
    case PROP_TRANSFORM:
      gst_vpi_warp_set_transformation_matrix (self, value);
      break;
    case PROP_DEMO:
      self->demo = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static void
gst_vpi_warp_get_transformation_matrix (GstVpiWarp * self, GValue * array)
{
  GValue row = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (self);
  g_return_if_fail (array);

  for (i = 0; i < NUM_ROWS_COLS; i++) {
    g_value_init (&row, GST_TYPE_ARRAY);

    for (j = 0; j < NUM_ROWS_COLS; j++) {
      g_value_init (&value, G_TYPE_DOUBLE);
      g_value_set_double (&value, self->transform[i][j]);
      gst_value_array_append_value (&row, &value);
      g_value_unset (&value);
    }

    gst_value_array_append_value (array, &row);
    g_value_unset (&row);
  }
}

void
gst_vpi_warp_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiWarp *self = GST_VPI_WARP (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_INTERPOLATOR:
      g_value_set_enum (value, self->interpolator);
      break;
    case PROP_WARP_FLAG:
      g_value_set_flags (value, self->warp_flag);
      break;
    case PROP_TRANSFORM:
      gst_vpi_warp_get_transformation_matrix (self, value);
      break;
    case PROP_DEMO:
      g_value_set_boolean (value, self->demo);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vpi_warp_stop (GstBaseTransform * trans)
{
  GstVpiWarp *self = GST_VPI_WARP (trans);
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_warp_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");

  vpiPayloadDestroy (self->warp);
  self->warp = NULL;

  return ret;
}
