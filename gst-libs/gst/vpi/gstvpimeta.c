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

#include "gstvpimeta.h"

#include "gstvpi.h"

static gboolean gst_vpi_meta_init (GstMeta * meta,
    gpointer params, GstBuffer * buffer);
static void gst_vpi_meta_free (GstMeta * meta, GstBuffer * buffer);
static gboolean gst_vpi_meta_transform (GstBuffer * transbuf,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data);
static gboolean gst_vpi_meta_copy (GstVpiMeta * dst, GstVpiMeta * src);

static void gst_vpi_image_free (gpointer data);

#define VPI_IMAGE_QUARK_STR "VPIImage"
static GQuark _vpi_image_quark;

GType
gst_vpi_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] =
      { GST_META_TAG_VIDEO_STR, GST_META_TAG_MEMORY_STR, NULL };

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
        gst_vpi_meta_transform);

    _vpi_image_quark = g_quark_from_static_string (VPI_IMAGE_QUARK_STR);

    g_once_init_leave (&info, meta);
  }
  return info;
}

static void
gst_vpi_image_free (gpointer data)
{
  VPIImage image = (VPIImage) data;

  GST_INFO ("Freeing VPI image %p", image);

  vpiImageDestroy (image);
}

GstVpiMeta *
gst_buffer_add_vpi_meta (GstBuffer * buffer, GstVideoInfo * video_info)
{
  GstVpiMeta *self = NULL;
  VPIImageData vpi_image_data = { 0 };
  GstMapInfo minfo = GST_MAP_INFO_INIT;
  VPIStatus status = VPI_SUCCESS;
  gint i = 0;
  GstMemory *mem = NULL;

  g_return_val_if_fail (buffer, NULL);
  g_return_val_if_fail (video_info, NULL);

  GST_LOG ("Adding VPI meta to buffer %" GST_PTR_FORMAT, buffer);

  self = (GstVpiMeta *) gst_buffer_add_meta (buffer, GST_VPI_META_INFO, NULL);

  /* Can't save a reference or upstream element will find the buffer
     non-writable */
  self->vpi_frame.buffer = buffer;
  gst_buffer_map (buffer, &minfo, GST_MAP_READ);

  vpi_image_data.type =
      gst_vpi_video_to_image_format (GST_VIDEO_INFO_FORMAT (video_info));
  vpi_image_data.numPlanes = GST_VIDEO_INFO_N_PLANES (video_info);

  for (i = 0; i < vpi_image_data.numPlanes; i++) {
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

  gst_buffer_unmap (buffer, &minfo);

  status = vpiImageCreateCudaMemWrapper (&vpi_image_data, VPI_BACKEND_ALL,
      &(self->vpi_frame.image));
  if (VPI_SUCCESS != status) {
    GST_ERROR ("Could not wrap buffer in VPIImage");
    self = NULL;
  }

  /* Associate the VPIImage to the memory, so it only gets destroyed
     when the underlying memory is destroyed. This will allow us to
     share images with different buffers that share the same memory,
     without worrying of an early free.
   */
  mem = gst_buffer_get_all_memory (buffer);
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem), _vpi_image_quark,
      self->vpi_frame.image, gst_vpi_image_free);
  gst_memory_unref (mem);

  return self;
}

static gboolean
gst_vpi_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstVpiMeta *self = (GstVpiMeta *) meta;

  /* *INDENT-OFF* */
  self->vpi_frame = (VpiFrame) { 0 };
  /* *INDENT-ON* */

  return TRUE;
}

static void
gst_vpi_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstVpiMeta *self = (GstVpiMeta *) meta;

  /* We don't need to free the Image, since its now associated to the
     memory and will be freed when that is no longer used */
  self->vpi_frame.image = NULL;

  self->vpi_frame.buffer = NULL;
}

static gboolean
gst_vpi_meta_copy (GstVpiMeta * dst, GstVpiMeta * src)
{
  g_return_val_if_fail (dst, FALSE);
  g_return_val_if_fail (src, FALSE);

  *dst = *src;

  return TRUE;
}

static gboolean
gst_vpi_meta_transform (GstBuffer * dest,
    GstMeta * meta, GstBuffer * buffer, GQuark type, gpointer data)
{
  GstVpiMeta *newmeta = NULL;
  gboolean ret = FALSE;

  /* TODO: actually care about the transformation type. I.e.: if it's
     a scaling, we might want to adjust bounding boxes, etc...
   */
  newmeta = (GstVpiMeta *) gst_buffer_add_meta (dest, GST_VPI_META_INFO, NULL);

  ret = gst_vpi_meta_copy (newmeta, (GstVpiMeta *) meta);
  newmeta->vpi_frame.buffer = dest;

  return ret;
}
