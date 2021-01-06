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

#include "gstvpioverlay.h"

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_overlay_debug_category);
#define GST_CAT_DEFAULT gst_vpi_overlay_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE }")

#define VPI_OVERLAY_COLORS_ENUM (vpi_overlay_colors_enum_get_type ())
GType vpi_overlay_colors_enum_get_type (void);

#define BLACK 0
#define WHITE 255

#define DEFAULT_PROP_BOXES_COLOR BLACK

struct _GstVpiOverlay
{
  GstVpiFilter parent;
  guint color;
};

/* prototypes */
static GstFlowReturn gst_vpi_overlay_transform_image_ip (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * frame);
static void gst_vpi_overlay_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_overlay_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
  PROP_BOXES_COLOR,
};

GType
vpi_overlay_colors_enum_get_type (void)
{
  static GType vpi_colors_enum_type = 0;
  static const GEnumValue values[] = {
    {BLACK, "Black color for boxes",
        "black"},
    {WHITE, "White color por boxes",
        "white"},
    {0, NULL, NULL}
  };

  if (!vpi_colors_enum_type) {
    vpi_colors_enum_type = g_enum_register_static ("VpiBoxesColors", values);
  }
  return vpi_colors_enum_type;
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiOverlay, gst_vpi_overlay,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_overlay_debug_category,
        "vpioverlay", 0, "debug category for vpioverlay element"));

static void
gst_vpi_overlay_class_init (GstVpiOverlayClass * klass)
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
      "VPI Algorithms Overlay", "Filter/Video",
      "VPI algorithms overlay for bounding boxes and keypoints.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_overlay_transform_image_ip);
  gobject_class->set_property = gst_vpi_overlay_set_property;
  gobject_class->get_property = gst_vpi_overlay_get_property;

  g_object_class_install_property (gobject_class, PROP_BOXES_COLOR,
      g_param_spec_enum ("color", "Boxes color",
          "Color to draw the boxes.",
          VPI_OVERLAY_COLORS_ENUM, DEFAULT_PROP_BOXES_COLOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_overlay_init (GstVpiOverlay * self)
{
  self->color = DEFAULT_PROP_BOXES_COLOR;
}

static GstFlowReturn
gst_vpi_overlay_transform_image_ip (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * frame)
{
  GstVpiOverlay *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->image, GST_FLOW_ERROR);

  self = GST_VPI_OVERLAY (filter);

  GST_LOG_OBJECT (self, "Transform image ip");

  return ret;
}

void
gst_vpi_overlay_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiOverlay *self = GST_VPI_OVERLAY (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOXES_COLOR:
      self->color = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_overlay_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiOverlay *self = GST_VPI_OVERLAY (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOXES_COLOR:
      g_value_set_enum (value, self->color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
