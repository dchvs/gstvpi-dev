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

#include "gstvpiklttracker.h"

#include <gst/gst.h>
#include <vpi/algo/KLTFeatureTracker.h>
#include <vpi/Array.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_klt_tracker_debug_category);
#define GST_CAT_DEFAULT gst_vpi_klt_tracker_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_LE }")

#define VPI_ARRAY_CAPACITY 128
#define MAX_BOUNDING_BOX 64
#define MIN_BOUNDING_BOX_SIZE 4
#define MAX_BOUNDING_BOX_SIZE 64
#define NEED_TEMPLATE_UPDATE 1
#define VALID_TRACKING 0
#define NUM_BOX_PARAMS 4
#define WHITE 255
#define BOX_BORDER_WIDTH 3
#define IDENTITY_TRANSFORM { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} }

#define DEFAULT_PROP_BOX_MIN 0
#define DEFAULT_PROP_BOX_MAX G_MAXINT
#define DEFAULT_PROP_MAX_CHANGE_MIN 0
#define DEFAULT_PROP_MAX_CHANGE_MAX G_MAXDOUBLE
#define DEFAULT_PROP_NCC_THRESHOLD_MIN 0.0001
#define DEFAULT_PROP_NCC_THRESHOLD_MAX 1
#define DEFAULT_PROP_SCALING_ITERATIONS_MIN 0
#define DEFAULT_PROP_SCALING_ITERATIONS_MAX G_MAXINT

#define DEFAULT_PROP_BOX 0
#define DEFAULT_PROP_DRAW_BOX TRUE
#define DEFAULT_PROP_MAX_SCALE_CHANGE 0.2
#define DEFAULT_PROP_MAX_TRANSLATION_CHANGE 1.5
#define DEFAULT_PROP_NCC_THRESHOLD_KILL 0.6
#define DEFAULT_PROP_NCC_THRESHOLD_STOP 1.0
#define DEFAULT_PROP_NCC_THRESHOLD_UPDATE 0.8
#define DEFAULT_PROP_SCALING_ITERATIONS 20

typedef struct _GstVpiKltTrackerPrivate GstVpiKltTrackerPrivate;

struct _GstVpiKltTrackerPrivate
{
  /* According to VPI requirements arrays must be of 128 */
  VPIKLTTrackedBoundingBox input_box_array[VPI_ARRAY_CAPACITY];
  VPIHomographyTransform2D input_trans_array[VPI_ARRAY_CAPACITY];
  VPIArray input_box_vpi_array;
  VPIArray input_trans_vpi_array;
  VPIArray output_box_vpi_array;
  VPIArray output_trans_vpi_array;
  VpiFrame template_frame;
  VPIKLTFeatureTrackerParams klt_params;
  VPIPayload klt;
  gboolean wrapped_arrays;
  gboolean first_frame;
  gboolean draw_box;
  guint box_count;
  guint total_boxes;
};
/* prototypes */
static VPIStatus gst_vpi_klt_tracker_set_vpi_arrays (GstVpiKltTracker * self,
    guint size);
static gboolean gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo
    * in_info, GstVideoInfo * out_info);
static void gst_vpi_klt_tracker_append_new_box (GstVpiKltTracker * self,
    gint x, gint y, gint width, gint height);
static GstFlowReturn gst_vpi_klt_tracker_transform_image_ip (GstVpiFilter *
    filter, VPIStream stream, VpiFrame * frame);
static gboolean gst_vpi_klt_tracker_stop (GstBaseTransform * trans);
static void gst_vpi_klt_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_finalize (GObject * object);

enum
{
  PROP_0,
  PROP_BOX,
  PROP_DRAW_BOX,
  PROP_MAX_SCALE_CHANGE,
  PROP_MAX_TRANSLATION_CHANGE,
  PROP_NCC_THRESHOLD_KILL,
  PROP_NCC_THRESHOLD_STOP,
  PROP_NCC_THRESHOLD_UPDATE,
  PROP_SCALING_ITERATIONS
};

enum
{
  X_POS,
  Y_POS,
  WIDTH,
  HEIGHT
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVpiKltTracker, gst_vpi_klt_tracker,
    GST_TYPE_VPI_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_klt_tracker_debug_category,
        "vpiklttracker", 0, "debug category for vpiklttracker element"));

static void
gst_vpi_klt_tracker_class_init (GstVpiKltTrackerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVpiFilterClass *vpi_filter_class = GST_VPI_FILTER_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (GstVpiKltTrackerPrivate));

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI KLT Tacker", "Filter/Video",
      "VPI based KLT feature tracker.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  klass->append_new_box =
      GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_append_new_box);
  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_start);
  vpi_filter_class->transform_image_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_transform_image_ip);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_stop);
  gobject_class->set_property = gst_vpi_klt_tracker_set_property;
  gobject_class->get_property = gst_vpi_klt_tracker_get_property;
  gobject_class->finalize = gst_vpi_klt_tracker_finalize;

  g_signal_new ("new-box", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstVpiKltTrackerClass, append_new_box), NULL, NULL,
      g_cclosure_marshal_generic, G_TYPE_NONE, NUM_BOX_PARAMS, G_TYPE_INT,
      G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  g_object_class_install_property (gobject_class, PROP_BOX,
      gst_param_spec_array ("boxes",
          "Bounding Boxes",
          "Nx4 matrix where N is the number of bounding boxes. Each bounding "
          "box (<x, y, w, h>) contains the x and y positions (x, y) of the box "
          "top left corner, and the width and height (w, h) of the bounding "
          "box. The maximum of bounding boxes is 64, and the minimum and "
          "maximum size for each bounding box is 4x4 and 64x64 respectively.\n"
          "Usage example: <<613,332,23,23>,<790,376,41,22>>",
          gst_param_spec_array ("bounding-boxes", "boxes", "boxes",
              g_param_spec_int ("boxes-params", "params", "params",
                  DEFAULT_PROP_BOX_MIN, DEFAULT_PROP_BOX_MAX, DEFAULT_PROP_BOX,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DRAW_BOX,
      g_param_spec_boolean ("draw-box", "Draw bounding box",
          "Draw bounding boxes of the tracker predictions.",
          DEFAULT_PROP_DRAW_BOX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MAX_SCALE_CHANGE,
      g_param_spec_double ("max-scale", "Maximum relative scale change",
          "Scale changes larger that this will make KLT consider that tracking"
          " was lost. Must be positive.",
          DEFAULT_PROP_MAX_CHANGE_MIN, DEFAULT_PROP_MAX_CHANGE_MAX,
          DEFAULT_PROP_MAX_SCALE_CHANGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MAX_TRANSLATION_CHANGE,
      g_param_spec_double ("max-trans", "Maximum relative translation change",
          "Translation changes larger that this will make KLT consider that "
          "tracking was lost. Must be positive.",
          DEFAULT_PROP_MAX_CHANGE_MIN,
          DEFAULT_PROP_MAX_CHANGE_MAX,
          DEFAULT_PROP_MAX_TRANSLATION_CHANGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_NCC_THRESHOLD_KILL,
      g_param_spec_double ("threshold-kill", "Threshold to kill tracking",
          "Threshold to consider template tracking was lost. Must be a value "
          "between 0 and 1 but the relationship between the other thresholds "
          "must be the following: 0 < kill <= update <= stop <= 1.",
          DEFAULT_PROP_NCC_THRESHOLD_MIN,
          DEFAULT_PROP_NCC_THRESHOLD_MAX, DEFAULT_PROP_NCC_THRESHOLD_KILL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_NCC_THRESHOLD_STOP,
      g_param_spec_double ("threshold-stop", "Threshold to stop iteration",
          "Threshold to stop estimating.  Must be a value between 0 and 1 but "
          "the relationship between the other thresholds must be the following"
          ": 0 < kill <= update <= stop <= 1.",
          DEFAULT_PROP_NCC_THRESHOLD_MIN,
          DEFAULT_PROP_NCC_THRESHOLD_MAX, DEFAULT_PROP_NCC_THRESHOLD_STOP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_NCC_THRESHOLD_UPDATE,
      g_param_spec_double ("threshold-update", "Threshold to update template",
          "Threshold for requiring template update. Must be a value between 0 "
          "and 1 but the relationship between the other thresholds must be the"
          " following: 0 < kill <= update <= stop <= 1.",
          DEFAULT_PROP_NCC_THRESHOLD_MIN,
          DEFAULT_PROP_NCC_THRESHOLD_MAX,
          DEFAULT_PROP_NCC_THRESHOLD_UPDATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SCALING_ITERATIONS,
      g_param_spec_int ("scaling-iterations",
          "Number of iterations for scaling",
          "Number of inverse compositional iterations of scale estimations.",
          DEFAULT_PROP_SCALING_ITERATIONS_MIN,
          DEFAULT_PROP_SCALING_ITERATIONS_MAX, DEFAULT_PROP_SCALING_ITERATIONS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_klt_tracker_init (GstVpiKltTracker * self)
{
  GstVpiKltTrackerPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  priv->klt_params.numberOfIterationsScaling = DEFAULT_PROP_SCALING_ITERATIONS;
  priv->klt_params.nccThresholdUpdate = DEFAULT_PROP_NCC_THRESHOLD_UPDATE;
  priv->klt_params.nccThresholdKill = DEFAULT_PROP_NCC_THRESHOLD_KILL;
  priv->klt_params.nccThresholdStop = DEFAULT_PROP_NCC_THRESHOLD_STOP;
  priv->klt_params.maxScaleChange = DEFAULT_PROP_MAX_SCALE_CHANGE;
  priv->klt_params.maxTranslationChange = DEFAULT_PROP_MAX_TRANSLATION_CHANGE;
  priv->klt_params.trackingType = VPI_KLT_INVERSE_COMPOSITIONAL;

  priv->draw_box = DEFAULT_PROP_DRAW_BOX;
  priv->wrapped_arrays = FALSE;
  priv->total_boxes = 0;
}

static void
gst_vpi_klt_tracker_validate_thresholds (GstVpiKltTracker * self)
{
  GstVpiKltTrackerPrivate *priv = NULL;

  g_return_if_fail (self);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_OBJECT_LOCK (self);

  /* We must verify that kill <= update <= stop */
  if ((priv->klt_params.nccThresholdUpdate > priv->klt_params.nccThresholdStop)
      || (priv->klt_params.nccThresholdKill >
          priv->klt_params.nccThresholdUpdate)) {
    priv->klt_params.nccThresholdKill = DEFAULT_PROP_NCC_THRESHOLD_KILL;
    priv->klt_params.nccThresholdUpdate = DEFAULT_PROP_NCC_THRESHOLD_UPDATE;
    priv->klt_params.nccThresholdStop = DEFAULT_PROP_NCC_THRESHOLD_STOP;
    GST_WARNING_OBJECT (priv, "The relationship kill <= update <= stop was not"
        " respected. Using default values for all the thresholds. kill=%f "
        "update=%f stop=%f.", priv->klt_params.nccThresholdKill,
        priv->klt_params.nccThresholdUpdate, priv->klt_params.nccThresholdStop);
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiKltTracker *self = NULL;
  GstVpiKltTrackerPrivate *priv = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  guint width = 0;
  guint height = 0;
  GstVideoFormat format = 0;
  gint backend = VPI_BACKEND_INVALID;

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_KLT_TRACKER (filter);
  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_DEBUG_OBJECT (self, "start");

  /* If no boxes were provided initialize the vpi arrays with size 0 */
  if (!priv->wrapped_arrays) {
    gst_vpi_klt_tracker_set_vpi_arrays (self, 0);
  }
  gst_vpi_klt_tracker_validate_thresholds (self);

  priv->first_frame = TRUE;
  priv->template_frame.image = NULL;
  priv->template_frame.buffer = NULL;

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);
  format = GST_VIDEO_INFO_FORMAT (in_info);
  backend = gst_vpi_filter_get_backend (filter);

  status = vpiCreateKLTFeatureTracker (backend, width, height,
      gst_vpi_video_to_image_format (format), &priv->klt);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create KLT tracker payload."), ("%s",
            vpiStatusGetName (status)));
    ret = FALSE;
    goto out;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX, VPI_BACKEND_ALL,
      &priv->output_box_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output bounding box array."), ("%s",
            vpiStatusGetName (status)));
    ret = FALSE;
    goto free_klt;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D, VPI_BACKEND_ALL,
      &priv->output_trans_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output homographies array."), ("%s",
            vpiStatusGetName (status)));
    ret = FALSE;
    goto free_out_box_array;
  }

  goto out;

free_out_box_array:
  vpiArrayDestroy (priv->output_box_vpi_array);
  priv->output_box_vpi_array = NULL;

free_klt:
  vpiPayloadDestroy (priv->klt);
  priv->klt = NULL;

out:
  return ret;
}

static void
gst_vpi_klt_tracker_draw_box_data (GstVpiKltTracker * self, VPIImage image)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  VPIImageData vpi_image_data = { 0 };
  VPIArrayData box_data = { 0 };
  VPIArrayData trans_data = { 0 };
  guint8 *image_data = NULL;
  VPIKLTTrackedBoundingBox *box = NULL;
  VPIHomographyTransform2D *trans = NULL;
  guint stride = 0;
  VPIImageFormat format = 0;
  guint scale_f = 0;
  guint b, i, j, i_aux, j_aux = 0;
  guint x, y, h, w = 0;

  g_return_if_fail (self);
  g_return_if_fail (image);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_OBJECT_LOCK (self);

  vpiImageLock (image, VPI_LOCK_READ_WRITE, &vpi_image_data);
  /* Supported formats only have one plane */
  stride = vpi_image_data.planes[0].pitchBytes;
  format = vpi_image_data.type;
  image_data = (guint8 *) vpi_image_data.planes[0].data;

  vpiArrayLock (priv->input_box_vpi_array, VPI_LOCK_READ, &box_data);
  vpiArrayLock (priv->input_trans_vpi_array, VPI_LOCK_READ, &trans_data);
  box = (VPIKLTTrackedBoundingBox *) box_data.data;
  trans = (VPIHomographyTransform2D *) trans_data.data;

  /* To address both types with same pointer */
  scale_f = (VPI_IMAGE_FORMAT_U8 == format) ? 1 : 2;

  for (b = 0; b < priv->total_boxes; b++) {
    if (box[b].trackingStatus == VALID_TRACKING) {
      x = (guint) box[b].bbox.xform.mat3[0][2] + trans[b].mat3[0][2];
      y = (guint) box[b].bbox.xform.mat3[1][2] + trans[b].mat3[1][2];
      w = (guint) box[b].bbox.width * box[b].bbox.xform.mat3[0][0] *
          trans[b].mat3[0][0];
      h = (guint) box[b].bbox.height * box[b].bbox.xform.mat3[1][1] *
          trans[b].mat3[1][1];

      /* Top and bottom borders */
      for (i = y; i < y + BOX_BORDER_WIDTH; i++) {
        for (j = scale_f * x; j < scale_f * (x + w); j++) {
          image_data[i * stride + j] = WHITE;
          i_aux = i + h - BOX_BORDER_WIDTH - 1;
          image_data[i_aux * stride + j] = WHITE;
        }
      }
      /* Left and right borders (do not include corners that were already
         covered by previous for) */
      for (i = y + BOX_BORDER_WIDTH; i < y + h - BOX_BORDER_WIDTH; i++) {
        for (j = scale_f * x; j < scale_f * (x + BOX_BORDER_WIDTH); j++) {
          image_data[i * stride + j] = WHITE;
          j_aux = j + scale_f * (w - BOX_BORDER_WIDTH);
          image_data[i * stride + j_aux] = WHITE;
        }
      }
    }
  }

  vpiImageUnlock (image);
  vpiArrayUnlock (priv->input_box_vpi_array);
  vpiArrayUnlock (priv->input_trans_vpi_array);

  GST_OBJECT_UNLOCK (self);
}

static void
gst_vpi_klt_tracker_update_bounding_boxes_status (GstVpiKltTracker * self)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  VPIArrayData updated_box_data = { 0 };
  VPIArrayData updated_trans_data = { 0 };
  VPIKLTTrackedBoundingBox *updated_box = NULL;
  VPIHomographyTransform2D *updated_trans = NULL;
  guint i = 0;

  g_return_if_fail (self);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  vpiArrayLock (priv->output_box_vpi_array, VPI_LOCK_READ, &updated_box_data);
  vpiArrayLock (priv->output_trans_vpi_array, VPI_LOCK_READ,
      &updated_trans_data);
  updated_box = (VPIKLTTrackedBoundingBox *) updated_box_data.data;
  updated_trans = (VPIHomographyTransform2D *) updated_trans_data.data;

  /* Set the input for next frame */
  GST_OBJECT_LOCK (self);
  for (i = 0; i < priv->total_boxes; i++) {
    priv->input_box_array[i].trackingStatus = updated_box[i].trackingStatus;
    priv->input_box_array[i].templateStatus = updated_box[i].templateStatus;

    /* Skip boxes that are not being tracked */
    if (VALID_TRACKING != updated_box[i].trackingStatus) {
      continue;
    }
    /* Must update template for this box ? */
    if (NEED_TEMPLATE_UPDATE == updated_box[i].templateStatus) {
      VPIHomographyTransform2D identity = { IDENTITY_TRANSFORM };

      /* Simple update approach */
      priv->input_box_array[i] = updated_box[i];
      priv->input_box_array[i].templateStatus = NEED_TEMPLATE_UPDATE;
      priv->input_trans_array[i] = identity;
    } else {
      priv->input_box_array[i].templateStatus = !NEED_TEMPLATE_UPDATE;
      priv->input_trans_array[i] = updated_trans[i];
    }
  }
  GST_OBJECT_UNLOCK (self);
  vpiArrayUnlock (priv->output_box_vpi_array);
  vpiArrayUnlock (priv->output_trans_vpi_array);
}

static void
gst_vpi_klt_tracker_track_bounding_boxes (GstVpiKltTracker * self,
    VPIStream stream, VPIImage image)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  VPIStatus status = VPI_SUCCESS;
  gboolean draw_box = DEFAULT_PROP_DRAW_BOX;

  g_return_if_fail (self);
  g_return_if_fail (stream);
  g_return_if_fail (image);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_OBJECT_LOCK (self);
  draw_box = priv->draw_box;
  status =
      vpiSubmitKLTFeatureTracker (stream, priv->klt, priv->template_frame.image,
      priv->input_box_vpi_array, priv->input_trans_vpi_array, image,
      priv->output_box_vpi_array, priv->output_trans_vpi_array,
      &priv->klt_params);
  vpiStreamSync (stream);
  GST_OBJECT_UNLOCK (self);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not predict the new bounding boxes."), ("%s",
            vpiStatusGetName (status)));
    goto out;
  }

  gst_vpi_klt_tracker_update_bounding_boxes_status (self);

  /* Force arrays to update because wrapped memory has been modifed */
  GST_OBJECT_LOCK (self);
  vpiArrayInvalidate (priv->input_box_vpi_array);
  vpiArrayInvalidate (priv->input_trans_vpi_array);
  GST_OBJECT_UNLOCK (self);

  if (draw_box) {
    gst_vpi_klt_tracker_draw_box_data (self, image);
  }

out:
  return;
}

static GstFlowReturn
gst_vpi_klt_tracker_transform_image_ip (GstVpiFilter * filter, VPIStream stream,
    VpiFrame * frame)
{
  GstVpiKltTracker *self = NULL;
  GstVpiKltTrackerPrivate *priv = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->image, GST_FLOW_ERROR);

  self = GST_VPI_KLT_TRACKER (filter);
  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_LOG_OBJECT (self, "Transform image ip");

  if (priv->first_frame) {
    GST_DEBUG_OBJECT (self, "Setting first frame");
    priv->first_frame = FALSE;

  } else {
    gst_vpi_klt_tracker_track_bounding_boxes (self, stream, frame->image);
  }

  if (priv->template_frame.buffer) {
    gst_buffer_unref (priv->template_frame.buffer);
  }
  priv->template_frame.buffer = gst_buffer_ref (frame->buffer);
  priv->template_frame.image = frame->image;

  return ret;
}

static VPIStatus
gst_vpi_klt_tracker_wrap_vpi_array (GstVpiKltTracker * self,
    VPIArrayData * array_data, void *data, VPIArray * array, VPIArrayType type)
{
  g_return_val_if_fail (self, VPI_ERROR_INVALID_ARGUMENT);
  g_return_val_if_fail (array_data, VPI_ERROR_INVALID_ARGUMENT);
  g_return_val_if_fail (data, VPI_ERROR_INVALID_ARGUMENT);
  g_return_val_if_fail (array, VPI_ERROR_INVALID_ARGUMENT);

  array_data->type = type;
  array_data->data = data;

  return vpiArrayCreateHostMemWrapper (array_data, VPI_BACKEND_ALL, array);
}

static VPIStatus
gst_vpi_klt_tracker_set_vpi_arrays (GstVpiKltTracker * self, guint size)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  VPIArrayData array_data = { 0 };
  VPIStatus status = VPI_SUCCESS;

  g_return_val_if_fail (self, VPI_ERROR_INVALID_ARGUMENT);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  array_data.capacity = VPI_ARRAY_CAPACITY;
  array_data.size = size;

  status =
      gst_vpi_klt_tracker_wrap_vpi_array (self, &array_data,
      priv->input_box_array, &priv->input_box_vpi_array,
      VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX);

  if (VPI_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Could not wrap bounding boxes into VPIArray. Code"
        " error: %s", vpiStatusGetName (status));
    goto out;
  }

  status =
      gst_vpi_klt_tracker_wrap_vpi_array (self, &array_data,
      priv->input_trans_array, &priv->input_trans_vpi_array,
      VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D);

  if (VPI_SUCCESS != status) {
    GST_ERROR_OBJECT (self, "Could not wrap homographies into VPIArray. Code "
        "error: %s", vpiStatusGetName (status));
    goto free_in_box_array;
  }

  goto out;

free_in_box_array:
  vpiArrayDestroy (priv->input_box_vpi_array);
  priv->input_box_vpi_array = NULL;

out:
  return status;
}

static VPIStatus
gst_vpi_klt_tracker_update_vpi_arrays (GstVpiKltTracker * self, gint boxes)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  VPIStatus ret = VPI_SUCCESS;

  g_return_val_if_fail (self, VPI_ERROR_INVALID_ARGUMENT);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  /* If the wrappers have not been created, do it with new boxes data */
  if (!priv->wrapped_arrays) {
    ret = gst_vpi_klt_tracker_set_vpi_arrays (self, boxes);
    priv->wrapped_arrays = (VPI_SUCCESS == ret);
  } else {
    /* Update the VPI array size in case number of boxes changed */
    if (boxes != priv->total_boxes) {
      vpiArrayLock (priv->input_box_vpi_array, VPI_LOCK_READ_WRITE, NULL);
      vpiArraySetSize (priv->input_box_vpi_array, boxes);
      vpiArrayUnlock (priv->input_box_vpi_array);
      vpiArrayLock (priv->input_trans_vpi_array, VPI_LOCK_READ_WRITE, NULL);
      vpiArraySetSize (priv->input_trans_vpi_array, boxes);
      vpiArrayUnlock (priv->input_trans_vpi_array);
    }
    /* Update the VPI array content */
    vpiArrayInvalidate (priv->input_box_vpi_array);
    vpiArrayInvalidate (priv->input_trans_vpi_array);
  }
  return ret;
}

static void
gst_vpi_klt_tracker_set_box_at (GstVpiKltTracker * self, gint index, gint x,
    gint y, gint width, gint height)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  float identity[3][3] = IDENTITY_TRANSFORM;

  g_return_if_fail (self);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  memcpy (&priv->input_box_array[index].bbox.xform.mat3, &identity,
      sizeof (identity));
  priv->input_box_array[index].bbox.xform.mat3[0][2] = x;
  priv->input_box_array[index].bbox.xform.mat3[1][2] = y;
  priv->input_box_array[index].bbox.width = width;
  priv->input_box_array[index].bbox.height = height;
  priv->input_box_array[index].trackingStatus = VALID_TRACKING;
  priv->input_box_array[index].templateStatus = NEED_TEMPLATE_UPDATE;
  memcpy (&priv->input_trans_array[index].mat3, &identity, sizeof (identity));
}

static guint
gst_vpi_klt_tracker_fill_bounding_boxes (GstVpiKltTracker * self,
    const GValue * gst_array, guint boxes, guint params)
{
  const GValue *box = NULL;
  gint width = 0;
  gint height = 0;
  guint i = 0;
  guint ret = 0;
  guint cur_box = 0;

  g_return_val_if_fail (self, ret);
  g_return_val_if_fail (gst_array, ret);

  for (i = 0; i < boxes; i++) {
    box = gst_value_array_get_value (gst_array, i);
    params = gst_value_array_get_size (box);

    if (NUM_BOX_PARAMS != params) {
      GST_ERROR_OBJECT (self,
          "Bounding boxes must have 4 parameters. Box %d has %d parameters.", i,
          params);
      goto out;
    }

    width = g_value_get_int (gst_value_array_get_value (box, WIDTH));
    height = g_value_get_int (gst_value_array_get_value (box, HEIGHT));

    if (MIN_BOUNDING_BOX_SIZE > width || MAX_BOUNDING_BOX_SIZE < width
        || MIN_BOUNDING_BOX_SIZE > height || MAX_BOUNDING_BOX_SIZE < height) {
      GST_WARNING_OBJECT (self,
          "Size must be between 4x4 and 64x64. Received %dx%d. Discarding "
          "bounding box %d.", width, height, i);
      continue;
    }
    gst_vpi_klt_tracker_set_box_at (self, cur_box,
        g_value_get_int (gst_value_array_get_value (box, X_POS)),
        g_value_get_int (gst_value_array_get_value (box, Y_POS)), width,
        height);
    cur_box++;

    /* Do this validation here in case more than 64 boxes were provided but
       some were discarded due to invalid parameters, leaving a valid number of
       boxes */
    if (MAX_BOUNDING_BOX <= cur_box) {
      GST_WARNING_OBJECT (self,
          "Received 64 boxes. Extra boxes will be discarded.");
      break;
    }
  }
  ret = cur_box;
out:
  return ret;
}

static void
gst_vpi_klt_tracker_set_bounding_boxes (GstVpiKltTracker * self,
    const GValue * gst_array)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  guint boxes = 0;
  guint params = 0;

  guint cur_box = 0;
  VPIStatus status = VPI_SUCCESS;

  g_return_if_fail (self);
  g_return_if_fail (gst_array);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  boxes = gst_value_array_get_size (gst_array);
  params = gst_value_array_get_size (gst_value_array_get_value (gst_array, 0));

  /* Reset arrays before filling them again */
  memset (priv->input_box_array, 0,
      priv->total_boxes * sizeof (VPIKLTTrackedBoundingBox));
  memset (priv->input_trans_array, 0,
      priv->total_boxes * sizeof (VPIHomographyTransform2D));
  priv->total_boxes = 0;

  cur_box =
      gst_vpi_klt_tracker_fill_bounding_boxes (self, gst_array, boxes, params);

  if (0 == cur_box) {
    goto out;
  }

  status = gst_vpi_klt_tracker_update_vpi_arrays (self, cur_box);
  if (VPI_SUCCESS != status) {
    goto out;
  }

  priv->total_boxes = cur_box;

out:
  return;
}

static void
gst_vpi_klt_tracker_append_new_box (GstVpiKltTracker * self, gint x, gint y,
    gint width, gint height)
{
  GstVpiKltTrackerPrivate *priv = NULL;

  g_return_if_fail (self);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_DEBUG_OBJECT (self, "Received new box");

  if (MIN_BOUNDING_BOX_SIZE > width || MAX_BOUNDING_BOX_SIZE < width
      || MIN_BOUNDING_BOX_SIZE > height || MAX_BOUNDING_BOX_SIZE < height) {
    GST_WARNING_OBJECT (self,
        "Size must be between 4x4 and 64x64. Received %dx%d. Refused append.",
        width, height);
    goto out;
  }

  if (priv->total_boxes >= MAX_BOUNDING_BOX) {
    GST_WARNING_OBJECT (self,
        "Maximum number of boxes reached. Refused append.");
    goto out;
  }

  GST_OBJECT_LOCK (self);
  gst_vpi_klt_tracker_set_box_at (self, priv->total_boxes, x, y, width, height);
  priv->total_boxes++;
  gst_vpi_klt_tracker_update_vpi_arrays (self, priv->total_boxes);
  GST_OBJECT_UNLOCK (self);

out:
  return;
}

static void
gst_vpi_klt_tracker_get_bounding_boxes (GstVpiKltTracker * self,
    GValue * gst_array)
{
  GstVpiKltTrackerPrivate *priv = NULL;
  GValue box = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (self);
  g_return_if_fail (gst_array);

  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  for (i = 0; i < priv->total_boxes; i++) {

    guint params[NUM_BOX_PARAMS] = {
      priv->input_box_array[i].bbox.xform.mat3[0][2],
      priv->input_box_array[i].bbox.xform.mat3[1][2],
      priv->input_box_array[i].bbox.width,
      priv->input_box_array[i].bbox.height
    };
    g_value_init (&box, GST_TYPE_ARRAY);

    for (j = 0; j < NUM_BOX_PARAMS; j++) {

      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, params[j]);
      gst_value_array_append_value (&box, &value);
      g_value_unset (&value);
    }

    gst_value_array_append_value (gst_array, &box);
    g_value_unset (&box);
  }
}

void
gst_vpi_klt_tracker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);
  GstVpiKltTrackerPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GST_TYPE_VPI_KLT_TRACKER, GstVpiKltTrackerPrivate);

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOX:
      gst_vpi_klt_tracker_set_bounding_boxes (self, value);
      break;
    case PROP_DRAW_BOX:
      priv->draw_box = g_value_get_boolean (value);
      break;
    case PROP_MAX_SCALE_CHANGE:
      priv->klt_params.maxScaleChange = g_value_get_double (value);
      break;
    case PROP_MAX_TRANSLATION_CHANGE:
      priv->klt_params.maxTranslationChange = g_value_get_double (value);
      break;
    case PROP_NCC_THRESHOLD_KILL:
      priv->klt_params.nccThresholdKill = g_value_get_double (value);
      break;
    case PROP_NCC_THRESHOLD_STOP:
      priv->klt_params.nccThresholdStop = g_value_get_double (value);
      break;
    case PROP_NCC_THRESHOLD_UPDATE:
      priv->klt_params.nccThresholdUpdate = g_value_get_double (value);
      break;
    case PROP_SCALING_ITERATIONS:
      priv->klt_params.numberOfIterationsScaling = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_vpi_klt_tracker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);
  GstVpiKltTrackerPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GST_TYPE_VPI_KLT_TRACKER, GstVpiKltTrackerPrivate);

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOX:
      gst_vpi_klt_tracker_get_bounding_boxes (self, value);
      break;
    case PROP_DRAW_BOX:
      g_value_set_boolean (value, priv->draw_box);
      break;
    case PROP_MAX_SCALE_CHANGE:
      g_value_set_double (value, priv->klt_params.maxScaleChange);
      break;
    case PROP_MAX_TRANSLATION_CHANGE:
      g_value_set_double (value, priv->klt_params.maxTranslationChange);
      break;
    case PROP_NCC_THRESHOLD_KILL:
      g_value_set_double (value, priv->klt_params.nccThresholdKill);
      break;
    case PROP_NCC_THRESHOLD_STOP:
      g_value_set_double (value, priv->klt_params.nccThresholdStop);
      break;
    case PROP_NCC_THRESHOLD_UPDATE:
      g_value_set_double (value, priv->klt_params.nccThresholdUpdate);
      break;
    case PROP_SCALING_ITERATIONS:
      g_value_set_int (value, priv->klt_params.numberOfIterationsScaling);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (self);
}

static gboolean
gst_vpi_klt_tracker_stop (GstBaseTransform * trans)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (trans);
  GstVpiKltTrackerPrivate *priv = NULL;
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_klt_tracker_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");
  priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GST_TYPE_VPI_KLT_TRACKER,
      GstVpiKltTrackerPrivate);

  GST_OBJECT_LOCK (self);

  vpiArrayDestroy (priv->input_trans_vpi_array);
  priv->input_trans_vpi_array = NULL;

  vpiArrayDestroy (priv->input_box_vpi_array);
  priv->input_box_vpi_array = NULL;

  vpiPayloadDestroy (priv->klt);
  priv->klt = NULL;

  GST_OBJECT_UNLOCK (self);

  vpiArrayDestroy (priv->output_trans_vpi_array);
  priv->output_trans_vpi_array = NULL;

  vpiArrayDestroy (priv->output_box_vpi_array);
  priv->output_box_vpi_array = NULL;

  if (priv->template_frame.buffer) {
    gst_buffer_unref (priv->template_frame.buffer);
  }
  priv->template_frame.buffer = NULL;
  priv->template_frame.image = NULL;

  return ret;
}

void
gst_vpi_klt_tracker_finalize (GObject * object)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (self, "finalize");

  G_OBJECT_CLASS (gst_vpi_klt_tracker_parent_class)->finalize (object);
}
