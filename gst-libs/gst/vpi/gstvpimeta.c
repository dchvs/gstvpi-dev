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

#include <gst/video/video.h>

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
  GstVpiMeta *meta = NULL;
  GstMapInfo *minfo = NULL;
  VPIImageData vpi_imgdata;
  VPIStatus status;

  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (video_info != NULL, NULL);

  GST_LOG ("Adding VPI meta to buffer %p", buffer);

  minfo = g_slice_new0 (GstMapInfo);
  gst_buffer_map (buffer, minfo, GST_MAP_READ | GST_MAP_WRITE);

  meta = (GstVpiMeta *) gst_buffer_add_meta (buffer, GST_VPI_META_INFO, NULL);

  memset (&(vpi_imgdata), 0, sizeof (vpi_imgdata));
  vpi_imgdata.type = VPI_IMAGE_TYPE_U8;
  vpi_imgdata.numPlanes = 1;
  vpi_imgdata.planes[0].width = GST_VIDEO_INFO_WIDTH (video_info);
  vpi_imgdata.planes[0].height = GST_VIDEO_INFO_HEIGHT (video_info);
  vpi_imgdata.planes[0].rowStride = video_info->stride[0];
  vpi_imgdata.planes[0].data = minfo->data;

  status = vpiImageWrapCudaDeviceMem (&vpi_imgdata, 0, &(meta->vpi_image));
  if (status != VPI_SUCCESS) {
    GST_ERROR ("Could not wrap buffer in VPIImage");
  }

  return meta;
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

  vpiImageDestroy (vpi_meta->vpi_image);
}
