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

#include "gstvpiupload.h"

#include "cuda.h"
#include "cudaEGL.h"
#include <cuda_runtime.h>
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include <gst/video/video.h>
#include "nvbuf_utils.h"
#include "gstcuda.h"
#include <gst/gstbuffer.h>

#include "gst-libs/gst/vpi/gstvpibufferpool.h"
#include "gst-libs/gst/vpi/gstcudameta.h"
#include "gst-libs/gst/vpi/gstcudaallocator.h"

#include <vpi/Image.h>
#include "gst-libs/gst/vpi/gstvpimeta.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (gst_vpi_upload_debug_category);
#define GST_CAT_DEFAULT gst_vpi_upload_debug_category

#define VPI_SUPPORTED_FORMATS "{ GRAY8, GRAY16_BE, GRAY16_LE, NV12, RGB, RGBA, RGBx, BGR, BGRA, BGRx }"
#define VIDEO_CAPS GST_VIDEO_CAPS_MAKE(VPI_SUPPORTED_FORMATS)
#define VIDEO_AND_VPIIMAGE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VPIImage", VPI_SUPPORTED_FORMATS)
#define VIDEO_AND_NVMM_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:NVMM", VPI_SUPPORTED_FORMATS)

struct _GstVpiUpload
{
  GstBaseTransform parent;
  GstVideoInfo out_caps_info;
  GstVideoInfo in_caps_info;
  GstVpiBufferPool *upstream_buffer_pool;
  gboolean is_nvmm;
  EGLDisplay egl_display;
};

/* prototypes */
static gboolean gst_vpi_upload_is_nvmm (GstVpiUpload * self, GstCaps * caps);
static GstCaps *gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_vpi_upload_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_vpi_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static GstFlowReturn gst_vpi_upload_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static void gst_vpi_upload_finalize (GObject * object);

gboolean init_nvmm (GstVpiUpload * self);
gboolean process_nvmm (GstVpiUpload * self, GstBuffer * gst_buffer);


// 2 Delete
//gboolean gst_cuda_init (void);
static gboolean initialized = FALSE;
gboolean gst_cuda_format_from_egl (CUeglColorFormat eglfmt,
    GstCudaFormat * fmt);
void gst_vpi_image_free (gpointer data);
VPIImageFormat gst_vpi_video_to_image_format (GstVideoFormat video_format);
#define VPI_IMAGE_QUARK_STR "VPIImage"
GQuark _vpi_image_quark;
/*static*/ GstFlowReturn gst_vpi_filter_prepare_output_buffer (GstBaseTransform
    * trans, GstBuffer * input, GstBuffer ** outbuf);

enum
{
  PROP_0
};

static gboolean
gst_vpi_upload_is_nvmm (GstVpiUpload * self, GstCaps * caps)
{
  GstCapsFeatures *features = NULL;
  gboolean is_nvmm = FALSE;
  gint i = 0;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (caps, FALSE);

  for (i = 0; i < gst_caps_get_size (caps) && !is_nvmm; i++) {
    features = gst_caps_get_features (caps, i);
    is_nvmm = gst_caps_features_contains (features, "memory:NVMM");
  }

  return is_nvmm;
}

/* pad templates */
static GstStaticPadTemplate
    gst_vpi_upload_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_AND_VPIIMAGE_CAPS)
    );

static GstStaticPadTemplate
    gst_vpi_upload_sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS ";" VIDEO_AND_NVMM_CAPS)
    );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstVpiUpload, gst_vpi_upload, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_vpi_upload_debug_category, "vpiupload", 0,
        "debug category for vpiupload element"));

static void
gst_vpi_upload_class_init (GstVpiUploadClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_src_template);
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_vpi_upload_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "VPI Upload", "Filter/Video", "Uploads data into NVIDIA VPI",
      "Andres Campos <andres.campos@ridgerun.com>");

  base_transform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_transform_caps);
  base_transform_class->set_caps = GST_DEBUG_FUNCPTR (gst_vpi_upload_set_caps);
  base_transform_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_propose_allocation);
  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_vpi_upload_transform_ip);

//  base_transform_class->prepare_output_buffer =
//      GST_DEBUG_FUNCPTR (gst_vpi_filter_prepare_output_buffer);
  gobject_class->finalize = gst_vpi_upload_finalize;
}



// 2 Delete
gboolean
gst_cuda_init (void)
{
  CUresult status;
  CUdevice cuda_device;
  gint device_count;
  const gchar *error;
  gboolean ret = FALSE;

  if (initialized) {
    return TRUE;
  }
//  GST_DEBUG_CATEGORY_INIT (gst_cuda_debug_category, "cuda", 0,
//      "debug category for cuda utils");

  GST_INFO ("Initializing CUDA");

  status = cuInit (0);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Unable to initialize the CUDA driver API: %s", error);
    goto out;
  }

  status = cuDeviceGetCount (&device_count);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Unable to get the number of compute-capable devices: %s",
        error);
    goto out;
  }

  if (device_count == 0) {
    GST_ERROR ("Did not find any compute-capable devices");
    goto out;
  }

  status = cuDeviceGet (&cuda_device, 0);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Unable to get a handle to a compute device: %s", error);
    goto out;
  }

  initialized = TRUE;
  ret = TRUE;

out:
  return ret;
}


static void
gst_vpi_upload_init (GstVpiUpload * self)
{
  self->upstream_buffer_pool = NULL;

  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (self), TRUE);

  if (!gst_cuda_init ()) {
    GST_ERROR_OBJECT (self, "Unable to start CUDA subsystem");
    return;
  }
//  init_nvmm (self);
}

static GstCaps *
gst_vpi_upload_transform_downstream_caps (GstVpiUpload * self,
    GstCaps * caps_src)
{
  GstCapsFeatures *vpiimage_feature = NULL;
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  vpiimage_feature = gst_caps_features_from_string ("memory:VPIImage");

  for (i = 0; i < gst_caps_get_size (caps_src); i++) {
    /* Add VPIImage to all structures */
    gst_caps_set_features (caps_src, i,
        gst_caps_features_copy (vpiimage_feature));
  }

  gst_caps_features_free (vpiimage_feature);

  return caps_src;
}

static GstCaps *
gst_vpi_upload_transform_upstream_caps (GstVpiUpload * self, GstCaps * caps_src)
{
  GstCapsFeatures *nvmm_feature = NULL;
  GstCaps *featured_caps = NULL;
  gint i = 0;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (caps_src, NULL);

  featured_caps = gst_caps_copy (caps_src);
  nvmm_feature = gst_caps_features_from_string ("memory:NVMM");

  /* All the result caps are Linux/NVMM */
  for (i = 0; i < gst_caps_get_size (caps_src); i++) {
    /* Linux caps */
    gst_caps_set_features (caps_src, i, NULL);
    /* NVMM caps */
    gst_caps_set_features (featured_caps, i,
        gst_caps_features_copy (nvmm_feature));
  }

  caps_src = gst_caps_merge (featured_caps, caps_src);
  gst_caps_features_free (nvmm_feature);

  return caps_src;
}

static GstCaps *
gst_vpi_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  GstCaps *given_caps = NULL;
  GstCaps *ret = NULL;

  GST_DEBUG_OBJECT (self, "Transforming caps on %s:caps: %"
      GST_PTR_FORMAT "filter: %" GST_PTR_FORMAT,
      GST_PAD_SRC == direction ? "src" : "sink", caps, filter);

  given_caps = gst_caps_copy (caps);

  if (GST_PAD_SRC == direction) {
    /* transform caps going upstream */
    ret = gst_vpi_upload_transform_upstream_caps (self, given_caps);
  } else if (GST_PAD_SINK == direction) {
    /* transform caps going downstream */
    ret = gst_vpi_upload_transform_downstream_caps (self, given_caps);
  } else {
    /* unknown direction */
    GST_ERROR_OBJECT (self, "Cannot transform caps of unknown GstPadDirection");
    gst_caps_unref (given_caps);
    goto out;
  }

  if (filter) {
    GstCaps *tmp = ret;
    ret = gst_caps_intersect (filter, ret);
    gst_caps_unref (tmp);
  }

out:
  GST_DEBUG_OBJECT (self, "Transformed caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

static gboolean
gst_vpi_upload_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  gboolean ret = FALSE;

  ret = gst_video_info_from_caps (&self->in_caps_info, incaps);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to get the input caps");
    goto out;
  }

  self->is_nvmm = gst_vpi_upload_is_nvmm (self, incaps);

  ret = gst_video_info_from_caps (&self->out_caps_info, outcaps);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Unable to get the output caps");
    goto out;
  }

  ret = TRUE;

out:
  return ret;
}

static gboolean
gst_vpi_upload_create_buffer_pool (GstVpiUpload * self,
    GstVpiBufferPool * buffer_pool, GstQuery * query)
{
  gsize size = 0;
  GstStructure *config = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  gboolean need_pool = FALSE;
  gboolean ret = FALSE;

  /*GST_INFO_OBJECT */ g_print ("Proposing VPI upstream_buffer_pool\n");

  gst_query_parse_allocation (query, &caps, &need_pool);

  pool = GST_BUFFER_POOL (buffer_pool);
  size = self->in_caps_info.size;

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

  if (gst_buffer_pool_is_active (pool)) {
    GST_LOG_OBJECT (self, "Deactivating pool for setting config");
    gst_buffer_pool_set_active (pool, FALSE);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Unable to set pool configuration");
    goto out;
  }

  gst_query_add_allocation_pool (query,
      GST_BUFFER_POOL (buffer_pool), size, 3, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_CUDA_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VPI_META_API_TYPE, NULL);

  ret = TRUE;

out:
  return ret;
}

/* propose allocation query parameters for input buffers */
static gboolean
gst_vpi_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);

  GST_INFO_OBJECT (self, "Proposing upstream allocation");
  if (!self->upstream_buffer_pool) {
    self->upstream_buffer_pool = g_object_new (GST_VPI_TYPE_BUFFER_POOL, NULL);
  }

  return gst_vpi_upload_create_buffer_pool (self,
      self->upstream_buffer_pool, query);
}

static GstFlowReturn
gst_vpi_upload_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (trans);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMeta *meta = NULL;

  g_return_val_if_fail (buf, FALSE);

  if (self->is_nvmm) {
//      gst_base_transform_set_in_place (trans, FALSE);

    gst_base_transform_set_passthrough (trans, TRUE);

    ret = process_nvmm (self, buf);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Failed to process NVMMs");
      return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
  }

  meta = gst_buffer_get_meta (buf, GST_CUDA_META_API_TYPE);
  if (meta) {
    GST_LOG_OBJECT (self, "Received buffer through proposed allocation.");
  } else {
    GST_ERROR_OBJECT (self,
        "Cannot process buffers that do not use the proposed allocation.");
    ret = GST_FLOW_ERROR;
  }

  return ret;
}

/*static*/ GstFlowReturn
gst_vpi_filter_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * input, GstBuffer ** outbuf)
{
//  GstVpiFilterClass *klass = GST_VPI_FILTER_GET_CLASS (trans);
  GstFlowReturn ret = GST_FLOW_ERROR;

//  if (klass->transform_image_ip && gst_base_transform_is_passthrough (trans)) {
  if (!gst_buffer_is_writable (input)) {
    /* Create a subbuffer to allow subclasses to add metas. This wont
       actually copy the data, just the GstBuffer skeleton */
    *outbuf = gst_buffer_copy_region (input, GST_BUFFER_COPY_ALL, 0, -1);
  } else {
    *outbuf = input;
  }

//  } else {
//    ret =
//        GST_BASE_TRANSFORM_CLASS
//        (gst_vpi_filter_parent_class)->prepare_output_buffer (trans, input,
//        outbuf);
//  }

  return ret;
}

void
gst_vpi_upload_finalize (GObject * object)
{
  GstVpiUpload *self = GST_VPI_UPLOAD (object);

  GST_DEBUG_OBJECT (self, "Freeing resources");

  g_clear_object (&self->upstream_buffer_pool);

  //Delete the EGL Display
  if (self->egl_display && !eglTerminate (self->egl_display)) {
    GST_ERROR ("Failed to terminate EGL display connection");
  }

  G_OBJECT_CLASS (gst_vpi_upload_parent_class)->finalize (object);
}

gboolean
init_nvmm (GstVpiUpload * self)
{
  gboolean status = TRUE;

  self->egl_display = EGL_NO_DISPLAY;
  self->egl_display = eglGetDisplay (EGL_DEFAULT_DISPLAY);
  if (self->egl_display == EGL_NO_DISPLAY) {
    GST_ERROR ("Failed to get the EGL display");
    return FALSE;
  }

  /* Init the EGL display */
  if (!eglInitialize (self->egl_display, NULL, NULL)) {
    GST_ERROR ("Failed to initialize the EGL display");
    return FALSE;
  }

  return status;
}

gboolean
gst_cuda_format_from_egl (CUeglColorFormat eglfmt, GstCudaFormat * fmt)
{
  gboolean ret;

  g_return_val_if_fail (fmt, FALSE);

  ret = TRUE;

  switch (eglfmt) {
    case CU_EGL_COLOR_FORMAT_YUV420_PLANAR:
      *fmt = GST_CUDA_I420;
      break;
    case CU_EGL_COLOR_FORMAT_ABGR:
      *fmt = GST_CUDA_RGBA;
      break;
    default:
      ret = FALSE;
      break;
  }

  return ret;
}

void
gst_vpi_image_free (gpointer data)
{
  VPIImage image = (VPIImage) data;

  g_print ("Freeing VPI image %p\n", image);

  vpiImageDestroy (image);
}


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

/*const GstMetaInfo *
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
} */

gboolean
process_nvmm (GstVpiUpload * self, GstBuffer * gst_buffer)
{
  GstMapInfo info = GST_MAP_INFO_INIT;
  NvBufferParams params;
  EGLImageKHR *egl_image = NULL;
  const gchar *error = NULL;
  int status = 0;
  int fd = -1;

  CUeglFrame egl_frame;
  CUgraphicsResource resource;

  gint i;
  VPIImageData vpi_image_data = { 0 };
  GstVpiMeta *meta = NULL;
  GstMemory *mem = NULL;

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (initialized, FALSE);
  g_return_val_if_fail (gst_buffer, FALSE);

  cudaFree (0);

  meta =
      (GstVpiMeta *) gst_buffer_add_meta (gst_buffer, GST_VPI_META_INFO, NULL);
  meta->vpi_frame.buffer = gst_buffer_ref (gst_buffer);

  status = gst_buffer_map (gst_buffer, &info, GST_MAP_READ);
  if (status == FALSE) {
    GST_ERROR ("Failed to map the gst buffer");
    goto error;
  }
  /// NVMM from buffer

  cudaDeviceSynchronize ();
  status = cudaGetLastError ();
  g_assert (status == cudaSuccess);
  _vpi_image_quark = g_quark_from_static_string (VPI_IMAGE_QUARK_STR);

//  status = cuCtxSynchronize ();
//  if (status != CUDA_SUCCESS) {
//    cuGetErrorString (status, &error);
//    GST_ERROR ("Failed to synchronize before processing: %s", error);
//    goto nomap;
//  }

  // Get fd from NVMM hardware buffer
  status = ExtractFdFromNvBuffer ((void *) info.data, (int *) &fd);
  if (status != 0) {
    GST_ERROR ("Failed to extract the fd from NVMM hardware buffer");
    goto error;
  }

  status = NvBufferGetParams (fd, &params);
  if (status == 0) {
    g_print ("fd: %d\n", params.dmabuf_fd);
    g_print ("width[0]: %d\n", params.width[0]);
    g_print ("pitch[0]: %d\n", params.pitch[0]);
  } else {
    GST_ERROR ("Failed to get params from fd for NVMM");
    goto error;
  }
  ///**///

  /// EGL from fd
  // Create the image from buffer
  egl_image = NvEGLImageFromFd (NULL, fd);
  if (!egl_image) {
    GST_ERROR ("Failed to create EGL image from NVMM buffer");
    goto error;
  }
  // Register image to the GPU
  status = cuGraphicsEGLRegisterImage (&resource, egl_image,
      CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Failed to register EGL image: %s. Status=>%d", error, status);
    goto noregister;
  }

  status = cuGraphicsResourceGetMappedEglFrame (&egl_frame, resource, 0, 0);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Failed to map EGL frame: %s", error);
    goto nomap;
  }

  status = cuCtxSynchronize ();
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Failed to synchronize before processing: %s", error);
    goto nomap;
  }
  ///**///

  /// DATA
  vpi_image_data.type = gst_vpi_video_to_image_format (GST_VIDEO_INFO_FORMAT (&
          (self->in_caps_info)));
  vpi_image_data.numPlanes = GST_VIDEO_INFO_N_PLANES (&(self->in_caps_info));
//      egl_frame.planeCount >
//      GST_CUDA_MAX_CHANNELS ? GST_CUDA_MAX_CHANNELS : egl_frame.planeCount;

  for (i = 0; i < vpi_image_data.numPlanes; ++i) {
    vpi_image_data.planes[i].data = egl_frame.frame.pPitch[i];  /*info.data + GST_VIDEO_INFO_PLANE_OFFSET (&(self->in_caps_info), i); */
    vpi_image_data.planes[i].pitchBytes = params.pitch[i];
    vpi_image_data.planes[i].width = params.width[i];
    vpi_image_data.planes[i].height = params.height[i];

    g_print (">>>>>> pitch => %d\n", vpi_image_data.planes[i].pitchBytes);
  }

  status = vpiImageCreateCudaMemWrapper (&vpi_image_data, VPI_BACKEND_ALL,
      &(meta->vpi_frame.image));
  if (VPI_SUCCESS != status) {
    GST_ERROR ("Could not wrap buffer in VPIImage");
    g_print (">>>>>> %d", status);
//    return FALSE;
  } else {
    g_print (">>>>>> Wrap Cuda mem successfully");
  }

  mem = gst_buffer_get_all_memory (gst_buffer);
  gst_mini_object_set_qdata (GST_MINI_OBJECT_CAST (mem), _vpi_image_quark,
      meta->vpi_frame.image, gst_vpi_image_free);
  gst_memory_unref (mem);

  cudaDeviceSynchronize ();
  status = cudaGetLastError ();
  g_assert (status == cudaSuccess);
  ///**///

  /// Free resources
  status = cuGraphicsUnregisterResource (resource);
  if (status != CUDA_SUCCESS) {
    cuGetErrorString (status, &error);
    GST_ERROR ("Failed to free CUDA resources: %s", error);
  }

  NvDestroyEGLImage (NULL, egl_image);
//  gst_buffer_unmap (gst_buffer, &info);
  ///**///

  return TRUE;

noregister:
//  NvDestroyEGLImage (NULL, egl_image);

  goto error;

nomap:
  cuGraphicsUnregisterResource (resource);

  goto error;

error:
  if (gst_buffer) {
    gst_buffer_unmap (gst_buffer, &info);
  }

  if (error) {
    g_clear_pointer (&error, g_free);
  }

  NvDestroyEGLImage (NULL, egl_image);

  g_slice_free (GstMapInfo, &info);
//  g_clear_pointer (&self->egl_display, gst_buffer_unref);
//  g_clear_object (&self->egl_display);

  if (gst_buffer) {
    g_clear_pointer (&gst_buffer, gst_buffer_unref);
  }

  return FALSE;
}
