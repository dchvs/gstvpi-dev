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

#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)
#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VPIImage", GST_VIDEO_FORMATS_ALL)

struct _GstVpiDownload
{
  GstBaseTransform parent;
};

/* prototypes */
static void gst_vpi_download_finalize (GObject * object);
static GstCaps *gst_vpi_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_vpi_download_accept_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_vpi_download_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_vpi_download_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, GstBuffer ** outbuf);
static GstFlowReturn gst_vpi_download_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Download", "transition",
      "VPI converter element from VPIImage memory to host memory",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  gobject_class->finalize = gst_vpi_download_finalize;
  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_download_transform_caps);
  base_transform_class->accept_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_download_accept_caps);
  base_transform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_download_set_caps);
  base_transform_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_vpi_download_prepare_output_buffer);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_download_transform_ip);
}

static void
gst_vpi_download_init (GstVpiDownload * vpidownload)
{
}

void
gst_vpi_download_finalize (GObject * object)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (object);

  GST_DEBUG_OBJECT (vpidownload, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_vpi_download_parent_class)->finalize (object);
}

static GstCaps *
gst_vpi_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (trans);
  GstCaps *othercaps;

  GST_DEBUG_OBJECT (vpidownload, "transform_caps");

  othercaps = gst_caps_copy (caps);

  /* Copy other caps and modify as appropriate */
  /* This works for the simplest cases, where the transform modifies one
   * or more fields in the caps structure.  It does not work correctly
   * if passthrough caps are preferred. */
  if (direction == GST_PAD_SRC) {
    /* transform caps going upstream */
  } else {
    /* transform caps going downstream */
  }

  if (filter) {
    GstCaps *intersect;

    intersect = gst_caps_intersect (othercaps, filter);
    gst_caps_unref (othercaps);

    return intersect;
  } else {
    return othercaps;
  }
}

static gboolean
gst_vpi_download_accept_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (trans);

  GST_DEBUG_OBJECT (vpidownload, "accept_caps");

  return TRUE;
}

static gboolean
gst_vpi_download_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (trans);

  GST_DEBUG_OBJECT (vpidownload, "set_caps");

  return TRUE;
}

static GstFlowReturn
gst_vpi_download_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (trans);

  GST_DEBUG_OBJECT (vpidownload, "prepare_output_buffer");

  return GST_FLOW_OK;
}

/* transform */
static GstFlowReturn
gst_vpi_download_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstVpiDownload *vpidownload = GST_VPI_DOWNLOAD (trans);

  GST_DEBUG_OBJECT (vpidownload, "transform_ip");

  return GST_FLOW_OK;
}
