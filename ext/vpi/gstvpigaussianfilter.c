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

#include "gstvpigaussianfilter.h"

#include <math.h>

#include <gst/gst.h>

#include <vpi/algo/GaussianFilter.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_gaussian_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_gaussian_filter_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_BE, GRAY16_LE }")

#define DEFAULT_PROP_SIZE_MIN 0
#define DEFAULT_PROP_SIZE_MAX 11
#define DEFAULT_PROP_SIGMA_MIN 0.0
#define DEFAULT_PROP_SIGMA_MAX G_MAXDOUBLE

#define DEFAULT_PROP_SIZE 7
#define DEFAULT_PROP_SIGMA 1.7
#define DEFAULT_PROP_BOUNDARY_COND VPI_BOUNDARY_COND_ZERO

struct _GstVpiGaussianFilter
{
  GstVpiFilter parent;
  gint boundary_cond;
  gint size_x;
  gint size_y;
  gdouble sigma_x;
  gdouble sigma_y;
};

/* prototypes */
static gboolean gst_vpi_gaussian_filter_start (GstVpiFilter * filter,
    GstVideoInfo * in_info, GstVideoInfo * out_info);
static GstFlowReturn gst_vpi_gaussian_filter_transform_image (GstVpiFilter *
    filter, VPIStream stream, VPIImage in_image, VPIImage out_image);
static void gst_vpi_gaussian_filter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_gaussian_filter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_gaussian_filter_finalize (GObject * object);

enum
{
  PROP_0,
  PROP_SIZE_X,
  PROP_SIZE_Y,
  PROP_SIGMA_X,
  PROP_SIGMA_Y,
  PROP_BOUNDARY_COND
};

GType
vpi_boundary_cond_enum_get_type (void)
{
  static GType vpi_boundary_cond_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_BOUNDARY_COND_ZERO, "All pixels outside the image are considered 0.",
        "zero"},
    {VPI_BOUNDARY_COND_CLAMP, "Border pixels are repeated indefinitely.",
        "clamp"},
    {0, NULL, NULL}
  };

  if (!vpi_boundary_cond_enum_type) {
    vpi_boundary_cond_enum_type = g_enum_register_static ("VpiBoundCond",
        values);
  }

  return vpi_boundary_cond_enum_type;
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiGaussianFilter, gst_vpi_gaussian_filter,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_gaussian_filter_debug_category,
        "vpigaussianfilter", 0,
        "debug category for vpigaussianfilter element"));

static void
gst_vpi_gaussian_filter_class_init (GstVpiGaussianFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Gaussian Filter", "Filter/Video",
      "VPI Gaussian filter element for grayscale images.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_gaussian_filter_start);
  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_gaussian_filter_transform_image);
  gobject_class->set_property = gst_vpi_gaussian_filter_set_property;
  gobject_class->get_property = gst_vpi_gaussian_filter_get_property;
  gobject_class->finalize = gst_vpi_gaussian_filter_finalize;

  g_object_class_install_property (gobject_class, PROP_SIZE_X,
      g_param_spec_int ("size-x", "Kernel size X",
          "Gaussian kernel size in X direction. "
          "Must be between 0 and 11, and odd."
          "If it is 0, sigma-x will be used to compute its value.",
          DEFAULT_PROP_SIZE_MIN, DEFAULT_PROP_SIZE_MAX, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SIZE_Y,
      g_param_spec_int ("size-y", "Kernel size Y",
          "Gaussian kernel size in Y direction. "
          "Must be between 0 and 11, and odd."
          "If it is 0, sigma-y will be used to compute its value.",
          DEFAULT_PROP_SIZE_MIN, DEFAULT_PROP_SIZE_MAX, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SIGMA_X,
      g_param_spec_double ("sigma-x", "Standard deviation X",
          "Standard deviation of the Gaussian kernel in the X direction."
          "If it is 0, size-x will be used to compute its value.",
          DEFAULT_PROP_SIGMA_MIN, DEFAULT_PROP_SIGMA_MAX, DEFAULT_PROP_SIGMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SIGMA_Y,
      g_param_spec_double ("sigma-y", "Standard deviation Y",
          "Standard deviation of the Gaussian kernel in the Y direction."
          "If it is 0, size-y will be used to compute its value.",
          DEFAULT_PROP_SIGMA_MIN, DEFAULT_PROP_SIGMA_MAX, DEFAULT_PROP_SIGMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BOUNDARY_COND,
      g_param_spec_enum ("boundary", "Boundary condition",
          "How pixel values outside of the image domain should be treated.",
          VPI_BOUNDARY_CONDS_ENUM, DEFAULT_PROP_BOUNDARY_COND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_gaussian_filter_init (GstVpiGaussianFilter * self)
{
  self->boundary_cond = DEFAULT_PROP_BOUNDARY_COND;
  self->size_x = DEFAULT_PROP_SIZE;
  self->size_y = DEFAULT_PROP_SIZE;
  self->sigma_x = DEFAULT_PROP_SIGMA;
  self->sigma_y = DEFAULT_PROP_SIGMA;
}

static gint
gst_vpi_gaussian_filter_adjust_size (GstVpiGaussianFilter * self, gint size,
    gdouble sigma)
{
  gint ret = DEFAULT_PROP_SIZE;

  g_return_val_if_fail (self, ret);

  if (0 == size && 0 == sigma) {
    GST_WARNING_OBJECT (self, "Properties size and sigma cannot be both 0 in "
        "the same direction. Using default value %d for size.", ret);
  } else if (0 == size) {
    /* Using VPI Gaussian Filter formula: size = max{3, 2*ceil(3*sigma)-1} */
    ret = 2 * ceil (3 * sigma) - 1;
    ret = (3 < ret) ? ret : 3;
    GST_WARNING_OBJECT (self,
        "Property size is 0. Using sigma to calculate new value: %d.", ret);
  } else if (0 == size % 2) {
    GST_WARNING_OBJECT (self,
        "Property size must be odd. Using default value %d.", ret);
  } else {
    ret = size;
  }
  return ret;
}

static gdouble
gst_vpi_gaussian_filter_adjust_sigma (GstVpiGaussianFilter * self, gint size,
    gdouble sigma)
{
  gdouble ret = DEFAULT_PROP_SIGMA;

  g_return_val_if_fail (self, ret);

  if (0 == size && 0 == sigma) {
    GST_WARNING_OBJECT (self, "Properties size and sigma cannot be both 0 in "
        "the same direction. Using default value %f for sigma.", ret);
  } else if (0 == sigma) {
    /* Using OpenCV getGaussianKernel formula */
    ret = 0.3 * ((size - 1) * 0.5 - 1) + 0.8;
    GST_WARNING_OBJECT (self,
        "Property sigma is 0. Using size to calculate new value: %f.", ret);
  } else {
    ret = sigma;
  }
  return ret;
}

static gboolean
gst_vpi_gaussian_filter_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiGaussianFilter *self = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_GAUSSIAN_FILTER (filter);

  GST_DEBUG_OBJECT (self, "start");

  /* Adjust size and sigma in case they are invalid */
  self->size_x = gst_vpi_gaussian_filter_adjust_size (self, self->size_x,
      self->sigma_x);
  self->sigma_x = gst_vpi_gaussian_filter_adjust_sigma (self, self->size_x,
      self->sigma_x);
  self->size_y = gst_vpi_gaussian_filter_adjust_size (self, self->size_y,
      self->sigma_y);
  self->sigma_y = gst_vpi_gaussian_filter_adjust_sigma (self, self->size_y,
      self->sigma_y);

  GST_INFO_OBJECT (self, "\nProperties summary:\nsize-x=%d\nsigma-x=%f\nsize-y="
      "%d\nsigma-y=%f\nboundary=%d", self->size_x, self->sigma_x, self->size_y,
      self->sigma_y, self->boundary_cond);

  return ret;
}


static GstFlowReturn
gst_vpi_gaussian_filter_transform_image (GstVpiFilter * filter,
    VPIStream stream, VPIImage in_image, VPIImage out_image)
{
  GstVpiGaussianFilter *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  gint size_x = 0;
  gint size_y = 0;
  gint boundary_cond = 0;
  gdouble sigma_x = 0;
  gdouble sigma_y = 0;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_GAUSSIAN_FILTER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  GST_OBJECT_LOCK (self);
  size_x = self->size_x;
  size_y = self->size_y;
  sigma_x = self->sigma_x;
  sigma_y = self->sigma_y;
  boundary_cond = self->boundary_cond;
  GST_OBJECT_UNLOCK (self);

  status = vpiSubmitGaussianFilter (stream, VPI_BACKEND_CUDA, in_image,
      out_image, size_x, size_y, sigma_x, sigma_y, boundary_cond);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

void
gst_vpi_gaussian_filter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiGaussianFilter *self = GST_VPI_GAUSSIAN_FILTER (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_SIZE_X:
      self->size_x = g_value_get_int (value);
      break;
    case PROP_SIZE_Y:
      self->size_y = g_value_get_int (value);
      break;
    case PROP_SIGMA_X:
      self->sigma_x = g_value_get_double (value);
      break;
    case PROP_SIGMA_Y:
      self->sigma_y = g_value_get_double (value);
      break;
    case PROP_BOUNDARY_COND:
      self->boundary_cond = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_gaussian_filter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiGaussianFilter *self = GST_VPI_GAUSSIAN_FILTER (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_SIZE_X:
      g_value_set_int (value, self->size_x);
      break;
    case PROP_SIZE_Y:
      g_value_set_int (value, self->size_y);
      break;
    case PROP_SIGMA_X:
      g_value_set_double (value, self->sigma_x);
      break;
    case PROP_SIGMA_Y:
      g_value_set_double (value, self->sigma_y);
      break;
    case PROP_BOUNDARY_COND:
      g_value_set_enum (value, self->boundary_cond);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_gaussian_filter_finalize (GObject * object)
{
  GstVpiGaussianFilter *vpi_gaussian_filter = GST_VPI_GAUSSIAN_FILTER (object);

  GST_DEBUG_OBJECT (vpi_gaussian_filter, "finalize");

  G_OBJECT_CLASS (gst_vpi_gaussian_filter_parent_class)->finalize (object);
}
