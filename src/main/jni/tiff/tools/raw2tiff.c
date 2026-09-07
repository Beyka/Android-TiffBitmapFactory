/*
 * Project:  libtiff tools
 * Purpose:  Convert raw byte sequences in TIFF images
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "libport.h"
#include "tif_config.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#include "tiffio.h"
#include "tiffiop.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef enum
{
    PIXEL,
    BAND
} InterleavingType;

static uint16_t compression = (uint16_t)-1;
static int jpegcolormode = JPEGCOLORMODE_RGB;
static int quality = 75; /* JPEG quality */
static uint16_t predictor = 0;

static void swapBytesInScanline(void *, uint32_t, TIFFDataType);
static int guessSize(int, TIFFDataType, _TIFF_off_t, uint32_t, int, uint32_t *,
                     uint32_t *);
static double correlation(void *, void *, uint32_t, TIFFDataType);
static void usage(int);
static int processCompressOptions(char *);

int main(int argc, char *argv[])
{
    uint32_t width = 0, length = 0, linebytes, bufsize;
    uint32_t nbands = 1;      /* number of bands in input image*/
    _TIFF_off_t hdr_size = 0; /* size of the header to skip */
    TIFFDataType dtype = TIFF_BYTE;
    int16_t depth = 1;                     /* bytes per pixel in input image */
    int swab = 0;                          /* byte swapping flag */
    InterleavingType interleaving = PIXEL; /* interleaving type flag */
    uint32_t rowsperstrip = (uint32_t)-1;
    uint16_t photometric = PHOTOMETRIC_MINISBLACK;
    uint16_t config = PLANARCONFIG_CONTIG;
    uint16_t fillorder = FILLORDER_LSB2MSB;
    int fd;
    char *outfilename = NULL;
    TIFF *out;
    uint32_t row, col, band;
    int c;
    int row_ok;
    uint64_t depth64;
    unsigned char *buf = NULL, *buf1 = NULL;
#if !HAVE_DECL_OPTARG
    extern int optind;
    extern char *optarg;
#endif

    while ((c = getopt(argc, argv, "c:r:H:w:l:b:d:LMp:si:o:h")) != -1)
    {
        switch (c)
        {
            case 'c': /* compression scheme */
                if (!processCompressOptions(optarg))
                    usage(EXIT_FAILURE);
                break;
            case 'r': /* rows/strip */
                rowsperstrip = (uint32_t)atoi(optarg);
                break;
            case 'H': /* size of input image file header */
                hdr_size = atoi(optarg);
                break;
            case 'w': /* input image width */
                width = (uint32_t)atoi(optarg);
                break;
            case 'l': /* input image length */
                length = (uint32_t)atoi(optarg);
                break;
            case 'b': /* number of bands in input image */
                nbands = (uint32_t)atoi(optarg);
                break;
            case 'd': /* type of samples in input image */
                if (strncmp(optarg, "byte", 4) == 0)
                    dtype = TIFF_BYTE;
                else if (strncmp(optarg, "short", 5) == 0)
                    dtype = TIFF_SHORT;
                else if (strncmp(optarg, "long", 4) == 0)
                    dtype = TIFF_LONG;
                else if (strncmp(optarg, "sbyte", 5) == 0)
                    dtype = TIFF_SBYTE;
                else if (strncmp(optarg, "sshort", 6) == 0)
                    dtype = TIFF_SSHORT;
                else if (strncmp(optarg, "slong", 5) == 0)
                    dtype = TIFF_SLONG;
                else if (strncmp(optarg, "float", 5) == 0)
                    dtype = TIFF_FLOAT;
                else if (strncmp(optarg, "double", 6) == 0)
                    dtype = TIFF_DOUBLE;
                else
                    dtype = TIFF_BYTE;
                depth = (int16_t)TIFFDataWidth(dtype);
                break;
            case 'L': /* input has lsb-to-msb fillorder */
                fillorder = FILLORDER_LSB2MSB;
                break;
            case 'M': /* input has msb-to-lsb fillorder */
                fillorder = FILLORDER_MSB2LSB;
                break;
            case 'p': /* photometric interpretation */
                if (strncmp(optarg, "miniswhite", 10) == 0)
                    photometric = PHOTOMETRIC_MINISWHITE;
                else if (strncmp(optarg, "minisblack", 10) == 0)
                    photometric = PHOTOMETRIC_MINISBLACK;
                else if (strncmp(optarg, "rgb", 3) == 0)
                    photometric = PHOTOMETRIC_RGB;
                else if (strncmp(optarg, "cmyk", 4) == 0)
                    photometric = PHOTOMETRIC_SEPARATED;
                else if (strncmp(optarg, "ycbcr", 5) == 0)
                    photometric = PHOTOMETRIC_YCBCR;
                else if (strncmp(optarg, "cielab", 6) == 0)
                    photometric = PHOTOMETRIC_CIELAB;
                else if (strncmp(optarg, "icclab", 6) == 0)
                    photometric = PHOTOMETRIC_ICCLAB;
                else if (strncmp(optarg, "itulab", 6) == 0)
                    photometric = PHOTOMETRIC_ITULAB;
                else
                    photometric = PHOTOMETRIC_MINISBLACK;
                break;
            case 's': /* do we need to swap bytes? */
                swab = 1;
                break;
            case 'i': /* type of interleaving */
                if (strncmp(optarg, "pixel", 4) == 0)
                    interleaving = PIXEL;
                else if (strncmp(optarg, "band", 6) == 0)
                    interleaving = BAND;
                else
                    interleaving = PIXEL;
                break;
            case 'o':
                outfilename = optarg;
                break;
            case 'h':
                usage(EXIT_SUCCESS);
            default:
                break;
        }
    }

    if (argc - optind < 2)
        usage(EXIT_FAILURE);

    fd = open(argv[optind], O_RDONLY | O_BINARY, 0);
    if (fd < 0)
    {
        fprintf(stderr, "%s: %s: Cannot open input file.\n", argv[0],
                argv[optind]);
        return (EXIT_FAILURE);
    }

    if (nbands == 0)
    {
        fprintf(stderr, "The number of bands is illegal.\n");
        close(fd);
        return (-1);
    }

    if (guessSize(fd, dtype, hdr_size, nbands, swab, &width, &length) < 0)
    {
        close(fd);
        return EXIT_FAILURE;
    }

    /* check for integer overflow in */
    /* hdr_size + (*width) * (*length) * nbands * depth */

    if ((width == 0) || (length == 0))
    {
        fprintf(stderr, "Too large nbands value specified.\n");
        close(fd);
        return (EXIT_FAILURE);
    }

    if (hdr_size < 0)
    {
        fprintf(stderr, "Invalid header size specified.\n");
        close(fd);
        return (EXIT_FAILURE);
    }

    if (depth <= 0)
    {
        fprintf(stderr, "Too large nbands value specified.\n");
        close(fd);
        return (EXIT_FAILURE);
    }
    depth64 = (uint64_t)depth;

    {
        uint64_t image_size64 =
            _TIFFMultiply64(NULL, depth64, nbands, "input image size");
        uint64_t total_size64;
        image_size64 =
            _TIFFMultiply64(NULL, image_size64, length, "input image size");
        image_size64 =
            _TIFFMultiply64(NULL, image_size64, width, "input image size");
        total_size64 = _TIFFAdd64(NULL, (uint64_t)hdr_size, image_size64,
                                  "input image size");
        if (image_size64 == 0 || total_size64 == 0 || total_size64 > UINT_MAX)
        {
            fprintf(stderr, "Too large image size specified.\n");
            close(fd);
            return (EXIT_FAILURE);
        }
    }

    if (outfilename == NULL)
        outfilename = argv[optind + 1];
    out = TIFFOpen(outfilename, "w");
    if (out == NULL)
    {
        fprintf(stderr, "%s: %s: Cannot open file for output.\n", argv[0],
                outfilename);
        close(fd);
        return (EXIT_FAILURE);
    }
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, length);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, nbands);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, depth * 8);
    TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, config);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
    switch (dtype)
    {
        case TIFF_BYTE:
        case TIFF_SHORT:
        case TIFF_LONG:
            TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
            break;
        case TIFF_SBYTE:
        case TIFF_SSHORT:
        case TIFF_SLONG:
            TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_INT);
            break;
        case TIFF_FLOAT:
        case TIFF_DOUBLE:
            TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
            break;
        case TIFF_NOTYPE:
        case TIFF_ASCII:
        case TIFF_RATIONAL:
        case TIFF_UNDEFINED:
        case TIFF_SRATIONAL:
        case TIFF_IFD:
        case TIFF_LONG8:
        case TIFF_SLONG8:
        case TIFF_IFD8:
        default:
            TIFFSetField(out, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_VOID);
            break;
    }
    if (compression == (uint16_t)-1)
        compression = COMPRESSION_PACKBITS;
    TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
    switch (compression)
    {
        case COMPRESSION_JPEG:
            if (photometric == PHOTOMETRIC_RGB &&
                jpegcolormode == JPEGCOLORMODE_RGB)
                photometric = PHOTOMETRIC_YCBCR;
            TIFFSetField(out, TIFFTAG_JPEGQUALITY, quality);
            TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, jpegcolormode);
            break;
        case COMPRESSION_LZW:
        case COMPRESSION_ADOBE_DEFLATE:
        case COMPRESSION_DEFLATE:
            if (predictor != 0)
                TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
            break;
        default:
            break;
    }
    /*
     * At this point depth * nbands * length * width has been checked
     * not to exceed UINT_MAX, and length is non-zero. Therefore the
     * per-scanline calculations below cannot overflow uint32_t.
     */
    switch (interleaving)
    {
        case BAND: /* band interleaved data */
        {
            uint64_t linebytes64 =
                _TIFFMultiply64(out, depth64, width, "scanline size");
            linebytes =
                _TIFFCastUInt64ToUInt32(out, linebytes64, "scanline size");
            if (linebytes64 == 0 || linebytes == 0)
            {
                fprintf(stderr, "Too large image size specified.\n");
                close(fd);
                TIFFClose(out);
                return (EXIT_FAILURE);
            }
            buf = (unsigned char *)_TIFFmalloc(linebytes);
            break;
        }
        case PIXEL: /* pixel interleaved data */
        default:
        {
            uint64_t linebytes64 =
                _TIFFMultiply64(out, depth64, width, "scanline size");
            linebytes64 =
                _TIFFMultiply64(out, linebytes64, nbands, "scanline size");
            linebytes =
                _TIFFCastUInt64ToUInt32(out, linebytes64, "scanline size");
            if (linebytes64 == 0 || linebytes == 0)
            {
                fprintf(stderr, "Too large image size specified.\n");
                close(fd);
                TIFFClose(out);
                return (EXIT_FAILURE);
            }
            break;
        }
    }
    {
        uint64_t bufsize64 =
            _TIFFMultiply64(out, depth64, width, "input buffer size");
        bufsize64 =
            _TIFFMultiply64(out, bufsize64, nbands, "input buffer size");
        bufsize = _TIFFCastUInt64ToUInt32(out, bufsize64, "input buffer size");
        if (bufsize64 == 0 || bufsize == 0)
        {
            fprintf(stderr, "Too large image size specified.\n");
            close(fd);
            TIFFClose(out);
            return (EXIT_FAILURE);
        }
    }
    buf1 = (unsigned char *)_TIFFmalloc(bufsize);

    rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip);
    if (rowsperstrip > length)
    {
        rowsperstrip = length;
    }
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);

    _TIFF_lseek_f(fd, (_TIFF_off_t)hdr_size,
                  SEEK_SET); /* Skip the file header */
    for (row = 0; row < length; row++)
    {
        row_ok = 1;
        switch (interleaving)
        {
            case BAND: /* band interleaved data */
                for (band = 0; band < nbands; band++)
                {
                    uint64_t band_rows64 =
                        _TIFFMultiply64(out, length, band, "band file offset");
                    uint64_t band_row64 =
                        _TIFFAdd64(out, band_rows64, row, "band file offset");
                    uint64_t data_offset64 = _TIFFMultiply64(
                        out, band_row64, linebytes, "band file offset");
                    uint64_t seek_offset64 =
                        _TIFFAdd64(out, (uint64_t)hdr_size, data_offset64,
                                   "band file offset");
                    if ((band_rows64 == 0 && length != 0 && band != 0) ||
                        (band_row64 == 0 && (band_rows64 != 0 || row != 0)) ||
                        (data_offset64 == 0 &&
                         (band_row64 != 0 && linebytes != 0)) ||
                        (seek_offset64 == 0 &&
                         ((uint64_t)hdr_size != 0 || data_offset64 != 0)))
                    {
                        fprintf(stderr, "Too large image size specified.\n");
                        row_ok = 0;
                        break;
                    }
                    if (_TIFF_lseek_f(fd, (_TIFF_off_t)seek_offset64,
                                      SEEK_SET) == (_TIFF_off_t)-1)
                    {
                        fprintf(stderr,
                                "%s: %s: scanline %" PRIu32 ": seek error.\n",
                                argv[0], argv[optind], row);
                        break;
                    }
                    if (read(fd, buf, linebytes) < 0)
                    {
                        fprintf(stderr,
                                "%s: %s: scanline %" PRIu32 ": Read error.\n",
                                argv[0], argv[optind], row);
                        break;
                    }
                    if (swab) /* Swap bytes if needed */
                        swapBytesInScanline(buf, width, dtype);
                    for (col = 0; col < width; col++)
                    {
                        uint64_t sample_index64 =
                            _TIFFMultiply64(out, col, nbands, "sample offset");
                        uint64_t dst_sample64 = _TIFFAdd64(
                            out, sample_index64, band, "sample offset");
                        uint64_t dst_offset64 = _TIFFMultiply64(
                            out, dst_sample64, depth64, "sample offset");
                        uint64_t src_offset64 =
                            _TIFFMultiply64(out, col, depth64, "sample offset");
                        tmsize_t dst_offset = _TIFFCastUInt64ToSSize(
                            out, dst_offset64, "sample offset");
                        tmsize_t src_offset = _TIFFCastUInt64ToSSize(
                            out, src_offset64, "sample offset");
                        if ((sample_index64 == 0 && col != 0 && nbands != 0) ||
                            (dst_sample64 == 0 &&
                             (sample_index64 != 0 || band != 0)) ||
                            (dst_offset64 == 0 &&
                             (dst_sample64 != 0 && depth64 != 0)) ||
                            (src_offset64 == 0 && col != 0 && depth64 != 0) ||
                            (dst_offset == 0 && dst_offset64 != 0) ||
                            (src_offset == 0 && src_offset64 != 0))
                        {
                            fprintf(stderr,
                                    "Too large image size specified.\n");
                            row_ok = 0;
                            break;
                        }
                        memcpy(buf1 + dst_offset, buf + src_offset,
                               (size_t)depth);
                    }
                    if (!row_ok)
                        break;
                }
                break;
            case PIXEL: /* pixel interleaved data */
            default:
                if (read(fd, buf1, bufsize) < 0)
                {
                    fprintf(stderr,
                            "%s: %s: scanline %" PRIu32 ": Read error.\n",
                            argv[0], argv[optind], row);
                    break;
                }
                if (swab) /* Swap bytes if needed */
                    swapBytesInScanline(buf1, width, dtype);
                break;
        }
        if (!row_ok)
        {
            close(fd);
            TIFFClose(out);
            return (EXIT_FAILURE);
        }

        if (TIFFWriteScanline(out, buf1, row, 0) < 0)
        {
            fprintf(stderr, "%s: %s: scanline %" PRIu32 ": Write error.\n",
                    argv[0], outfilename, row);
            break;
        }
    }
    if (buf)
        _TIFFfree(buf);
    if (buf1)
        _TIFFfree(buf1);
    TIFFClose(out);
    close(fd);
    return (EXIT_SUCCESS);
}

static void swapBytesInScanline(void *buf, uint32_t width, TIFFDataType dtype)
{
    switch (dtype)
    {
        case TIFF_SHORT:
        case TIFF_SSHORT:
            TIFFSwabArrayOfShort((uint16_t *)buf, (tmsize_t)width);
            break;
        case TIFF_LONG:
        case TIFF_SLONG:
            TIFFSwabArrayOfLong((uint32_t *)buf, (tmsize_t)width);
            break;
        /* case TIFF_FLOAT: */ /* FIXME */
        case TIFF_DOUBLE:
            TIFFSwabArrayOfDouble((double *)buf, (tmsize_t)width);
            break;
        case TIFF_NOTYPE:
        case TIFF_BYTE:
        case TIFF_ASCII:
        case TIFF_SBYTE:
        case TIFF_RATIONAL:
        case TIFF_UNDEFINED:
        case TIFF_SRATIONAL:
        case TIFF_FLOAT:
        case TIFF_IFD:
        case TIFF_LONG8:
        case TIFF_SLONG8:
        case TIFF_IFD8:
        default:
            break;
    }
}

static int guessSize(int fd, TIFFDataType dtype, _TIFF_off_t hdr_size,
                     uint32_t nbands, int swab, uint32_t *width,
                     uint32_t *length)
{
    const float longt = 40.0; /* maximum possible height/width ratio */
    char *buf1, *buf2;
    _TIFF_stat_s filestat;
    uint32_t w, h, scanlinesize, imagesize;
    uint32_t depth = (uint32_t)TIFFDataWidth(dtype);
    uint64_t depth64 = (uint64_t)depth;
    double cor_coef = 0, tmp;

    if (_TIFF_fstat_f(fd, &filestat) == -1)
    {
        fprintf(stderr, "Failed to obtain file size.\n");
        return -1;
    }

    if (filestat.st_size < hdr_size)
    {
        fprintf(stderr, "Too large header size specified.\n");
        return -1;
    }

    if (depth == 0)
    {
        fprintf(stderr, "Invalid sample data type.\n");
        return -1;
    }

    if (((filestat.st_size - hdr_size) / nbands / depth) > UINT32_MAX)
    {
        fprintf(stderr, "Too large image size calculated.\n");
        return -1;
    }
    else
        imagesize = (uint32_t)((filestat.st_size - hdr_size) / nbands / depth);

    if (*width != 0 && *length == 0)
    {
        fprintf(stderr, "Image height is not specified.\n");

        *length = imagesize / *width;

        fprintf(stderr, "Height is guessed as %" PRIu32 ".\n", *length);

        return 1;
    }
    else if (*width == 0 && *length != 0)
    {
        fprintf(stderr, "Image width is not specified.\n");

        *width = imagesize / *length;

        fprintf(stderr, "Width is guessed as %" PRIu32 ".\n", *width);

        return 1;
    }
    else if (*width == 0 && *length == 0)
    {
        unsigned int fail = 0;
        fprintf(stderr, "Image width and height are not specified.\n");
        w = (uint32_t)sqrt((double)imagesize / (double)longt);
        if (w == 0)
        {
            fprintf(stderr, "Too small image size.\n");
            return -1;
        }

        for (; w < (uint32_t)sqrt((double)imagesize * (double)longt); w++)
        {
            if (imagesize % w == 0)
            {
                {
                    uint64_t scanlinesize64 =
                        _TIFFMultiply64(NULL, w, depth64, "scanline size");
                    scanlinesize = _TIFFCastUInt64ToUInt32(NULL, scanlinesize64,
                                                           "scanline size");
                    if (scanlinesize64 == 0 || scanlinesize == 0)
                    {
                        fprintf(stderr, "scanline size overflow.\n");
                        fail = 1;
                        break;
                    }
                }
                h = imagesize / w;
                if (h < 2)
                    continue;
                /* reads 2 lines at the middle of the image and calculate their
                 * correlation. it works for h >= 2. (in this case it will
                 * compare line 0 and line 1 */
                buf1 = (char *)_TIFFmalloc(scanlinesize);
                buf2 = (char *)_TIFFmalloc(scanlinesize);
                do
                {
                    uint64_t seek_row = (uint32_t)((h - 1) / 2);
                    uint64_t data_offset64 = _TIFFMultiply64(
                        NULL, seek_row, scanlinesize, "scanline file offset");
                    uint64_t seek_offset64 =
                        _TIFFAdd64(NULL, (uint64_t)hdr_size, data_offset64,
                                   "scanline file offset");
                    if ((data_offset64 == 0 && seek_row != 0 &&
                         scanlinesize != 0) ||
                        (seek_offset64 == 0 &&
                         ((uint64_t)hdr_size != 0 || data_offset64 != 0)) ||
                        _TIFF_lseek_f(fd, (_TIFF_off_t)seek_offset64,
                                      SEEK_SET) == (_TIFF_off_t)-1)
                    {
                        fprintf(stderr, "seek error.\n");
                        fail = 1;
                        break;
                    }
                    /* read line (h-1)/2 */
                    if (read(fd, buf1, scanlinesize) != (long)scanlinesize)
                    {
                        fprintf(stderr, "read error.\n");
                        fail = 1;
                        break;
                    }
                    /* read line ((h-1)/2)+1 */
                    if (read(fd, buf2, scanlinesize) != (long)scanlinesize)
                    {
                        fprintf(stderr, "read error.\n");
                        fail = 1;
                        break;
                    }
                    if (swab)
                    {
                        swapBytesInScanline(buf1, w, dtype);
                        swapBytesInScanline(buf2, w, dtype);
                    }
                    if (0 == memcmp(buf1, buf2, scanlinesize))
                    {
                        *width = w, *length = h;
                    }
                    else
                    {
                        tmp = fabs(correlation(buf1, buf2, w, dtype));
                        if (tmp > cor_coef)
                        {
                            cor_coef = tmp;
                            *width = w, *length = h;
                        }
                    }
                } while (0);

                _TIFFfree(buf1);
                _TIFFfree(buf2);
            }
        }

        if (fail)
        {
            return -1;
        }

        fprintf(stderr,
                "Width is guessed as %" PRIu32 ", height is guessed as %" PRIu32
                ".\n",
                *width, *length);

        return 1;
    }
    else
    {
        uint64_t needed_size64 =
            _TIFFMultiply64(NULL, *width, *length, "input image size");
        needed_size64 =
            _TIFFMultiply64(NULL, needed_size64, nbands, "input image size");
        needed_size64 =
            _TIFFMultiply64(NULL, needed_size64, depth64, "input image size");
        needed_size64 = _TIFFAdd64(NULL, (uint64_t)hdr_size, needed_size64,
                                   "input image size");
        if (needed_size64 == 0 || (uint64_t)filestat.st_size < needed_size64)
        {
            fprintf(stderr, "Input file too small.\n");
            return -1;
        }
    }

    return 1;
}

/* Calculate correlation coefficient between two numeric vectors */
static double correlation(void *buf1, void *buf2, uint32_t n_elem,
                          TIFFDataType dtype)
{
    double X, Y, M1 = 0.0, M2 = 0.0, D1 = 0.0, D2 = 0.0, K = 0.0;
    uint32_t i;

    switch (dtype)
    {
        case TIFF_BYTE:
        case TIFF_NOTYPE:
        case TIFF_ASCII:
        case TIFF_RATIONAL:
        case TIFF_UNDEFINED:
        case TIFF_SRATIONAL:
        case TIFF_IFD:
        case TIFF_LONG8:
        case TIFF_SLONG8:
        case TIFF_IFD8:
        default:
            for (i = 0; i < n_elem; i++)
            {
                X = ((unsigned char *)buf1)[i];
                Y = ((unsigned char *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_SBYTE:
            for (i = 0; i < n_elem; i++)
            {
                X = ((signed char *)buf1)[i];
                Y = ((signed char *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_SHORT:
            for (i = 0; i < n_elem; i++)
            {
                X = ((uint16_t *)buf1)[i];
                Y = ((uint16_t *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_SSHORT:
            for (i = 0; i < n_elem; i++)
            {
                X = ((int16_t *)buf1)[i];
                Y = ((int16_t *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_LONG:
            for (i = 0; i < n_elem; i++)
            {
                X = ((uint32_t *)buf1)[i];
                Y = ((uint32_t *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_SLONG:
            for (i = 0; i < n_elem; i++)
            {
                X = ((int32_t *)buf1)[i];
                Y = ((int32_t *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_FLOAT:
            for (i = 0; i < n_elem; i++)
            {
                X = (double)((float *)buf1)[i];
                Y = (double)((float *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
        case TIFF_DOUBLE:
            for (i = 0; i < n_elem; i++)
            {
                X = ((double *)buf1)[i];
                Y = ((double *)buf2)[i];
                M1 += X, M2 += Y;
                D1 += X * X, D2 += Y * Y;
                K += X * Y;
            }
            break;
    }

    M1 /= n_elem;
    M2 /= n_elem;
    D1 -= M1 * M1 * n_elem;
    D2 -= M2 * M2 * n_elem;
    if (TIFF_DOUBLE_EQ(D1 * D2, 0.0))
        return 0.0; /* avoid divide by zero */
    K = (K - M1 * M2 * n_elem) / sqrt(D1 * D2);

    return K;
}

static int processCompressOptions(char *opt)
{
    if (strcmp(opt, "none") == 0)
        compression = COMPRESSION_NONE;
    else if (strcmp(opt, "packbits") == 0)
        compression = COMPRESSION_PACKBITS;
    else if (strncmp(opt, "jpeg", 4) == 0)
    {
        char *cp = strchr(opt, ':');

        compression = COMPRESSION_JPEG;
        while (cp)
        {
            if (isdigit((unsigned char)cp[1]))
                quality = atoi(cp + 1);
            else if (cp[1] == 'r')
                jpegcolormode = JPEGCOLORMODE_RAW;
            else
                usage(EXIT_FAILURE);

            cp = strchr(cp + 1, ':');
        }
    }
    else if (strncmp(opt, "lzw", 3) == 0)
    {
        char *cp = strchr(opt, ':');
        if (cp)
            predictor = (uint16_t)atoi(cp + 1);
        compression = COMPRESSION_LZW;
    }
    else if (strncmp(opt, "zip", 3) == 0)
    {
        char *cp = strchr(opt, ':');
        if (cp)
            predictor = (uint16_t)atoi(cp + 1);
        compression = COMPRESSION_ADOBE_DEFLATE;
    }
    else
        return (0);
    return (1);
}

static const char usage_info[] =
    "Create a TIFF file from raw data\n\n"
    "usage: raw2tiff [options] input.raw output.tif\n"
    "where options are:\n"
    " -L		input data has LSB2MSB bit order (default)\n"
    " -M		input data has MSB2LSB bit order\n"
    " -r #		make each strip have no more than # rows\n"
    " -H #		size of input image file header in bytes (0 by "
    "default)\n"
    " -w #		width of input image in pixels\n"
    " -l #		length of input image in lines\n"
    " -b #		number of bands in input image (1 by default)\n"
    "\n"
    " -d data_type	type of samples in input image\n"
    "where data_type may be:\n"
    " byte		8-bit unsigned integer (default)\n"
    " short		16-bit unsigned integer\n"
    " long		32-bit unsigned integer\n"
    " sbyte		8-bit signed integer\n"
    " sshort		16-bit signed integer\n"
    " slong		32-bit signed integer\n"
    " float		32-bit IEEE floating point\n"
    " double		64-bit IEEE floating point\n"
    "\n"
    " -p photo	photometric interpretation (color space) of the input image\n"
    "where photo may be:\n"
    " miniswhite	white color represented with 0 value\n"
    " minisblack	black color represented with 0 value (default)\n"
    " rgb		image has RGB color model\n"
    " cmyk		image has CMYK (separated) color model\n"
    " ycbcr		image has YCbCr color model\n"
    " cielab		image has CIE L*a*b color model\n"
    " icclab		image has ICC L*a*b color model\n"
    " itulab		image has ITU L*a*b color model\n"
    "\n"
    " -s		swap bytes fetched from input file\n"
    "\n"
    " -i config	type of samples interleaving in input image\n"
    "where config may be:\n"
    " pixel		pixel interleaved data (default)\n"
    " band		band interleaved data\n"
    "\n"
#ifdef LZW_SUPPORT
    " -c lzw[:opts]	compress output with Lempel-Ziv & Welch encoding\n"
    /* "    LZW options:\n" */
    "    #  set predictor value\n"
    "    For example, -c lzw:2 for LZW-encoded data with horizontal "
    "differencing\n"
#endif
#ifdef ZIP_SUPPORT
    " -c zip[:opts]	compress output with deflate encoding\n"
    /* "    Deflate (ZIP) options:\n" */
    "    #  set predictor value\n"
#endif
#ifdef JPEG_SUPPORT
    " -c jpeg[:opts]	compress output with JPEG encoding\n"
    /* "    JPEG options:\n" */
    "    #  set compression quality level (0-100, default 75)\n"
    "    r  output color image as RGB rather than YCbCr\n"
    "    For example, -c jpeg:r:50 for JPEG-encoded RGB data with 50% comp. "
    "quality\n"
#endif
#ifdef PACKBITS_SUPPORT
    " -c packbits	compress output with packbits encoding\n"
#endif
#if defined(LZW_SUPPORT) || defined(ZIP_SUPPORT) || defined(JPEG_SUPPORT) ||   \
    defined(PACKBITS_SUPPORT)
    " -c none	use no compression algorithm on output\n"
#endif
    "\n"
    " -o out.tif	write output to out.tif\n"
    " -h		this help message\n";

static void usage(int code)
{
    FILE *out = (code == EXIT_SUCCESS) ? stdout : stderr;

    fprintf(out, "%s\n\n", TIFFGetVersion());
    fprintf(out, "%s", usage_info);
    exit(code);
}
