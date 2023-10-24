// Copyright Epic Games, Inc. All Rights Reserved.
/*

OOJPEG - A JPEG Super-compressor using an oodle backend as the entropy coder.

Can encode from a JPEG file to an OOJPEG file, and can decode directly to a JPEG file or a raw image.
Can also encode raw images directly to OOJPEG files.

API has simple APIs and threaded APIs. 

The simple APIs is as follows:
-----------------------------

The following functions encode from a JPEG file to a OOJPEG file...

  int oojpeg_encode_jpeg_mem(const unsigned char *jpeg_data, int jpeg_data_size, unsigned char **out_oojpeg_data, int *out_oojpeg_size, int flags);

The following functions encode from a raw RGBA image to a OOJPEG file...

  int oojpeg_encode_to_mem(unsigned char **out_data, int *out_size, int width, int height, int channels, const unsigned char *rgba, int quality, int flags);

The following functions decode from a OOJPEG file back to a JPEG file...

  int oojpeg_decode_mem_to_jpeg(const unsigned char *data, int size, unsigned char **out_data, int *out_size, int flags);

The following functions decode from a OOJPEG file to a raw RGBA image...

  unsigned char *oojpeg_decode_mem(const unsigned char *data, int size, int *width, int *height, int *channels, int flags);

The threaded APIs is as follows:
-------------------------------

The following functions encode from a JPEG file to a OOJPEG file...

  oojpeg_encode_context_t oojpeg_encode_jpeg_mem_threaded_start(const unsigned char *data, int size, int flags);
  int oojpeg_encode_jpeg_thread_run(oojpeg_encode_context_t *context, int job_idx);
  int oojpeg_encode_jpeg_mem_threaded_finish(oojpeg_encode_context_t *context, unsigned char **out_data, int *out_size);

The following functions encode from a raw RGBA image to a OOJPEG file...

  oojpeg_encode_image_context_t oojpeg_encode_image_mem_threaded_start(int width, int height, int comp, const unsigned char *in_data, int quality, int flags);
  oojpeg_encode_image_context_t oojpeg_encode_image_threaded_start(const oojpeg_io_callbacks_t *io, void *io_user, int width, int height, int comp, const unsigned char *in_data, int quality, int flags);
  int oojpeg_encode_image_thread_run(oojpeg_encode_image_context_t *ctx, int job_idx);
  int oojpeg_encode_image_threaded_finish(oojpeg_encode_image_context_t *ctx);
  int oojpeg_encode_image_mem_threaded_finish(oojpeg_encode_image_context_t *ctx, unsigned char **out_data, int *out_size);

The following functions decode from a OOJPEG file to a raw RGBA image...

  oojpeg_decode_context_t oojpeg_decode_mem_threaded_start(const unsigned char *data, int size, int flags);
  oojpeg_decode_context_t oojpeg_decode_threaded_start(const oojpeg_io_callbacks_t *io, void *io_user, int flags);
  int oojpeg_decode_thread_run(oojpeg_decode_context_t *context, int job_idx);
  unsigned char *oojpeg_decode_threaded_finish(oojpeg_decode_context_t *context, int *out_width, int *out_height, int *out_comp);

You use the threaded APIs by first calling the _start function, which gives you a context. You then pass that context into the thread_run function N times,
where N is the value of the context.jobs_to_run variable. You then wait for all the threads to finish, and then call the _finish function to get the output data.

Example:

      oojpeg_encode_context_t ctx = oojpeg_encode_jpeg_mem_threaded_start(input, output, flags);
      #pragma omp parallel for
      for(int i = 0; i < ctx.jobs_to_run; ++i) {
        oojpeg_encode_jpeg_thread_run(&ctx, i);
      }
      oojpeg_encode_jpeg_file_threaded_finish(&ctx);

Other Notes:

* Custom Allocators can be set via oojpeg_set_alloc() function.
* Custom compression/decompression (must be set the same in both the encoder and decoder) can be set via oojpeg_set_compression() function.

*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
typedef enum {
  OOJPEG_ERR_NONE                = 0, // No Error, everything OK! :)
  OOJPEG_ERR_NOT_A_JPEG          = 1, // Not a JPEG file. Happens if you try to encode from a non-JPEG file.
  OOJPEG_ERR_CANNOT_OPEN_FILE    = 2, // Cannot open file. Happens if the file routines cannot open the files you specified!
  OOJPEG_ERR_NOT_OOJPEG_FILE     = 3, // Not a Oojpeg file. Happens if you try to decode a non-Oojpeg file.
  OOJPEG_ERR_CORRUPT_OOJPEG_FILE = 4, // Corrupt Oojpeg file. Happens if the Oojpeg file is corrupt.
  OOJPEG_ERR_WRITE_FAILED        = 5, // Write failed. Happens if the file routines cannot write to the files you specified!
  OOJPEG_ERR_OUT_OF_MEMORY       = 6, // Out of memory. Happens if the Oojpeg library cannot allocate enough memory.
  OOJPEG_ERR_UNSUPPORTED_COLORSPACE = 7, // Unsupported colorspace. Happens when exporting if the JPEG file is not YCbCr.
  OOJPEG_ERR_READ_FAILED         = 8, // Read failed. Happens if the file routines cannot read from the files you specified!
  OOJPEG_ERR_UNSUPPORTED         = 9, // Other Unsupported. Happens usually when you try to decode to jpeg in an uncommon format.
} oojpeg_error_t;

// Flags to be used in compression or decompression
typedef enum {
  OOJPEG_FLAG_NONE            = 0x0, // no flags
  OOJPEG_FLAG_NO_COMPRESSION  = 0x1, // don't compress the coefficients, just store them
  OOJPEG_FLAG_FASTDCT         = 0x2, // use a fast dct transform which matches libjpegturbo's fastdct.
} oojpeg_flags_t;

enum {
  OOJPEG_MAX_SPLITS = 16,
};

// IO callbacks
//  For use with oojpeg_*() to specify any input or output method. All methods internally use this!
typedef struct {
  size_t (*write)(void *user, const void *data, size_t size);  // write 'data' with 'size' bytes.  return number of bytes actually written
  size_t (*read)(void *user, void *data, size_t size);  // read 'data' with 'size' bytes.  return number of bytes actually read
  int (*eof)(void *user);  // return 1 if end-of-file, 0 otherwise
} oojpeg_io_callbacks_t;

// For using memory buffers as input or output.
typedef struct {
  char *base;
  char *ptr;
  size_t size;
} oojpeg_io_buffer_t;

extern const oojpeg_io_callbacks_t s_oojpeg_buffer_fns;

// For setting custom memory allocators.
typedef struct {
  void *(*alloc)(size_t size);  // allocate 'size' bytes.  return pointer to allocated memory
  void (*free)(void *ptr);  // free memory at 'ptr'
  void *(*realloc)(void *ptr, size_t size);  // reallocate 'ptr' to 'size' bytes.  return pointer to reallocated memory
} oojpeg_alloc_t;

typedef struct {
    void *data;
    size_t size;
} oojpeg_buffer_t;

// compression callbacks, for using custom compression methods, such as zlib
typedef struct {
  oojpeg_buffer_t (*compress)(const void *data, size_t size);
  int (*decompress)(const void *data, size_t size, void *out_data, size_t out_size);
} oojpeg_compression_t;

// Set custom allocators.  If not set, malloc/free/realloc will be used.
void oojpeg_set_alloc(const oojpeg_alloc_t *alloc);
// Set custom compression methods.  If not set, oodle compression methods will be used.
void oojpeg_set_compression(const oojpeg_compression_t *compression);

// An internal structure used to store the header of a Oojpeg file.
typedef struct oojpeg_ihdr_t {
  unsigned version;
  unsigned width;
  unsigned height;
  unsigned char bit_depth;
  unsigned char comp;
  unsigned char method;
  unsigned char num_splits;
} oojpeg_ihdr_t;

// An internal structure used to store the header of a Oojpeg file.
typedef struct oojpeg_lossy_hdr_t {
  unsigned short coef_dims[4][2];
  // Quantization tables
  unsigned short dequant[4][64];
  char app14_color_transform; // valid values are 0(CMYK),1(YCCK),2(YCbCrA)
  char jfif;
  char comp_id[4];
} oojpeg_lossy_hdr_t;

typedef struct {
  int y_start[4];
  int y_end[4];
  int width[4];
  int height[4];
  int num_blocks;
  short *xcoefs;
  char *split_xcoefs;
  unsigned char *oodle_buf;
  int oodle_size;
  int coefs_size;
} oojpeg_internal_split_t;

typedef struct {
  int error;
  int split_height_min;
  int split_height[4];
  int split_blocks[4];
  int total_split_blocks;
  oojpeg_internal_split_t splits[OOJPEG_MAX_SPLITS];
} oojpeg_internal_splits_t;

typedef struct {
  int error; // If there was an error, it is stored here.
  int jobs_to_run; // How many times to run oojpeg_decode_thread_run!

  // arguments
  const oojpeg_io_callbacks_t *io;
  void *io_user;
  int flags;

  // internal state
  int width, height, comp;
  int num_splits;

  oojpeg_lossy_hdr_t lhdr;
  oojpeg_internal_splits_t s;
} oojpeg_encode_context_t;

typedef struct {
  int error; // If there was an error, it is stored here.
  int jobs_to_run; // How many times to run oojpeg_decode_thread_run!

  // arguments
  int flags;

  // internal state
  oojpeg_ihdr_t ihdr;
  oojpeg_lossy_hdr_t lhdr;
  oojpeg_internal_splits_t spx;
  int yuva_stride[4];
  int yuva_height[4];
  unsigned char *yuva[4];
} oojpeg_decode_context_t;

typedef struct {
  int error; // If there was an error, it is stored here.
  int jobs_to_run; // How many times to run oojpeg_decode_thread_run!

  // arguments
  const oojpeg_io_callbacks_t *io;
  void *io_user;
  int width, height, comp;
  const unsigned char *in_data;
  int flags;

  // internal state
  int num_splits;
  oojpeg_internal_splits_t spx;
  oojpeg_lossy_hdr_t lhdr;
	float fdtbl_Y[64];
  float fdtbl_UV[64];
} oojpeg_encode_image_context_t;

// Encode a JPEG file to Oojpeg format.
// This second function operates totally in memory.
int oojpeg_encode_jpeg_mem(const unsigned char *jpeg_data, int jpeg_data_size, unsigned char **out_oojpeg_data, int *out_oojpeg_size, int flags);

// Threaded API
// In this API you first call start, and it does some synchronous work and gives you a state context to pass to the run function.
// Then inside threads, you call the run function with job_idx == 0 .. context.jobs_to_run
// Then you wait for all the threads to finish processing all the jobs.
// Then you call finish to get the output data.
oojpeg_encode_context_t oojpeg_encode_jpeg_mem_threaded_start(const unsigned char *data, int size, int flags);
int oojpeg_encode_jpeg_thread_run(oojpeg_encode_context_t *context, int job_idx);
int oojpeg_encode_jpeg_mem_threaded_finish(oojpeg_encode_context_t *context, unsigned char **out_data, int *out_size);

// Encode a raw image to a Oojpeg file.
int oojpeg_encode_to_mem(unsigned char **out_data, int *out_size, int width, int height, int channels, const unsigned char *rgba, int quality, int flags);

oojpeg_encode_image_context_t oojpeg_encode_image_mem_threaded_start(int width, int height, int comp, const unsigned char *in_data, int quality, int flags);
oojpeg_encode_image_context_t oojpeg_encode_image_threaded_start(const oojpeg_io_callbacks_t *io, void *io_user, int width, int height, int comp, const unsigned char *in_data, int quality, int flags);
int oojpeg_encode_image_thread_run(oojpeg_encode_image_context_t *ctx, int job_idx);
int oojpeg_encode_image_threaded_finish(oojpeg_encode_image_context_t *ctx);
int oojpeg_encode_image_mem_threaded_finish(oojpeg_encode_image_context_t *ctx, unsigned char **out_data, int *out_size);

// Decode a OOJPEG file to a raw image.
// Interface is like stbi_load_from_file, stbi_load_from_memory, stbi_load, etc.
unsigned char *oojpeg_decode_mem(const unsigned char *data, int size, int *width, int *height, int *channels, int flags);
unsigned char *oojpeg_decode(const oojpeg_io_callbacks_t *io, void *io_user, int *out_width, int *out_height, int *out_comp, int flags);

// Threaded API
// In this API you first call start, and it does some synchronous work and gives you a state context to pass to the run function.
// Then inside threads, you call the run function with job_idx == 0 .. context.jobs_to_run
// Then you wait for all the threads to finish processing all the jobs.
// Then you call finish to get the output data.

oojpeg_decode_context_t oojpeg_decode_mem_threaded_start(const unsigned char *data, int size, int flags);
oojpeg_decode_context_t oojpeg_decode_threaded_start(const oojpeg_io_callbacks_t *io, void *io_user, int flags);
int oojpeg_decode_thread_run(oojpeg_decode_context_t *context, int job_idx);
unsigned char *oojpeg_decode_threaded_finish(oojpeg_decode_context_t *context, int *out_width, int *out_height, int *out_comp);

// Decode a OOJPEG file to a JPEG file.
int oojpeg_decode_to_jpeg(const oojpeg_io_callbacks_t *in_io, void *in_io_user, const oojpeg_io_callbacks_t *out_io, void *out_io_user, int flags);
int oojpeg_decode_mem_to_jpeg(const unsigned char *data, int size, unsigned char **out_data, int *out_size, int flags);

#ifdef __cplusplus
}
#endif

