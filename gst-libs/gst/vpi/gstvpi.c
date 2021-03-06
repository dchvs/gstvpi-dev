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

#include "gstvpi.h"

VPIImageFormat
gst_vpi_video_to_image_format (GstVideoFormat video_format)
{
  VPIImageFormat ret;

  switch (video_format) {
    case GST_VIDEO_FORMAT_GRAY8:{
      ret = VPI_IMAGE_FORMAT_U8;
      break;
    }
    case GST_VIDEO_FORMAT_GRAY16_BE:{
      ret = VPI_IMAGE_FORMAT_U16;
      break;
    }
    case GST_VIDEO_FORMAT_GRAY16_LE:{
      ret = VPI_IMAGE_FORMAT_U16;
      break;
    }
    case GST_VIDEO_FORMAT_NV12:{
      ret = VPI_IMAGE_FORMAT_NV12;
      break;
    }
    case GST_VIDEO_FORMAT_RGB:{
      ret = VPI_IMAGE_FORMAT_RGB8;
      break;
    }
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_RGBA:{
      ret = VPI_IMAGE_FORMAT_RGBA8;
      break;
    }
    case GST_VIDEO_FORMAT_BGR:{
      ret = VPI_IMAGE_FORMAT_BGR8;
      break;
    }
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:{
      ret = VPI_IMAGE_FORMAT_BGRA8;
      break;
    }
    default:{
      ret = VPI_IMAGE_FORMAT_INVALID;
    }
  }

  return ret;
}

GstVideoFormat
gst_vpi_image_to_video_format (VPIImageFormat image_format)
{
  GstVideoFormat ret;

  switch (image_format) {
    case VPI_IMAGE_FORMAT_U8:{
      ret = GST_VIDEO_FORMAT_GRAY8;
      break;
    }
    case VPI_IMAGE_FORMAT_U16:{
      ret = GST_VIDEO_FORMAT_GRAY16_BE;
      break;
    }
    case VPI_IMAGE_FORMAT_NV12:{
      ret = GST_VIDEO_FORMAT_NV12;
      break;
    }
    case VPI_IMAGE_FORMAT_RGB8:{
      ret = GST_VIDEO_FORMAT_RGB;
      break;
    }
    case VPI_IMAGE_FORMAT_RGBA8:{
      ret = GST_VIDEO_FORMAT_RGBA;
      break;
    }
    case VPI_IMAGE_FORMAT_BGR8:{
      ret = GST_VIDEO_FORMAT_BGR;
      break;
    }
    case VPI_IMAGE_FORMAT_BGRA8:{
      ret = GST_VIDEO_FORMAT_BGRA;
      break;
    }
    default:{
      ret = GST_VIDEO_FORMAT_UNKNOWN;
    }
  }

  return ret;
}

GType
vpi_boundary_cond_enum_get_type (void)
{
  static GType vpi_boundary_cond_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_BOUNDARY_COND_ZERO, "All pixels outside the image are considered 0.",
        "zero"},
    {VPI_BOUNDARY_COND_CLAMP, "Border pixels are repeated indefinitely.",
        "clamp"},
    {0, NULL, NULL}
  };

  if (!vpi_boundary_cond_enum_type) {
    vpi_boundary_cond_enum_type = g_enum_register_static ("VpiBoundCond",
        values);
  }

  return vpi_boundary_cond_enum_type;
}

GType
vpi_interpolator_enum_get_type (void)
{
  static GType vpi_interpolator_enum_type = 0;
  static const GEnumValue values[] = {
    {VPI_INTERP_NEAREST, "Nearest neighbor interpolation.",
        "nearest"},
    {VPI_INTERP_LINEAR, "Fast linear interpolation.",
        "linear"},
    {VPI_INTERP_CATMULL_ROM, "Fast Catmull-Rom cubic interpolation.",
        "catmull"},
    {0, NULL, NULL}
  };

  if (!vpi_interpolator_enum_type) {
    vpi_interpolator_enum_type =
        g_enum_register_static ("VpiInterpolator", values);
  }

  return vpi_interpolator_enum_type;
}
