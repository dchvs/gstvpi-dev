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

GST_DEBUG_CATEGORY_STATIC (gst_vpi_upload_debug_category);
#define GST_CAT_DEFAULT gst_vpi_upload_debug_category

struct _GstVpiUpload
{
  GstBaseTransform parent;
};

/* prototypes */
static void gst_vpi_upload_finalize (GObject * object);

enum
{
  PROP_0
};

/* pad templates */
static GstStaticPadTemplate gst_vpi_upload_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );

static GstStaticPadTemplate gst_vpi_upload_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstVpiUpload, gst_vpi_upload, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_upload_debug_category, "vpiupload", 0,
        "debug category for vpiupload element"));

static void
gst_vpi_upload_class_init (GstVpiUploadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Upload", "transition", "VPI memory interface element",
      "Andres Campos <andres.campos@ridgerun.com>");

  gobject_class->finalize = gst_vpi_upload_finalize;
}

static void
gst_vpi_upload_init (GstVpiUpload * vpi_upload)
{
}

void
gst_vpi_upload_finalize (GObject * object)
{
  GstVpiUpload *vpi_upload = GST_VPI_UPLOAD (object);

  GST_DEBUG_OBJECT (vpi_upload, "finalize");

  G_OBJECT_CLASS (gst_vpi_upload_parent_class)->finalize (object);
}
