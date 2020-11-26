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
#include <vpi/Array.h>
#include <vpi/algo/Rescale.h>
#include <vpi/algo/KLTFeatureTracker.h>

#include "gst-libs/gst/vpi/gstvpi.h"

GST_DEBUG_CATEGORY_STATIC (gst_vpi_klt_tracker_debug_category);
#define GST_CAT_DEFAULT gst_vpi_klt_tracker_debug_category

#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VPIImage", "{ GRAY8, GRAY16_BE, GRAY16_LE }")

#define VPI_ARRAY_CAPACITY 128
#define NEED_TEMPLATE_UPDATE 1
#define VALID_TRACKING 0
#define NUM_BOX_PARAMS 5
#define WHITE 255
#define IDENTITY_TRANSFORM { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} }

#define DEFAULT_PROP_BOX_MIN 0
#define DEFAULT_PROP_BOX_MAX G_MAXDOUBLE

#define DEFAULT_PROP_BOX 0

struct _GstVpiKltTracker
{
  GstVpiFilter parent;
  VPIKLTTrackedBoundingBox input_box_array[VPI_ARRAY_CAPACITY];
  VPIHomographyTransform2D input_trans_array[VPI_ARRAY_CAPACITY];
  VPIArray input_box_vpi_array;
  VPIArray input_trans_vpi_array;
  VPIArray output_box_vpi_array;
  VPIArray output_trans_vpi_array;
  VPIImage template_image;
  VPIKLTFeatureTrackerParams klt_params;
  VPIPayload klt;
  guint *box_frames;
  guint frame_count;
  guint box_count;
  guint total_boxes;
};

/* prototypes */
static gboolean gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo
    * in_info, GstVideoInfo * out_info);
static GstFlowReturn gst_vpi_klt_tracker_transform_image (GstVpiFilter *
    filter, VPIStream stream, VPIImage in_image, VPIImage out_image);
static gboolean gst_vpi_klt_tracker_stop (GstBaseTransform * trans);
static void gst_vpi_klt_tracker_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vpi_klt_tracker_finalize (GObject * object);

enum
{
  PROP_0,
  PROP_BOX
};

enum
{
  FRAME,
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

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_AND_VPIIMAGE_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI KLT Tacker", "Filter/Video",
      "VPI based KLT feature tracker element.",
      "Jimena Salas <jimena.salas@ridgerun.com>");

  vpi_filter_class->start = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_start);
  vpi_filter_class->transform_image =
      GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_transform_image);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_vpi_klt_tracker_stop);
  gobject_class->set_property = gst_vpi_klt_tracker_set_property;
  gobject_class->get_property = gst_vpi_klt_tracker_get_property;
  gobject_class->finalize = gst_vpi_klt_tracker_finalize;

  g_object_class_install_property (gobject_class, PROP_BOX,
      gst_param_spec_array ("boxes",
          "Bounding Boxes",
          "Nx5 matrix where N is the number of bounding boxes. Each bounding "
          "box (<f, x, y, w, h>) contains the frame (f) on which it first appears"
          ", the x and y positions (x, y) of the box top left corner, and the "
          "width and height (w, h) of the bounding box.\n"
          "Usage example: <<0.0,613.0,332.0,23.0,23.0>,<0.0,790.0,376.0,41.0,22.0>>",
          gst_param_spec_array ("bounding-boxes", "boxes", "boxes",
              g_param_spec_double ("boxes-params", "params", "params",
                  DEFAULT_PROP_BOX_MIN, DEFAULT_PROP_BOX_MAX, DEFAULT_PROP_BOX,
                  (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
              (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)),
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_vpi_klt_tracker_init (GstVpiKltTracker * self)
{
  /* TODO: Expose these as properties */
  self->klt_params.numberOfIterationsScaling = 20;
  self->klt_params.nccThresholdUpdate = 0.8;
  self->klt_params.nccThresholdKill = 0.6;
  self->klt_params.nccThresholdStop = 1.0;
  self->klt_params.maxScaleChange = 0.2;
  self->klt_params.maxTranslationChange = 1.5;
  self->klt_params.trackingType = VPI_KLT_INVERSE_COMPOSITIONAL;

  self->box_frames = NULL;
  self->total_boxes = 0;
  self->frame_count = 0;
  self->box_count = 0;
}

static gboolean
gst_vpi_klt_tracker_start (GstVpiFilter * filter, GstVideoInfo * in_info,
    GstVideoInfo * out_info)
{
  GstVpiKltTracker *self = NULL;
  gboolean ret = TRUE;
  VPIStatus status = VPI_SUCCESS;
  guint width = 0;
  guint height = 0;
  GstVideoFormat format = 0;
  VPIArrayData array_data = { 0 };

  g_return_val_if_fail (filter, FALSE);
  g_return_val_if_fail (in_info, FALSE);
  g_return_val_if_fail (out_info, FALSE);

  self = GST_VPI_KLT_TRACKER (filter);

  GST_DEBUG_OBJECT (self, "start");

  width = GST_VIDEO_INFO_WIDTH (in_info);
  height = GST_VIDEO_INFO_HEIGHT (in_info);
  format = GST_VIDEO_INFO_FORMAT (in_info);

  if (0 == self->total_boxes) {
    GST_WARNING_OBJECT (self, "No bounding boxes provided.");
  }

  array_data.type = VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX;
  array_data.capacity = VPI_ARRAY_CAPACITY;
  array_data.size = 0;
  array_data.data = &self->input_box_array[0];

  status = vpiArrayCreateHostMemWrapper (&array_data, VPI_BACKEND_ALL,
      &self->input_box_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not wrap bounding boxes into VPIArray."), (NULL));
    ret = FALSE;
    goto out;
  }

  array_data.type = VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D;
  array_data.data = &self->input_trans_array[0];

  status = vpiArrayCreateHostMemWrapper (&array_data, VPI_BACKEND_ALL,
      &self->input_trans_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not wrap homographies into VPIArray."), (NULL));
    ret = FALSE;
    goto free_in_box_array;
  }

  status = vpiCreateKLTFeatureTracker (VPI_BACKEND_CUDA, width, height,
      gst_vpi_video_to_image_format (format), &self->klt);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create KLT tracker payload."), (NULL));
    ret = FALSE;
    goto free_in_trans_array;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_KLT_TRACKED_BOUNDING_BOX, VPI_BACKEND_ALL,
      &self->output_box_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output bounding box array."), (NULL));
    ret = FALSE;
    goto free_klt;
  }

  status = vpiArrayCreate (VPI_ARRAY_CAPACITY,
      VPI_ARRAY_TYPE_HOMOGRAPHY_TRANSFORM_2D, VPI_BACKEND_ALL,
      &self->output_trans_vpi_array);

  if (VPI_SUCCESS != status) {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
        ("Could not create output homographies array."), (NULL));
    ret = FALSE;
    goto free_out_box_array;
  }

  goto out;

free_out_box_array:
  vpiArrayDestroy (self->output_box_vpi_array);
  self->output_box_vpi_array = NULL;

free_klt:
  vpiPayloadDestroy (self->klt);
  self->klt = NULL;

free_in_trans_array:
  vpiArrayDestroy (self->input_trans_vpi_array);
  self->input_trans_vpi_array = NULL;

free_in_box_array:
  vpiArrayDestroy (self->input_box_vpi_array);
  self->input_box_vpi_array = NULL;

out:
  return ret;
}

static void
gst_vpi_klt_tracker_draw_box_data (GstVpiKltTracker * self, VPIImage image)
{
  VPIImageData vpi_image_data = { 0 };
  VPIArrayData box_data = { 0 };
  VPIArrayData trans_data = { 0 };
  guint8 *image_data = NULL;
  VPIKLTTrackedBoundingBox *box = NULL;
  VPIHomographyTransform2D *trans = NULL;
  guint stride = 0;
  guint b, i, j = 0;
  float x, y, h, w = 0;

  g_return_if_fail (self);
  g_return_if_fail (image);

  vpiImageLock (image, VPI_LOCK_READ_WRITE, &vpi_image_data);
  /* Supported formats only have one plane */
  stride = vpi_image_data.planes[0].pitchBytes;
  image_data = (guint8 *) vpi_image_data.planes[0].data;

  vpiArrayLock (self->input_box_vpi_array, VPI_LOCK_READ, &box_data);
  vpiArrayLock (self->input_trans_vpi_array, VPI_LOCK_READ, &trans_data);
  box = (VPIKLTTrackedBoundingBox *) box_data.data;
  trans = (VPIHomographyTransform2D *) trans_data.data;

  for (b = 0; b < self->box_count; b++) {
    if (box[b].trackingStatus == VALID_TRACKING) {
      x = box[b].bbox.xform.mat3[0][2] + trans[b].mat3[0][2];
      y = box[b].bbox.xform.mat3[1][2] + trans[b].mat3[1][2];
      w = box[b].bbox.width * box[b].bbox.xform.mat3[0][0] *
          trans[b].mat3[0][0];
      h = box[b].bbox.height * box[b].bbox.xform.mat3[1][1] *
          trans[b].mat3[1][1];

      for (i = y; i < y + h; i++) {
        for (j = x; j < x + w; j++) {
          image_data[i * stride + j] = WHITE;
        }
      }
    }
  }

  vpiImageUnlock (image);
  vpiArrayUnlock (self->input_box_vpi_array);
  vpiArrayUnlock (self->input_trans_vpi_array);
}

static guint
gst_vpi_klt_tracker_get_num_boxes_until_frame (GstVpiKltTracker * self,
    guint frame)
{
  guint num_boxes = 0;
  guint i = 0;

  g_return_val_if_fail (self, num_boxes);

  for (i = 0; i < self->total_boxes; i++) {
    if (self->box_frames[i] <= frame) {
      num_boxes++;
    }
  }
  return num_boxes;
}

static GstFlowReturn
gst_vpi_klt_tracker_transform_image (GstVpiFilter * filter, VPIStream stream,
    VPIImage in_image, VPIImage out_image)
{
  GstVpiKltTracker *self = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  VPIArrayData updated_box_data = { 0 };
  VPIArrayData updated_trans_data = { 0 };
  VPIKLTTrackedBoundingBox *updated_box = NULL;
  VPIHomographyTransform2D *updated_trans = NULL;
  VPIStatus status = VPI_SUCCESS;
  guint current_num_boxes = 0;
  guint i = 0;

  g_return_val_if_fail (filter, GST_FLOW_ERROR);
  g_return_val_if_fail (stream, GST_FLOW_ERROR);
  g_return_val_if_fail (in_image, GST_FLOW_ERROR);
  g_return_val_if_fail (out_image, GST_FLOW_ERROR);

  self = GST_VPI_KLT_TRACKER (filter);

  GST_LOG_OBJECT (self, "Transform image");

  /* Quick way for copying the in image contents */
  vpiSubmitRescale (stream, VPI_BACKEND_CUDA, in_image, out_image,
      VPI_INTERP_LINEAR_FAST, VPI_BOUNDARY_COND_ZERO);
  vpiStreamSync (stream);

  if (self->frame_count == 0) {
    GST_DEBUG_OBJECT (self, "Setting first frame");

  } else {
    current_num_boxes =
        gst_vpi_klt_tracker_get_num_boxes_until_frame (self,
        self->frame_count - 1);

    /* New bounding boxes in this frame */
    if (self->box_count != current_num_boxes) {
      vpiArrayLock (self->input_box_vpi_array, VPI_LOCK_READ_WRITE, NULL);
      vpiArraySetSize (self->input_box_vpi_array, current_num_boxes);
      vpiArrayUnlock (self->input_box_vpi_array);
      vpiArrayLock (self->input_trans_vpi_array, VPI_LOCK_READ_WRITE, NULL);
      vpiArraySetSize (self->input_trans_vpi_array, current_num_boxes);
      vpiArrayUnlock (self->input_trans_vpi_array);

      self->box_count = current_num_boxes;
    }

    gst_vpi_klt_tracker_draw_box_data (self, out_image);

    status =
        vpiSubmitKLTFeatureTracker (stream, self->klt, self->template_image,
        self->input_box_vpi_array, self->input_trans_vpi_array, in_image,
        self->output_box_vpi_array, self->output_trans_vpi_array,
        &self->klt_params);

    if (VPI_SUCCESS != status) {
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED,
          ("Could not predict the new bounding boxes."), (NULL));
      ret = GST_FLOW_ERROR;
      goto out;
    }

    vpiStreamSync (stream);

    vpiArrayLock (self->output_box_vpi_array, VPI_LOCK_READ, &updated_box_data);
    vpiArrayLock (self->output_trans_vpi_array, VPI_LOCK_READ,
        &updated_trans_data);
    updated_box = (VPIKLTTrackedBoundingBox *) updated_box_data.data;
    updated_trans = (VPIHomographyTransform2D *) updated_trans_data.data;

    /* Set the input for next frame */
    for (i = 0; i < self->box_count; i++) {
      self->input_box_array[i].trackingStatus = updated_box[i].trackingStatus;
      self->input_box_array[i].templateStatus = updated_box[i].templateStatus;

      /* Skip boxes that are not being tracked */
      if (VALID_TRACKING != updated_box[i].trackingStatus) {
        continue;
      }
      /* Must update template for this box ? */
      if (NEED_TEMPLATE_UPDATE == updated_box[i].templateStatus) {
        VPIHomographyTransform2D identity = { IDENTITY_TRANSFORM };

        /* Simple update approach */
        self->input_box_array[i] = updated_box[i];
        self->input_box_array[i].templateStatus = NEED_TEMPLATE_UPDATE;
        self->input_trans_array[i] = identity;
      } else {
        self->input_box_array[i].templateStatus = !NEED_TEMPLATE_UPDATE;
        self->input_trans_array[i] = updated_trans[i];
      }
    }

    vpiArrayUnlock (self->output_box_vpi_array);
    vpiArrayUnlock (self->output_trans_vpi_array);
    /* Force arrays to update because wrapped memory has been modifed */
    vpiArrayInvalidate (self->input_box_vpi_array);
    vpiArrayInvalidate (self->input_trans_vpi_array);
  }

  self->template_image = in_image;
  self->frame_count++;

out:
  return ret;
}

static gboolean
gst_vpi_klt_tracker_set_bounding_boxes (GstVpiKltTracker * self,
    const GValue * gst_array)
{
  const GValue *box = NULL;
  gboolean ret = TRUE;
  guint boxes = 0;
  guint params = 0;
  guint i = 0;
  float identity[3][3] = IDENTITY_TRANSFORM;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (gst_array, FALSE);

  boxes = gst_value_array_get_size (gst_array);
  params = gst_value_array_get_size (gst_value_array_get_value (gst_array, 0));

  if (NUM_BOX_PARAMS == params) {
    self->box_frames = g_malloc (boxes * sizeof (guint));

    for (i = 0; i < boxes; i++) {
      box = gst_value_array_get_value (gst_array, i);
      memcpy (&self->input_box_array[i].bbox.xform.mat3, &identity,
          sizeof (identity));
      self->box_frames[i] =
          g_value_get_double (gst_value_array_get_value (box, FRAME));
      self->input_box_array[i].bbox.xform.mat3[0][2] =
          g_value_get_double (gst_value_array_get_value (box, X_POS));
      self->input_box_array[i].bbox.xform.mat3[1][2] =
          g_value_get_double (gst_value_array_get_value (box, Y_POS));
      self->input_box_array[i].bbox.width =
          g_value_get_double (gst_value_array_get_value (box, WIDTH));
      self->input_box_array[i].bbox.height =
          g_value_get_double (gst_value_array_get_value (box, HEIGHT));
      self->input_box_array[i].trackingStatus = VALID_TRACKING;
      self->input_box_array[i].templateStatus = NEED_TEMPLATE_UPDATE;
      /* Identity transformation at the beginning */
      memcpy (&self->input_trans_array[i].mat3, &identity, sizeof (identity));
    }
    self->total_boxes = boxes;

  } else {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS,
        ("Bounding boxes must have 5 parameters. Received %d.", params),
        (NULL));
    ret = FALSE;
  }

  return ret;
}

static void
gst_vpi_klt_tracker_get_bounding_boxes (GstVpiKltTracker * self,
    GValue * gst_array)
{
  GValue box = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  guint i = 0;
  guint j = 0;

  g_return_if_fail (self);
  g_return_if_fail (gst_array);

  for (i = 0; i < self->total_boxes; i++) {

    guint params[NUM_BOX_PARAMS] = { self->box_frames[i],
      self->input_box_array[i].bbox.xform.mat3[0][2],
      self->input_box_array[i].bbox.xform.mat3[1][2],
      self->input_box_array[i].bbox.width,
      self->input_box_array[i].bbox.height
    };
    g_value_init (&box, GST_TYPE_ARRAY);

    for (j = 0; j < NUM_BOX_PARAMS; j++) {

      g_value_init (&value, G_TYPE_UINT);
      g_value_set_double (&value, params[j]);
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

  GST_DEBUG_OBJECT (self, "set_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOX:
      gst_vpi_klt_tracker_set_bounding_boxes (self, value);
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

  GST_DEBUG_OBJECT (self, "get_property");

  GST_OBJECT_LOCK (self);
  switch (property_id) {
    case PROP_0:
      break;
    case PROP_BOX:
      gst_vpi_klt_tracker_get_bounding_boxes (self, value);
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
  gboolean ret = TRUE;

  GST_BASE_TRANSFORM_CLASS (gst_vpi_klt_tracker_parent_class)->stop (trans);

  GST_DEBUG_OBJECT (self, "stop");

  vpiArrayDestroy (self->input_trans_vpi_array);
  self->input_trans_vpi_array = NULL;

  vpiArrayDestroy (self->input_box_vpi_array);
  self->input_box_vpi_array = NULL;

  vpiArrayDestroy (self->output_trans_vpi_array);
  self->output_trans_vpi_array = NULL;

  vpiArrayDestroy (self->output_box_vpi_array);
  self->output_box_vpi_array = NULL;

  vpiPayloadDestroy (self->klt);
  self->klt = NULL;

  return ret;
}

void
gst_vpi_klt_tracker_finalize (GObject * object)
{
  GstVpiKltTracker *self = GST_VPI_KLT_TRACKER (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_free (self->box_frames);

  G_OBJECT_CLASS (gst_vpi_klt_tracker_parent_class)->finalize (object);
}
