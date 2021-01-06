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

#ifndef _GST_VPI_KLT_TRACKER_H_
#define _GST_VPI_KLT_TRACKER_H_

#include <gst-libs/gst/vpi/gstvpifilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VPI_KLT_TRACKER (gst_vpi_klt_tracker_get_type())
G_DECLARE_DERIVABLE_TYPE(GstVpiKltTracker, gst_vpi_klt_tracker, GST, VPI_KLT_TRACKER, GstVpiFilter)

struct _GstVpiKltTrackerClass {
  GstVpiFilterClass parent_class;

  /* actions */
  void (*append_new_box) (GstVpiKltTracker *self, gint x, gint y, gint width,
                          gint height);
};

G_END_DECLS

#endif
