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

#include <gst/gst.h>
#include <vpi/algo/PerspectiveWarp.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_warp_debug_category);
#define GST_CAT_DEFAULT gst_vpi_warp_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE, RGB, BGR, RGBx, BGRx, NV12 }")

#define VPI_WARP_FLAGS_ENUM (vpi_warp_flags_enum_get_type ())
GType vpi_warp_flags_enum_get_type (void);

#define VPI_WARP_NOT_INVERTED 0

#define DEFAULT_PROP_INTERPOLATOR VPI_INTERP_LINEAR
#define DEFAULT_PROP_WARP_FLAG VPI_WARP_NOT_INVERTED

struct _GstVpiWarp
{
  GstVpiFilter parent;
  VPIPayload warp;
  gint interpolator;
  gint warp_flag;
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
  PROP_WARP_FLAG
};

GType
vpi_warp_flags_enum_get_type (void)
{
  static GType vpi_warp_flags_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_WARP_NOT_INVERTED, "Transform is not inverted, algorithm inverts it",
        "not-inverted"},
    {VPI_WARP_INVERSE,
          "Transform is already inverted, algorithm can use it directly",
        "inverted"},
    {0, NULL, NULL}
  };

  if (!vpi_warp_flags_enum_type) {
    vpi_warp_flags_enum_type = g_enum_register_static ("VpiWarpFlags", values);
  }

  return vpi_warp_flags_enum_type;
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
      g_param_spec_enum ("behavior-flag", "Algorithm behavior flag",
          "Flag to modify algorithm behavior.",
          VPI_WARP_FLAGS_ENUM, DEFAULT_PROP_WARP_FLAG,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_warp_init (GstVpiWarp * self)
{
  self->warp = NULL;
  self->interpolator = DEFAULT_PROP_INTERPOLATOR;
  self->warp_flag = DEFAULT_PROP_WARP_FLAG;
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

static GstFlowReturn
gst_vpi_warp_transform_image (GstVpiFilter * filter, VPIStream stream,
    VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiWarp *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  gint interpolator = DEFAULT_PROP_INTERPOLATOR;
  gint warp_flag = DEFAULT_PROP_WARP_FLAG;
  VPIPerspectiveTransform transform = { {0.5386, 0.1419, -74},
  {-0.4399, 0.8662, 291.5},
  {-0.0005, 0.0003, 1}
  };

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_WARP (filter);

  GST_LOG_OBJECT (self, "Transform image");

  GST_OBJECT_LOCK (self);
  interpolator = self->interpolator;
  warp_flag = self->warp_flag;
  GST_OBJECT_UNLOCK (self);

  status =
      vpiSubmitPerspectiveWarp (stream, self->warp, in_frame->image, transform,
      out_frame->image, interpolator, VPI_BOUNDARY_COND_ZERO, warp_flag);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

  return ret;
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
      self->warp_flag = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
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
      g_value_set_enum (value, self->warp_flag);
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
