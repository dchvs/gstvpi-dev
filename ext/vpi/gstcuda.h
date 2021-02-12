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

#ifndef __GST_CUDA_H__
#define __GST_CUDA_H__

#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

/**
 * GstCudaBuffer:
 *
 * An opaque structure representing a data buffer.
 */
typedef struct _GstCudaBuffer GstCudaBuffer;

/**
 * GstCudaMapper:
 *
 * An opaque structure representing a mapper to convert #GstBuffer to #GstCudaBuffer
 */
typedef struct _GstCudaMapper GstCudaMapper;

typedef struct _GstCudaChannel GstCudaChannel;
typedef struct _GstCudaData GstCudaData;

/**
 * GstCudaStream:
 * 
 * An opaque object representing a CUDA stream
 */
typedef struct _GstCudaStream GstCudaStream;

/**
 * GstCudaFormat:
 *
 * The colorspace format represented by a #GstCudaData
 * @GST_CUDA_I420: Planar YUV format with vertical and horizontal chroma subsampling
 * @GST_CUDA_RGBA: Interleaved RGB format with 8 bit per channel plus alpha component
 * @GST_CUDA_GREY: Greyscale with 8 bit per pixel
 */
typedef enum _GstCudaFormat
{
  GST_CUDA_I420,
  GST_CUDA_RGBA,
  GST_CUDA_GREY,
} GstCudaFormat;

/**
 * GstCudaChannel:
 * @data: A pointer to the plane data. This data can be directly consumed by CUDA.
 * @pitch: The width of the plane plus some HW dependent extra padding.
 * @width: The width of the plane.
 * @height: The height of the plane.
 *
 * The image size and data information for a single image plane. I.e.; for I420 a plane could be either Y, U or V.
 */
struct _GstCudaChannel
{
  gpointer data;
  gint32 pitch;
  gint32 width;
  gint32 height;
};

#define GST_CUDA_MAX_CHANNELS 4

/**
 * GstCudaData:
 * @channels: An array of planes specified as #GstCudaChannel.
 * @num_planes: The number of valid planes in channels
 * @stream: A pointer to the stream with the associated CudaData.
 *
 * Data information as an array of channels. The meaning of each channel depends on the application's
 * color space, i.e.: for I420 they will represent Y, U and V, respectively.
 */
struct _GstCudaData
{
  GstCudaChannel channels[GST_CUDA_MAX_CHANNELS];
  gint32 num_planes;
  GstCudaFormat format;
  GstCudaStream *stream;
};

/**
 * gst_cuda_mapper_new:
 *
 * Create a newly allocated GstCudaMapper. Free with gst_cuda_mapper_free() after usage.
 *
 * Returns: (transfer full): A newly allocated #GstCudaMapper.
 */
//GstCudaMapper *gst_cuda_mapper_new (void);

/**
 * gst_cuda_mapper_free:
 * @mapper: (in) (transfer none): A #GstCudaMapper object.
 *
 * Free a previously allocated gst_cuda_mapper_free().
 */
//void gst_cuda_mapper_free (GstCudaMapper * mapper);

/**
 * gst_cuda_mapper_map:
 * @mapper: (in) (transfer none): A #GstCudaMapper object.
 * @buf: (in) (transfer none): The #GstBuffer to take the data from.
 * @cuda_buf: (out) (transfer full): The resulting #GstCudaBuffer to be used on the GPU.
 *
 * Map a #GstBuffer into a #GstCudaBuffer to be consumed by the GPU. Unmap with gst_cuda_mapper_unmap() after usage.
 *
 * Returns: true if the mapping was successful, false otherwise.
 */
gboolean gst_cuda_mapper_map (GstCudaMapper * mapper, GstBuffer * buf,
    GstCudaBuffer ** cuda_buf);

/**
 * gst_cuda_mapper_unmap:
 * @mapper: (in) (transfer none): A #GstCudaMapper object.
 * @cuda_buf: (in) (transfer full): The #GstCudaBuffer to unmap.
 *
 * Unmap a previously mapped #GstCudaBuffer.
 *
 * Returns: true if the unmap was successful, false otherwise.
 */
//gboolean gst_cuda_mapper_unmap (GstCudaMapper * mapper,
 //   GstCudaBuffer * cuda_buf);

/**
 * gst_cuda_buffer_data:
 * @buffer: (in) (transfer none): The #GstCudaBuffer to query the data from.
 * @data: (out) (transfer none): The resulting associated to buffer.
 *
 * Query the pointer and data size information from a #GstCudaBuffer.
 */
//void gst_cuda_buffer_data (GstCudaBuffer * buffer, GstCudaData * data);

/**
 * gst_cuda_init:
 *
 * Initialize the GstCuda subsystem. This should be invoked before any other API call.
 *
 * Returns: true if initialization was successful, false otherwise.
 */
gboolean gst_cuda_init (void);

/**
 * gst_cuda_alloc:
 * @size: (in): The allocation size in bytes.
 *
 * Allocates a new CUDA buffer to be used by the GPU; this memory is also visible to the CPU.
 *
 * Free the CUDA buffer with gst_cuda_free().
 * 
 * Returns: (transfer full): A pointer to the allocated CUDA memory buffer.
 */
gpointer gst_cuda_alloc (gsize size);

/**
 *
 * gst_cuda_free:
 * @buffer: (in) (transfer full): A pointer to the CUDA buffer allocated with gst_cuda_alloc().
 *
 * Free a previously reserved CUDA buffer with gst_cuda_alloc().
 */
void gst_cuda_free (gpointer buffer);

/**
 *
 * gst_cuda_wrap:
 * @cuda_buf: (in) (transfer full): A pointer to the CUDA buffer allocated with gst_cuda_alloc().
 * @size: (in): size in bytes of the cuda_buf.
 *
 * Allocate a new GstBuffer that wraps the given CUDA buffer.
 * 
 * Free the buffer with gst_buffer_unref().
 * 
 * Returns: (transfer full): A GstBuffer that wraps the CUDA buffer.
 */
GstBuffer *gst_cuda_wrap (gpointer cuda_buf, gsize size);

/**
 * gst_cuda_data_from_info:
 * @data: (out) (transfer none): A #GstCudaData placeholder.
 * @info: (in) (transfer none): The #GstMapinfo to get the data from.
 * @caps: (in) (transfer none): The #GstVideoInfo associated to this buffer.
 *
 * Fills the #GstCudaData structure from the info and the given
 * caps.
 * 
 * Returns: True if the data was successfully retrieved, false otherwise.
 */
gboolean gst_cuda_data_from_info (GstCudaData * data, const GstMapInfo * info,
    const GstVideoInfo * caps);

/**
 * gst_cuda_gst_buffer_new:
 * @size: (in): size in bytes of the new #GstBuffer
 *
 * Create a new #GstBuffer and allocate size bytes for the data.
 * 
 * Free the buffer with gst_buffer_unref().
 * 
 * Returns: (transfer full): A newly allocated #GstBuffer.
 */
GstBuffer *gst_cuda_gst_buffer_new (gsize size);

G_END_DECLS
#endif // __GST_CUDA_H__
