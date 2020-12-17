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

#include "gstvpimeta.h"

#include "gstvpi.h"

static gboolean gst_vpi_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);
static void gst_vpi_meta_free (GstMeta * meta, GstBuffer * buffer);

GType
gst_vpi_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { GST_META_TAG_VIDEO_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVpiMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_vpi_meta_get_info (void)
{
  static const GstMetaInfo *info = NULL;

  if (g_once_init_enter (&info)) {
    const GstMetaInfo *meta = gst_meta_register (GST_VPI_META_API_TYPE,
        "GstVpiMeta",
        sizeof (GstVpiMeta),
        gst_vpi_meta_init,
        gst_vpi_meta_free,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

GstVpiMeta *
gst_buffer_add_vpi_meta (GstBuffer * buffer, GstVideoInfo * video_info)
{
  GstVpiMeta *ret = NULL;
  GstMapInfo minfo;
  VPIImageData vpi_image_data;
  VPIStatus status;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (video_info, NULL);

  GST_LOG ("Adding VPI meta to buffer %p", buffer);

  gst_buffer_map (buffer, &minfo, GST_MAP_READWRITE);

  ret = (GstVpiMeta *) gst_buffer_add_meta (buffer, GST_VPI_META_INFO, NULL);

  memset (&(vpi_image_data), 0, sizeof (vpi_image_data));
  vpi_image_data.type =
      gst_vpi_video_to_image_format (GST_VIDEO_INFO_FORMAT (video_info));
  vpi_image_data.numPlanes = GST_VIDEO_INFO_N_PLANES (video_info);
  for (int i = 0; i < vpi_image_data.numPlanes; i++) {
    vpi_image_data.planes[i].width =
        GST_VIDEO_SUB_SCALE (video_info->finfo->w_sub[i],
        GST_VIDEO_INFO_WIDTH (video_info));
    vpi_image_data.planes[i].height =
        GST_VIDEO_SUB_SCALE (video_info->finfo->h_sub[i],
        GST_VIDEO_INFO_HEIGHT (video_info));
    vpi_image_data.planes[i].pitchBytes =
        GST_VIDEO_INFO_PLANE_STRIDE (video_info, i);
    vpi_image_data.planes[i].data =
        minfo.data + GST_VIDEO_INFO_PLANE_OFFSET (video_info, i);
  }

  ret->vpi_frame.buffer = buffer;
  status = vpiImageCreateCudaMemWrapper (&vpi_image_data, VPI_BACKEND_ALL,
      &(ret->vpi_frame.image));
  if (VPI_SUCCESS != status) {
    GST_ERROR ("Could not wrap buffer in VPIImage");
    ret = NULL;
  }

  return ret;
}

static gboolean
gst_vpi_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}

static void
gst_vpi_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVpiMeta *vpi_meta = (GstVpiMeta *) meta;

  vpiImageDestroy (vpi_meta->vpi_frame.image);
}
