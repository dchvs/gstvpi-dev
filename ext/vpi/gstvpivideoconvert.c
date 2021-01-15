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

#include "gstvpivideoconvert.h"

#include <gst/gst.h>
#include <vpi/algo/ConvertImageFormat.h>

#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_video_convert_debug_category);
#define GST_CAT_DEFAULT gst_vpi_video_convert_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE, NV12, RGB, BGR, RGBA, BGRA, RGBx, BGRx }")

struct _GstVpiVideoConvert
{
  GstVpiFilter parent;
};

/* prototypes */
static gboolean gst_vpi_video_convert_start (GstVpiFilter * filter,
    GstVideoInfo * in_info, GstVideoInfo * out_info);
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
};

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

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_video_convert_start);
  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_video_convert_transform_image);
  bt_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_video_convert_transform_caps);
  gobject_class->set_property = gst_vpi_video_convert_set_property;
  gobject_class->get_property = gst_vpi_video_convert_get_property;

  /* Disable any sort of processing if input/output caps are equal */
  bt_class->passthrough_on_same_caps = TRUE;
  bt_class->transform_ip_on_passthrough = FALSE;
}

static void
gst_vpi_video_convert_init (GstVpiVideoConvert * self)
{

}

static gboolean
gst_vpi_video_convert_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiVideoConvert *self = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_VIDEO_CONVERT (filter);

  GST_DEBUG_OBJECT (self, "start");

  return ret;
}


static GstFlowReturn
gst_vpi_video_convert_transform_image (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiVideoConvert *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_VIDEO_CONVERT (filter);

  GST_LOG_OBJECT (self, "Transform image");

  status =
      vpiSubmitConvertImageFormat (stream, VPI_BACKEND_CUDA, in_frame->image,
      out_frame->image, VPI_CONVERSION_CLAMP, 1, 0);

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
