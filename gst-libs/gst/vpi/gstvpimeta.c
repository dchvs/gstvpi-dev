/*
 * Copyright (C) 2017 RidgeRun, LLC (http://www.ridgerun.com)
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

GType
gst_vpi_meta_api_get_type (void)
{
  static volatile GType type = 0;
  static const gchar *tags[] = { GST_META_TAG_VIDEO_STR, NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstVPIMetaAPI", tags);
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
        "GstVPIMeta",
        sizeof (GstVPIMeta),
        gst_vpi_meta_init,
        NULL,
        NULL);
    g_once_init_leave (&info, meta);
  }
  return info;
}

GstVPIMeta *
gst_buffer_add_vpi_meta (GstBuffer * buffer)
{
  g_return_val_if_fail (buffer != NULL, NULL);

  GST_LOG ("Adding VPI meta to buffer %p", buffer);

  return (GstVPIMeta *) gst_buffer_add_meta (buffer, GST_VPI_META_INFO, NULL);
}

static gboolean
gst_vpi_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  /* Gst requires this func to be implemented, even if it is empty */
  return TRUE;
}
