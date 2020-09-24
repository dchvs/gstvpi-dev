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

#include "gstvpifilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_filter_debug_category);
#define GST_CAT_DEFAULT gst_vpi_filter_debug_category

/* prototypes */

static void gst_vpi_filter_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_filter_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_filter_finalize (GObject * object);

enum
{
  PROP_0
};

/* pad templates */

#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, RGBA }")

#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ I420, RGBA }")

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiFilter, gst_vpi_filter, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_vpi_filter_debug_category, "vpifilter", 0,
  "debug category for vpifilter element"));

static void
gst_vpi_filter_class_init (GstVpiFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
        gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "VPI Filter", "filter", "VPI Filter",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  gobject_class->set_property = gst_vpi_filter_set_property;
  gobject_class->get_property = gst_vpi_filter_get_property;
  gobject_class->finalize = gst_vpi_filter_finalize;
}

static void
gst_vpi_filter_init (GstVpiFilter *vpifilter)
{
}

void
gst_vpi_filter_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiFilter *vpifilter = GST_VPI_FILTER (object);

  GST_DEBUG_OBJECT (vpifilter, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_filter_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiFilter *vpifilter = GST_VPI_FILTER (object);

  GST_DEBUG_OBJECT (vpifilter, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_vpi_filter_finalize (GObject * object)
{
  GstVpiFilter *vpifilter = GST_VPI_FILTER (object);

  GST_DEBUG_OBJECT (vpifilter, "finalize");

  G_OBJECT_CLASS (gst_vpi_filter_parent_class)->finalize (object);
}
