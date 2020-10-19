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

#include <gst/gst.h>

#include <vpi/algo/GaussianImageFilter.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_gaussian_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_gaussian_filter_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_BE, GRAY16_LE }")

#define DEFAULT_PROP_SIZE_MIN 3
#define DEFAULT_PROP_SIZE_MAX 11

#define DEFAULT_PROP_SIZE 7

struct _GstVpiGaussianFilter
{
  GstVpiFilter parent;
  gint size_x;
  gint size_y;
};

/* prototypes */
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
  PROP_SIZE_Y
};

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

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_gaussian_filter_transform_image);
  gobject_class->set_property = gst_vpi_gaussian_filter_set_property;
  gobject_class->get_property = gst_vpi_gaussian_filter_get_property;
  gobject_class->finalize = gst_vpi_gaussian_filter_finalize;

  g_object_class_install_property (gobject_class, PROP_SIZE_X,
      g_param_spec_int ("size-x", "Kernel size X",
          "Gaussian kernel size in X direction. "
          "Must be between 3 and 11, and odd",
          DEFAULT_PROP_SIZE_MIN, DEFAULT_PROP_SIZE_MAX, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SIZE_Y,
      g_param_spec_int ("size-y", "Kernel size Y",
          "Gaussian kernel size in Y direction. "
          "Must be between 3 and 11, and odd",
          DEFAULT_PROP_SIZE_MIN, DEFAULT_PROP_SIZE_MAX, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_gaussian_filter_init (GstVpiGaussianFilter * self)
{
  self->size_x = DEFAULT_PROP_SIZE;
  self->size_y = DEFAULT_PROP_SIZE;
}

static GstFlowReturn
gst_vpi_gaussian_filter_transform_image (GstVpiFilter * filter,
    VPIStream stream, VPIImage in_image, VPIImage out_image)
{
  GstVpiGaussianFilter *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  const gdouble sigma = 1.7;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_GAUSSIAN_FILTER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  status = vpiSubmitGaussianImageFilter (stream, in_image, out_image,
      self->size_x, self->size_y, sigma, sigma, VPI_BOUNDARY_COND_ZERO);

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

  switch (property_id) {
    case PROP_SIZE_X:
      self->size_x = g_value_get_int (value);
      break;
    case PROP_SIZE_Y:
      self->size_y = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_gaussian_filter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiGaussianFilter *self = GST_VPI_GAUSSIAN_FILTER (object);

  GST_DEBUG_OBJECT (self, "get_property");

  switch (property_id) {
    case PROP_SIZE_X:
      g_value_set_int (value, self->size_x);
      break;
    case PROP_SIZE_Y:
      g_value_set_int (value, self->size_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_gaussian_filter_finalize (GObject * object)
{
  GstVpiGaussianFilter *vpi_gaussian_filter = GST_VPI_GAUSSIAN_FILTER (object);

  GST_DEBUG_OBJECT (vpi_gaussian_filter, "finalize");

  G_OBJECT_CLASS (gst_vpi_gaussian_filter_parent_class)->finalize (object);
}
