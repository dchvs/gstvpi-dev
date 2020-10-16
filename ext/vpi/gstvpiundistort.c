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

#include <gst/gst.h>

#include "gstvpiundistort.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_undistort_debug_category);
#define GST_CAT_DEFAULT gst_vpi_undistort_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", GST_VIDEO_FORMATS_ALL)

struct _GstVpiUndistort
{
  GstVpiFilter parent;
};

/* prototypes */
static GstFlowReturn gst_vpi_undistort_transform_image (GstVpiFilter * filter,
    VPIImage * in_image, VPIImage * out_image);
static void gst_vpi_undistort_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_undistort_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_undistort_finalize (GObject * object);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiUndistort, gst_vpi_undistort,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_undistort_debug_category, "vpiundistort",
        0, "debug category for vpiundistort element"));

static void
gst_vpi_undistort_class_init (GstVpiUndistortClass * klass)
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
      "VPI Undistort", "Filter/Video",
      "VPI based camera lens undistort element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_undistort_transform_image);
  gobject_class->set_property = gst_vpi_undistort_set_property;
  gobject_class->get_property = gst_vpi_undistort_get_property;
  gobject_class->finalize = gst_vpi_undistort_finalize;
}

static void
gst_vpi_undistort_init (GstVpiUndistort * vpi_undistort)
{
}

static GstFlowReturn
gst_vpi_undistort_transform_image (GstVpiFilter * filter,
    VPIImage * in_image, VPIImage * out_image)
{
  GstVpiUndistort *self = GST_VPI_UNDISTORT (filter);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_LOG_OBJECT (self, "Transform image");

  g_return_val_if_fail (in_image != NULL, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image != NULL, GST_FLOW_ERROR);

  /* TODO: Call to undistort function */

  return ret;
}

void
gst_vpi_undistort_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_undistort_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_undistort_finalize (GObject * object)
{
  GstVpiUndistort *vpi_undistort = GST_VPI_UNDISTORT (object);

  GST_DEBUG_OBJECT (vpi_undistort, "finalize");

  G_OBJECT_CLASS (gst_vpi_undistort_parent_class)->finalize (object);
}
