/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "htp.h"
#include "htp_decompressors.h"

/**
 * Decompress a chunk of gzip-compressed data.
 *
 * @param drec
 * @param d
 */
static int htp_gzip_decompressor_decompress(htp_decompressor_gzip_t *drec, htp_tx_data_t *d) {
    size_t consumed = 0;

    // Return if we've previously had an error
    if (drec->initialized < 0) {
        return drec->initialized;
    }

    // Do we need to initialize?
    if (drec->initialized == 0) {
        // Check the header
        if ((drec->header_len == 0) && (d->len >= 10)) {
            // We have received enough data initialize; use the input buffer directly
            if ((d->data[0] != DEFLATE_MAGIC_1) || (d->data[1] != DEFLATE_MAGIC_2)) {
                htp_log(d->tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                    "GZip decompressor: Magic bytes mismatch");
                drec->initialized = -1;
                return -1;
            }

            if (d->data[3] != 0) {
                htp_log(d->tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                    "GZip decompressor: Unable to handle flags: %d", d->data[3]);
                drec->initialized = -1;
                return -1;
            }

            drec->initialized = 1;
            consumed = 10;
        } else {
            // We do not (or did not) have enough bytes, so we have
            // to copy some data into our internal header buffer.

            // How many bytes do we need?
            size_t copylen = 10 - drec->header_len;

            // Is there enough in input?
            if (copylen > d->len) copylen = d->len;

            // Copy the bytes
            memcpy(drec->header + drec->header_len, d->data, copylen);
            drec->header_len += copylen;
            consumed = copylen;

            // Do we have enough now?
            if (drec->header_len == 10) {
                // We do!
                if ((drec->header[0] != DEFLATE_MAGIC_1) || (drec->header[1] != DEFLATE_MAGIC_2)) {
                    htp_log(d->tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                        "GZip decompressor: Magic bytes mismatch");
                    drec->initialized = -1;
                    return -1;
                }

                if (drec->header[3] != 0) {
                    htp_log(d->tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                        "GZip decompressor: Unable to handle flags: %d", d->data[3]);
                    drec->initialized = -1;
                    return -1;
                }

                drec->initialized = 1;
            } else {
                // Need more data
                return 1;
            }
        }
    }

    // Decompress data
    int rc = 0;
    drec->stream.next_in = d->data + consumed;
    drec->stream.avail_in = d->len - consumed;

    while (drec->stream.avail_in != 0) {
        // If there's no more data left in the
        // buffer, send that information out
        if (drec->stream.avail_out == 0) {
            drec->crc = crc32(drec->crc, drec->buffer, GZIP_BUF_SIZE);

            // Prepare data for callback
            htp_tx_data_t d2;
            d2.tx = d->tx;
            d2.data = drec->buffer;
            d2.len = GZIP_BUF_SIZE;

            // Send decompressed data to callback
            if (drec->super.callback(&d2) < 0) {
                inflateEnd(&drec->stream);
                drec->zlib_initialized = 0;
                return -1;
            }

            drec->stream.next_out = drec->buffer;
            drec->stream.avail_out = GZIP_BUF_SIZE;
        }

        rc = inflate(&drec->stream, Z_NO_FLUSH);

        if (rc == Z_STREAM_END) {
            // How many bytes do we have?
            size_t len = GZIP_BUF_SIZE - drec->stream.avail_out;

            // Update CRC
            drec->crc = crc32(drec->crc, drec->buffer, len);

            // Prepare data for callback
            htp_tx_data_t d2;
            d2.tx = d->tx;
            d2.data = drec->buffer;
            d2.len = len;

            // Send decompressed data to callback
            if (drec->super.callback(&d2) < 0) {
                inflateEnd(&drec->stream);
                drec->zlib_initialized = 0;
                return -1;
            }

            // TODO Handle trailer

            return 1;
        }

        if (rc != Z_OK) {
            htp_log(d->tx->connp, HTP_LOG_MARK, HTP_LOG_WARNING, 0,
                    "GZip decompressor: inflate failed with %d", rc);

            inflateEnd(&drec->stream);
            drec->zlib_initialized = 0;

            return -1;
        }
    }

    return 1;
}

/**
 * Shut down gzip decompressor.
 *
 * @param drec
 */
static void htp_gzip_decompressor_destroy(htp_decompressor_gzip_t * drec) {
    if (drec == NULL) return;

    if (drec->zlib_initialized) {
        inflateEnd(&drec->stream);
        drec->zlib_initialized = 0;
    }

    free(drec->buffer);
    free(drec);
}

/**
 * Initialize gzip decompressor.
 *
 * @param connp
 */
htp_decompressor_t * htp_gzip_decompressor_create(htp_connp_t *connp, int format) {
    htp_decompressor_gzip_t *drec = calloc(1, sizeof (htp_decompressor_gzip_t));
    if (drec == NULL) return NULL;

    drec->super.decompress = (int (*)(htp_decompressor_t *, htp_tx_data_t *)) htp_gzip_decompressor_decompress;
    drec->super.destroy = (void (*)(htp_decompressor_t *))htp_gzip_decompressor_destroy;

    drec->buffer = malloc(GZIP_BUF_SIZE);
    if (drec->buffer == NULL) {
        free(drec);
        return NULL;
    }

    int rc = inflateInit2(&drec->stream, GZIP_WINDOW_SIZE);
    if (rc != Z_OK) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0,
            "GZip decompressor: inflateInit2 failed with code %d", rc);

        inflateEnd(&drec->stream);
        free(drec->buffer);
        free(drec);

        return NULL;
    }

    drec->zlib_initialized = 1;
    drec->stream.avail_out = GZIP_BUF_SIZE;
    drec->stream.next_out = drec->buffer;

    if (format == COMPRESSION_DEFLATE) {
        drec->initialized = 1;
    }

    return (htp_decompressor_t *) drec;
}
