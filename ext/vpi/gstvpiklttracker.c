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

#include "gstvpiklttracker.h"

#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_klt_tracker_debug_category);
#define GST_CAT_DEFAULT gst_vpi_klt_tracker_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_BE, GRAY16_LE }")

struct _GstVpiKltTracker
{
  GstVpiFilter parent;
};

/* prototypes */
static GstFlowReturn gst_vpi_klt_tracker_transform_image (GstVpiFilter *
    filter, VPIStream stream, VPIImage in_image, VPIImage out_image);
static void gst_vpi_klt_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_finalize (GObject * object);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiKltTracker, gst_vpi_klt_tracker,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_klt_tracker_debug_category,
        "vpiklttracker", 0, "debug category for vpiklttracker element"));

static void
gst_vpi_klt_tracker_class_init (GstVpiKltTrackerClass * klass)
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
      "VPI KLT Tacker", "Filter/Video",
      "VPI based KLT feature tracker element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_transform_image);
  gobject_class->set_property = gst_vpi_klt_tracker_set_property;
  gobject_class->get_property = gst_vpi_klt_tracker_get_property;
  gobject_class->finalize = gst_vpi_klt_tracker_finalize;
}

static void
gst_vpi_klt_tracker_init (GstVpiKltTracker * self)
{
}

static GstFlowReturn
gst_vpi_klt_tracker_transform_image (GstVpiFilter * filter, VPIStream stream,
    VPIImage in_image, VPIImage out_image)
{
  GstVpiKltTracker *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_KLT_TRACKER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  return ret;
}


void
gst_vpi_klt_tracker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

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
gst_vpi_klt_tracker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

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

void
gst_vpi_klt_tracker_finalize (GObject * object)
{
  GstVpiKltTracker *vpi_klt_tracker = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (vpi_klt_tracker, "finalize");

  G_OBJECT_CLASS (gst_vpi_klt_tracker_parent_class)->finalize (object);
}
