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
#include <gst/video/video.h>

#include "gstvpidownload.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_download_debug_category);
#define GST_CAT_DEFAULT gst_vpi_download_debug_category

#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL)
#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", GST_VIDEO_FORMATS_ALL)

struct _GstVpiDownload
{
  GstBaseTransform parent;
};

/* prototypes */
static GstCaps *gst_vpi_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static GstCaps *gst_vpi_download_transform_downstream_caps (GstVpiDownload *
    self, GstCaps * caps_src);
static GstCaps *gst_vpi_download_transform_upstream_caps (GstVpiDownload *
    self, GstCaps * caps_src);

enum
{
  PROP_0
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiDownload, gst_vpi_download,
    GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_download_debug_category, "vpidownload", 0,
        "debug category for vpidownload element"));

static void
gst_vpi_download_class_init (GstVpiDownloadClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Download", "Filter/Video",
      "VPI converter element from VPIImage memory to host memory",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_download_transform_caps);
}

static void
gst_vpi_download_init (GstVpiDownload * self)
{
}

static GstCaps *
gst_vpi_download_transform_downstream_caps (GstVpiDownload * self,
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
gst_vpi_download_transform_upstream_caps (GstVpiDownload * self,
    GstCaps * caps_src)
{
  GstCaps *vpi_image = gst_caps_copy (caps_src);
  GstCapsFeatures *vpi_image_feature =
      gst_caps_features_from_string ("memory:VPIImage");
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  for (i = 0; i < gst_caps_get_size (vpi_image); i++) {

    /* Add VPIImage to all structures */
    gst_caps_set_features (vpi_image, i,
        gst_caps_features_copy (vpi_image_feature));
  }

  gst_caps_features_free (vpi_image_feature);

  return vpi_image;
}

static GstCaps *
gst_vpi_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *given_caps = gst_caps_copy (caps);
  GstCaps *result = NULL;

  GST_DEBUG_OBJECT (trans, "Transforming caps on %s:\ncaps: %"
      GST_PTR_FORMAT "\nfilter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  if (direction == GST_PAD_SRC) {
    /* transform caps going upstream */
    result =
        gst_vpi_download_transform_upstream_caps (GST_VPI_DOWNLOAD (trans),
        given_caps);
  } else if (direction == GST_PAD_SINK) {
    /* transform caps going downstream */
    result =
        gst_vpi_download_transform_downstream_caps (GST_VPI_DOWNLOAD (trans),
        given_caps);
  } else {
    /* unknown direction */
    GST_ERROR_OBJECT (trans,
        "Cannot transform caps of unknown GstPadDirection");
    goto out;
  }

  if (filter) {
    GstCaps *tmp = result;
    result = gst_caps_intersect (filter, result);
    gst_caps_unref (tmp);
  }

out:
  GST_DEBUG_OBJECT (trans, "Transformed caps: %" GST_PTR_FORMAT, result);

  return result;
}
