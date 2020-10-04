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

#include "gstvpiupload.h"

#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_upload_debug_category);
#define GST_CAT_DEFAULT gst_vpi_upload_debug_category

#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)
#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VPIImage", GST_VIDEO_FORMATS_ALL)

struct _GstVpiUpload
{
  GstBaseTransform parent;
  GstVideoInfo out_caps_info;
  GstVideoInfo in_caps_info;
};

/* prototypes */
static GstCaps *gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_vpi_upload_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);

enum
{
  PROP_0
};

/* pad templates */
static GstStaticPadTemplate
    gst_vpi_upload_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_AND_VPIIMAGE_CAPS)
    );

static GstStaticPadTemplate
    gst_vpi_upload_sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS ";" VIDEO_AND_VPIIMAGE_CAPS)
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstVpiUpload, gst_vpi_upload, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_upload_debug_category, "vpiupload", 0,
        "debug category for vpiupload element"));

static void
gst_vpi_upload_class_init (GstVpiUploadClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Upload", "Filter/Video", "Uploads data into NVIDIA VPI",
      "Andres Campos <andres.campos@ridgerun.com>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_transform_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_vpi_upload_set_caps);
}

static void
gst_vpi_upload_init (GstVpiUpload * self)
{
}

static GstCaps *
gst_vpi_download_transform_downstream_caps (GstVpiUpload * self,
    GstCaps * caps_src)
{
  GstCaps *vpiimage = gst_caps_copy (caps_src);
  GstCapsFeatures *vpiimage_feature =
      gst_caps_features_from_string ("memory:VPIImage");
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  for (i = 0; i < gst_caps_get_size (vpiimage); ++i) {

    /* Add VPIImage to all structures */
    gst_caps_set_features (vpiimage, i,
        gst_caps_features_copy (vpiimage_feature));
  }

  gst_caps_features_free (vpiimage_feature);

  return vpiimage;
}

static GstCaps *
gst_vpi_download_transform_upstream_caps (GstVpiUpload * self,
    GstCaps * caps_src)
{
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  /* All the result caps are Linux */
  for (i = 0; i < gst_caps_get_size (caps_src); i++) {
    gst_caps_set_features (caps_src, i, NULL);
  }

  return caps_src;
}

static GstCaps *
gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  GstCaps *given_caps = NULL;
  GstCaps *result = 0;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  given_caps = gst_caps_copy (caps);

  if (direction == GST_PAD_SRC) {
    /* transform caps going upstream */
    result = gst_vpi_download_transform_upstream_caps (self, given_caps);
  } else {
    /* transform caps going downstream */
    result = gst_vpi_download_transform_downstream_caps (self, given_caps);
  }

  if (filter) {
    GstCaps *tmp = result;
    result = gst_caps_intersect (filter, result);
    gst_caps_unref (tmp);
  }

  GST_DEBUG_OBJECT (self, "Transformed caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_vpi_upload_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  gboolean status = FALSE;

  GST_INFO_OBJECT (self, "set_caps");

  status = gst_video_info_from_caps (&self->in_caps_info, incaps);
  if (!status) {
    GST_ERROR ("Unable to get the input caps");
    goto out;
  }

  status = gst_video_info_from_caps (&self->out_caps_info, outcaps);
  if (!status) {
    GST_ERROR ("Unable to get the output caps");
    goto out;
  }

  status = TRUE;

out:
  return status;
}
