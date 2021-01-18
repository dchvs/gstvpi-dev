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

#include "gstvpivideoconvert.h"

#include <gst/gst.h>
#include <vpi/algo/ConvertImageFormat.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_video_convert_debug_category);
#define GST_CAT_DEFAULT gst_vpi_video_convert_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE, NV12, RGB, BGR, RGBA, BGRA, RGBx, BGRx }")

#define VPI_CONVERSION_POLICY_ENUM (vpi_conversion_policy_enum_get_type ())
GType vpi_conversion_policy_enum_get_type (void);

GType
vpi_conversion_policy_enum_get_type (void)
{
  static GType vpi_conversion_policy_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_CONVERSION_CAST, "Casts input to the output type. Overflows "
          "and underflows are handled as per C specification, including "
          "situations of undefined behavior.", "cast"},
    {VPI_CONVERSION_CLAMP, "Clamps input to output's type range. Overflows "
          "and underflows are mapped to the output type's maximum and minimum "
          "representable value, respectively. When output type is floating point, "
          "clamp behaves like cast.", "clamp"},
    {0, NULL, NULL}
  };

  if (!vpi_conversion_policy_enum_type) {
    vpi_conversion_policy_enum_type =
        g_enum_register_static ("VpiConversionPolicy", values);
  }

  return vpi_conversion_policy_enum_type;
}

struct _GstVpiVideoConvert
{
  GstVpiFilter parent;

  gint conversion_policy;
  gfloat scale;
  gfloat offset;
};

/* prototypes */
static GstFlowReturn gst_vpi_video_convert_transform_image (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame);
static void gst_vpi_video_convert_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_video_convert_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static GstCaps *gst_vpi_video_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);

enum
{
  PROP_0,
  PROP_CONVERSION_POLICY,
  PROP_SCALE,
  PROP_OFFSET,
};

#define PROP_CONVERSION_POLICY_DEFAULT VPI_CONVERSION_CLAMP

#define PROP_SCALE_DEFAULT 1.0f
#define PROP_SCALE_MIN -G_MAXFLOAT
#define PROP_SCALE_MAX G_MAXFLOAT

#define PROP_OFFSET_DEFAULT 0.0f
#define PROP_OFFSET_MIN -G_MAXFLOAT
#define PROP_OFFSET_MAX G_MAXFLOAT

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiVideoConvert, gst_vpi_video_convert,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_video_convert_debug_category,
        "vpivideoconvert", 0, "debug category for vpivideoconvert element"));

static void
gst_vpi_video_convert_class_init (GstVpiVideoConvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *bt_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Video Convert", "Filter/Converter/Video",
      "Converts video from one colorspace to another using VPI",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_video_convert_transform_image);
  bt_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_video_convert_transform_caps);
  gobject_class->set_property = gst_vpi_video_convert_set_property;
  gobject_class->get_property = gst_vpi_video_convert_get_property;

  g_object_class_install_property (gobject_class, PROP_CONVERSION_POLICY,
      g_param_spec_enum ("conversion-policy", "Conversion Policy",
          "Policy used when converting between image types.",
          VPI_CONVERSION_POLICY_ENUM, PROP_CONVERSION_POLICY_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SCALE,
      g_param_spec_float ("scale", "Range Scale",
          "Factor on which to scale the conversion. Useful for type conversions.",
          PROP_SCALE_MIN, PROP_SCALE_MAX, PROP_SCALE_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_OFFSET,
      g_param_spec_float ("offset", "Range Offset",
          "Offset to add to the conversion. Useful for type conversions.",
          PROP_OFFSET_MIN, PROP_OFFSET_MAX, PROP_OFFSET_DEFAULT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  /* Disable any sort of processing if input/output caps are equal */
  bt_class->passthrough_on_same_caps = TRUE;
  bt_class->transform_ip_on_passthrough = FALSE;
}

static void
gst_vpi_video_convert_init (GstVpiVideoConvert * self)
{
  self->conversion_policy = PROP_CONVERSION_POLICY_DEFAULT;
  self->scale = PROP_SCALE_DEFAULT;
  self->offset = PROP_OFFSET_DEFAULT;
}

static GstFlowReturn
gst_vpi_video_convert_transform_image (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiVideoConvert *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  gint conversion_policy = -1;
  gfloat scale = 1;
  gfloat offset = 0;
  gint backend = VPI_BACKEND_INVALID;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_VIDEO_CONVERT (filter);

  GST_LOG_OBJECT (self, "Transform image");

  GST_OBJECT_LOCK (self);
  conversion_policy = self->conversion_policy;
  scale = self->scale;
  offset = self->offset;
  GST_OBJECT_UNLOCK (self);

  backend = gst_vpi_filter_get_backend (filter);

  status =
      vpiSubmitConvertImageFormat (stream, backend, in_frame->image,
      out_frame->image, conversion_policy, scale, offset);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Unable to perform format conversion."), ("%s",
            vpiStatusGetName (status)));
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

void
gst_vpi_video_convert_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiVideoConvert *self = GST_VPI_VIDEO_CONVERT (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_CONVERSION_POLICY:
      self->conversion_policy = g_value_get_enum (value);
      break;
    case PROP_SCALE:
      self->scale = g_value_get_float (value);
      break;
    case PROP_OFFSET:
      self->offset = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_video_convert_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiVideoConvert *self = GST_VPI_VIDEO_CONVERT (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_CONVERSION_POLICY:
      g_value_set_enum (value, self->conversion_policy);
      break;
    case PROP_SCALE:
      g_value_set_float (value, self->scale);
      break;
    case PROP_OFFSET:
      g_value_set_float (value, self->offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static GstCaps *
gst_vpi_video_convert_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *othercaps = NULL;
  gint i = 0;
  const gchar *dir = direction == GST_PAD_SRC ? "src" : "sink";
  const gchar *otherdir = direction == GST_PAD_SRC ? "sink" : "src";

  GST_DEBUG_OBJECT (trans,
      "Negotiating %s caps given the following %s caps: %" GST_PTR_FORMAT
      " and filter: %" GST_PTR_FORMAT, otherdir, dir, caps, filter);

  othercaps = gst_caps_copy (caps);

  for (i = 0; i < gst_caps_get_size (othercaps); ++i) {
    GstStructure *st = gst_caps_get_structure (othercaps, i);

    /* Remove the format field since its the only one allowed to
       change.
     */
    gst_structure_remove_field (st, "format");
  }

  if (filter) {
    GstCaps *tmp = othercaps;
    othercaps = gst_caps_intersect (othercaps, filter);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (trans, "Reduced %s caps to: %" GST_PTR_FORMAT, otherdir,
      othercaps);

  return othercaps;
}
