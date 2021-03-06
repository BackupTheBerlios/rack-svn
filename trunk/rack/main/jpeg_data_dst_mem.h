/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Marko Reimer     <reimer@l3s.de>
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#ifndef __JPEG_DATA_DST_MEM_H__
#define __JPEG_DATA_DST_MEM_H__

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <jpeglib.h>
#include <jerror.h>

#define OUTPUT_BUF_SIZE  4096

/* Expanded data destination object for stdio output */

typedef struct {
    struct jpeg_destination_mgr pub; /* public fields */

    char * outstream;          /* target stream */
    int    outstreamOffset;    /* offset into outstream */
    JOCTET * buffer;           /* start of buffer */
} jpeg_data_dst_mgr;

typedef jpeg_data_dst_mgr * jpeg_data_dst_ptr;


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

inline METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
    jpeg_data_dst_ptr dest = (jpeg_data_dst_ptr) cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                  OUTPUT_BUF_SIZE * sizeof(JOCTET));

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    dest->outstreamOffset = 0;
}
/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

inline METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
    jpeg_data_dst_ptr dest = (jpeg_data_dst_ptr) cinfo->dest;

//  if (JFWRITE(dest->outfile, dest->buffer, OUTPUT_BUF_SIZE) !=
//      (size_t) OUTPUT_BUF_SIZE)
    if (memcpy(dest->outstream + dest->outstreamOffset, dest->buffer, OUTPUT_BUF_SIZE) !=
        dest->outstream + dest->outstreamOffset)
        ERREXIT(cinfo, JERR_FILE_WRITE);

    dest->outstreamOffset      += OUTPUT_BUF_SIZE;
    dest->pub.next_output_byte  = dest->buffer;
    dest->pub.free_in_buffer    = OUTPUT_BUF_SIZE;

    return TRUE;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

inline METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
    jpeg_data_dst_ptr dest = (jpeg_data_dst_ptr) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0)
    {
//    if (JFWRITE(dest->outfile, dest->buffer, datacount) != datacount)
        if (memcpy(dest->outstream + dest->outstreamOffset, dest->buffer, datacount) !=
                                     dest->outstream + dest->outstreamOffset)
            ERREXIT(cinfo, JERR_FILE_WRITE);
        dest->outstreamOffset += datacount;
    }
//  fflush(dest->outfile);
    /* Make sure we wrote the output file OK */
//  if (ferror(dest->outfile))
//    ERREXIT(cinfo, JERR_FILE_WRITE);
}

/*
 * Prepare for output to a stdio stream.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing compression.
 */

inline GLOBAL(void) jpeg_stdmem_dest (j_compress_ptr cinfo, char * outstream)
{
    jpeg_data_dst_ptr dest;

    /* The destination object is made permanent so that multiple JPEG images
     * can be written to the same file without re-executing jpeg_stdio_dest.
     * This makes it dangerous to use this manager and a different destination
     * manager serially with the same JPEG object, because their private object
     * sizes may be different.  Caveat programmer.
     */
    if (cinfo->dest == NULL)
    {    /* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)
              (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
              sizeof(jpeg_data_dst_mgr));
    }

    dest = (jpeg_data_dst_ptr) cinfo->dest;
    dest->pub.init_destination    = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination    = term_destination;
    dest->outstream               = outstream;
    dest->outstreamOffset         = 0;
}

#ifdef __cplusplus
}
#endif

#endif // __JPEG_DATA_DST_MEM_H__
