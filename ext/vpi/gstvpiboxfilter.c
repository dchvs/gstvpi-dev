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

#define VPI_KERNEL_SIZE_ENUM (vpi_kernel_size_enum_get_type ())
GType vpi_kernel_size_enum_get_type (void);

#define KERNEL_SIZE_3 3
#define KERNEL_SIZE_5 5
#define KERNEL_SIZE_7 7
#define KERNEL_SIZE_9 9
#define KERNEL_SIZE_11 11

#define DEFAULT_PROP_SIZE KERNEL_SIZE_5

struct _GstVpiBoxFilter
{
  GstVpiFilter parent;
  guint size_x;
  guint size_y;
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
  PROP_SIZE_X,
  PROP_SIZE_Y
};

GType
vpi_kernel_size_enum_get_type (void)
{
  static GType vpi_kernel_size_enum_type = 0;
  static const GEnumValue values[] = {
    {KERNEL_SIZE_3, "Kernel size of 3",
        "3"},
    {KERNEL_SIZE_5, "Kernel size of 5",
        "5"},
    {KERNEL_SIZE_7, "Kernel size of 7",
        "7"},
    {KERNEL_SIZE_9, "Kernel size of 9",
        "9"},
    {KERNEL_SIZE_11, "Kernel size of 11",
        "11"},
    {0, NULL, NULL}
  };

  if (!vpi_kernel_size_enum_type) {
    vpi_kernel_size_enum_type =
        g_enum_register_static ("VpiKernelSize", values);
  }
  return vpi_kernel_size_enum_type;
}

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

  g_object_class_install_property (gobject_class, PROP_SIZE_X,
      g_param_spec_enum ("size-x", "Kernel size X",
          "Box kernel size in X direction. ",
          VPI_KERNEL_SIZE_ENUM, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SIZE_Y,
      g_param_spec_enum ("size-y", "Kernel size Y",
          "Box kernel size in Y direction. ",
          VPI_KERNEL_SIZE_ENUM, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_box_filter_init (GstVpiBoxFilter * self)
{
  self->size_x = DEFAULT_PROP_SIZE;
  self->size_y = DEFAULT_PROP_SIZE;
}

static GstFlowReturn
gst_vpi_box_filter_transform_image (GstVpiFilter * filter,
    VPIStream stream, VpiFrame * in_frame, VpiFrame * out_frame)
{
  GstVpiBoxFilter *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIStatus status = VPI_SUCCESS;
  guint size_x, size_y = DEFAULT_PROP_SIZE;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (in_frame->image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame, GST_FLOW_ERROR);
  g_return_val_if_fail (out_frame->image, GST_FLOW_ERROR);

  self = GST_VPI_BOX_FILTER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  GST_OBJECT_LOCK (self);
  size_x = self->size_x;
  size_y = self->size_y;
  GST_OBJECT_UNLOCK (self);

  status = vpiSubmitBoxFilter (stream, VPI_BACKEND_CUDA, in_frame->image,
      out_frame->image, size_x, size_y, VPI_BOUNDARY_COND_ZERO);

  if (VPI_SUCCESS != status) {
    ret = GST_FLOW_ERROR;
  }

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
    case PROP_SIZE_X:
      self->size_x = g_value_get_enum (value);
      break;
    case PROP_SIZE_Y:
      self->size_y = g_value_get_enum (value);
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
    case PROP_SIZE_X:
      g_value_set_enum (value, self->size_x);
      break;
    case PROP_SIZE_Y:
      g_value_set_enum (value, self->size_y);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}
