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

#include "gstvpiboxfilter.h"

#include <gst/gst.h>
#include <vpi/algo/BoxFilter.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_box_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_box_filter_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE }")

struct _GstVpiBoxFilter
{
  GstVpiFilter parent;
};

/* prototypes */
static GstFlowReturn gst_vpi_box_filter_transform_image (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame);
static void gst_vpi_box_filter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_box_filter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiBoxFilter, gst_vpi_box_filter,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_box_filter_debug_category,
        "vpiboxfilter", 0, "debug category for vpiboxfilter element"));

static void
gst_vpi_box_filter_class_init (GstVpiBoxFilterClass * klass)
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
      "VPI Box Filter", "Filter/Video",
      "VPI box filter element for grayscale images.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_box_filter_transform_image);
  gobject_class->set_property = gst_vpi_box_filter_set_property;
  gobject_class->get_property = gst_vpi_box_filter_get_property;
}

static void
gst_vpi_box_filter_init (GstVpiBoxFilter * self)
{
}

static GstFlowReturn
gst_vpi_box_filter_transform_image (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiBoxFilter *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Transform image");

  return ret;
}

void
gst_vpi_box_filter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiBoxFilter *self = GST_VPI_BOX_FILTER (object);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_box_filter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiBoxFilter *self = GST_VPI_BOX_FILTER (object);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
