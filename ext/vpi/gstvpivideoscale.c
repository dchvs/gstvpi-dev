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

#include "gstvpivideoscale.h"

#include <gst/gst.h>
#include <vpi/algo/Rescale.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_video_scale_debug_category);
#define GST_CAT_DEFAULT gst_vpi_video_scale_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE, NV12, RGB, BGR, RGBA, BGRA, RGBx, BGRx }")

#define DEFAULT_PROP_INTERPOLATOR VPI_INTERP_LINEAR
#define DEFAULT_PROP_BOUNDARY_COND VPI_BOUNDARY_COND_ZERO

struct _GstVpiVideoScale
{
  GstVpiFilter parent;

  gint interpolator;
  gint boundary_cond;
};

/* prototypes */
static GstFlowReturn gst_vpi_video_scale_transform_image (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame);
static void gst_vpi_video_scale_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_video_scale_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_INTERPOLATOR,
  PROP_BOUNDARY_COND
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiVideoScale, gst_vpi_video_scale,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_video_scale_debug_category,
        "vpivideoscale", 0, "debug category for vpivideoscale element"));

static void
gst_vpi_video_scale_class_init (GstVpiVideoScaleClass * klass)
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
      "VPI Video Scale", "Filter/Converter/Video",
      "Rescales video from one resolution to another using VPI",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_video_scale_transform_image);
  gobject_class->set_property = gst_vpi_video_scale_set_property;
  gobject_class->get_property = gst_vpi_video_scale_get_property;

  g_object_class_install_property (gobject_class, PROP_INTERPOLATOR,
      g_param_spec_enum ("interpolator", "Interpolation method",
          "Interpolation method to be used.",
          VPI_INTERPOLATORS_ENUM, DEFAULT_PROP_INTERPOLATOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BOUNDARY_COND,
      g_param_spec_enum ("boundary", "Boundary condition",
          "How pixel values outside of the image domain should be treated.",
          VPI_BOUNDARY_CONDS_ENUM, DEFAULT_PROP_BOUNDARY_COND,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_video_scale_init (GstVpiVideoScale * self)
{
  self->interpolator = DEFAULT_PROP_INTERPOLATOR;
  self->boundary_cond = DEFAULT_PROP_BOUNDARY_COND;
}

static GstFlowReturn
gst_vpi_video_scale_transform_image (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiVideoScale *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  gint backend = VPI_BACKEND_INVALID;
  guint interpolator = DEFAULT_PROP_INTERPOLATOR;
  guint boundary_cond = DEFAULT_PROP_BOUNDARY_COND;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_VIDEO_SCALE (filter);

  GST_LOG_OBJECT (self, "Transform image");

  backend = gst_vpi_filter_get_backend (filter);

  GST_OBJECT_LOCK (self);
  interpolator = self->interpolator;
  boundary_cond = self->boundary_cond;
  GST_OBJECT_UNLOCK (self);

  status =
      vpiSubmitRescale (stream, backend, in_frame->image, out_frame->image,
      interpolator, boundary_cond);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to perform rescale."), ("%s", vpiStatusGetName (status)));
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

void
gst_vpi_video_scale_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiVideoScale *self = GST_VPI_VIDEO_SCALE (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_INTERPOLATOR:
      self->interpolator = g_value_get_enum (value);
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
gst_vpi_video_scale_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiVideoScale *self = GST_VPI_VIDEO_SCALE (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_INTERPOLATOR:
      g_value_set_enum (value, self->interpolator);
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
