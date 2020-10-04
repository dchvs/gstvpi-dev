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

#include "gstcudaallocator.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <nvbuf_utils.h>

/**
 * SECTION:gstcudaallocator
 * @short_description: GStreamer allocator for GstCuda based elements
 *
 * This class implements a GStreamer standard allocator for GstCuda
 * based elements. By using HW accelerated buffers in unified memory
 * space data is accessible in both the GPU and CPU. Furthermore, if
 * scheduled correctly, this allows for high performance zero memory
 * copy pipelines. 
 */

GST_DEBUG_CATEGORY_STATIC (gst_cuda_allocator_debug_category);
#define GST_CAT_DEFAULT gst_cuda_allocator_debug_category

struct _GstCudaAllocator
{
  GstAllocator base;
};

#define GST_CUDA_MEMORY_TYPE "CudaMemory"

typedef struct _GstCudaMemory GstCudaMemory;
struct _GstCudaMemory
{
  GstMemory base;
};

G_DEFINE_TYPE_WITH_CODE (GstCudaAllocator, gst_cuda_allocator,
    GST_TYPE_ALLOCATOR,
    GST_DEBUG_CATEGORY_INIT (gst_cuda_allocator_debug_category,
        "cudaallocator", 0, "debug category for cuda allocator class"));

/* prototypes */
static GstMemory *gst_cuda_allocator_alloc (GstAllocator * allocator,
    gsize size, GstAllocationParams * params);
static void gst_cuda_allocator_free (GstAllocator * allocator,
    GstMemory * memory);
static void gst_cuda_allocator_free_data (gpointer data);

static void
gst_cuda_allocator_class_init (GstCudaAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_cuda_allocator_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_cuda_allocator_free);
}

static void
gst_cuda_allocator_init (GstCudaAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR_CAST (self);

  GST_INFO_OBJECT (self, "New CUDA allocator");

  allocator->mem_type = GST_CUDA_MEMORY_TYPE;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static GstMemory *
gst_cuda_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstMemory *mem = NULL;
  gsize max;
  gsize alignm1;
  guint8 *data;
  guint8 *dataaligned;
  gsize offset;
  cudaError_t status;
  const gchar *errstr;

  g_return_val_if_fail (GST_CUDA_IS_ALLOCATOR (allocator), NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (params->prefix >= 0, NULL);
  g_return_val_if_fail (params->padding >= 0, NULL);
  g_return_val_if_fail (params->align >= 0, NULL);

  GST_LOG_OBJECT (allocator, "Allocating CUDA memory of size %" G_GSIZE_FORMAT,
      size);

  /* The "no alignment" case should really be align=1 and not align=0 */
  if (params->align > 0) {
    alignm1 = params->align - 1;
  } else {
    alignm1 = params->align;
  }

  max = size + params->prefix + params->padding + alignm1;

  status = cudaMallocManaged ((gpointer *) & data, max, cudaMemAttachHost);
  if (status != cudaSuccess) {
    errstr = cudaGetErrorString (status);
    GST_ERROR_OBJECT (allocator, "Unable to allocate unified CUDA memory: %s",
        errstr);
    goto out;
  }

  /* Since we can't ask cudaMallocManage for special alignments, we manually
     force alignment by request more mem if necessary and aligning the pointer
     to the required boundary */
  dataaligned =
      (guint8 *) (((gsize) (data + params->prefix + alignm1)) & ~alignm1);
  offset = dataaligned - data - params->prefix;

  GST_LOG_OBJECT (allocator,
      "Before alignment: %p, %" G_GSIZE_FORMAT "(prefix), %" G_GSIZE_FORMAT
      "(padding), %" G_GSIZE_FORMAT "(alignment)", data, params->prefix,
      params->padding, params->align);
  GST_LOG_OBJECT (allocator,
      "After alignment: %p, %" G_GSIZE_FORMAT "(prefix), %" G_GSIZE_FORMAT
      "(padding), %" G_GSIZE_FORMAT "(alignment), %" G_GSIZE_FORMAT "(offset)",
      dataaligned, params->prefix, params->padding, params->align, offset);

  mem =
      gst_memory_new_wrapped (params->flags, data, max, offset, size, data,
      gst_cuda_allocator_free_data);

out:
  return mem;
}

static void
gst_cuda_allocator_free (GstAllocator * allocator, GstMemory * memory)
{
  g_return_if_fail (allocator);
  g_return_if_fail (memory);

  GST_LOG ("Freeing CUDA memory");

  /* Fallback to the default allocator free since we created our memory via
     gst_memory_new_wrapped */
  gst_memory_unref (memory);
}

static void
gst_cuda_allocator_free_data (gpointer data)
{
  cudaError_t status;
  const gchar *errstr;

  g_return_if_fail (data);

  GST_LOG ("Freeing CUDA memory");

  status = cudaFree (data);
  if (status != cudaSuccess) {
    errstr = cudaGetErrorString (status);
    GST_ERROR ("Unable to free CUDA buffer: %s", errstr);
  }
}
