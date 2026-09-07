/*
 * Copyright (c) 1991-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "tiffio.h"
#include "tiffiop.h"

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define streq(a, b) (strcmp(a, b) == 0)
#define CopyField(tag, v)                                                      \
    if (TIFFGetField(in, tag, &v))                                             \
    TIFFSetField(out, tag, v)
#define CopyFieldFloat(tag, v)                                                 \
    if (TIFFGetField(in, tag, &v))                                             \
    TIFFSetField(out, tag, (double)(v))

#ifndef howmany
#define howmany(x, y) (((uint32_t)(x) + (uint32_t)(y) - 1U) / (uint32_t)(y))
#endif

#define LumaRed ycbcrCoeffs[0]
#define LumaGreen ycbcrCoeffs[1]
#define LumaBlue ycbcrCoeffs[2]

uint16_t compression = COMPRESSION_PACKBITS;
uint32_t rowsperstrip = (uint32_t)-1;

uint16_t horizSubSampling = 2; /* YCbCr horizontal subsampling */
uint16_t vertSubSampling = 2;  /* YCbCr vertical subsampling */
float ycbcrCoeffs[3] = {.299f, .587f, .114f};
/* default coding range is CCIR Rec 601-1 with no headroom/footroom */
float refBlackWhite[6] = {0.f, 255.f, 128.f, 255.f, 128.f, 255.f};

static int tiffcvt(TIFF *in, TIFF *out);
static void usage(int code);
static void setupLumaTables(void);

int main(int argc, char *argv[])
{
    TIFF *in, *out;
    int c;
#if !HAVE_DECL_OPTARG
    extern int optind;
    extern char *optarg;
#endif

    while ((c = getopt(argc, argv, "c:h:r:v:z")) != -1)
        switch (c)
        {
            case 'c':
                if (streq(optarg, "none"))
                    compression = COMPRESSION_NONE;
                else if (streq(optarg, "packbits"))
                    compression = COMPRESSION_PACKBITS;
                else if (streq(optarg, "lzw"))
                    compression = COMPRESSION_LZW;
                else if (streq(optarg, "jpeg"))
                    compression = COMPRESSION_JPEG;
                else if (streq(optarg, "zip"))
                    compression = COMPRESSION_ADOBE_DEFLATE;
                else
                    usage(EXIT_FAILURE);
                break;
            case 'h':
                horizSubSampling = (uint16_t)atoi(optarg);
                if (horizSubSampling != 1 && horizSubSampling != 2 &&
                    horizSubSampling != 4)
                    usage(EXIT_FAILURE);
                break;
            case 'v':
                vertSubSampling = (uint16_t)atoi(optarg);
                if (vertSubSampling != 1 && vertSubSampling != 2 &&
                    vertSubSampling != 4)
                    usage(EXIT_FAILURE);
                break;
            case 'r':
                rowsperstrip = (uint32_t)atoi(optarg);
                break;
            case 'z': /* CCIR Rec 601-1 w/ headroom/footroom */
                refBlackWhite[0] = 16.;
                refBlackWhite[1] = 235.;
                refBlackWhite[2] = 128.;
                refBlackWhite[3] = 240.;
                refBlackWhite[4] = 128.;
                refBlackWhite[5] = 240.;
                break;
            case '?':
                usage(EXIT_FAILURE);
                /*NOTREACHED*/
                break;
            default:
                break;
        }
    if (argc - optind < 2)
        usage(EXIT_FAILURE);
    out = TIFFOpen(argv[argc - 1], "w");
    if (out == NULL)
        return (EXIT_FAILURE);
    setupLumaTables();
    for (; optind < argc - 1; optind++)
    {
        in = TIFFOpen(argv[optind], "r");
        if (in != NULL)
        {
            do
            {
                if (!tiffcvt(in, out) || !TIFFWriteDirectory(out))
                {
                    TIFFClose(out);
                    TIFFClose(in);
                    return (1);
                }
            } while (TIFFReadDirectory(in));
            TIFFClose(in);
        }
    }
    TIFFClose(out);
    return (EXIT_SUCCESS);
}

float *lumaRed;
float *lumaGreen;
float *lumaBlue;
float D1, D2;
int Yzero;

static float *setupLuma(float c)
{
    float *v = (float *)_TIFFmalloc(256 * sizeof(float));
    int i;
    for (i = 0; i < 256; i++)
        v[i] = c * (float)i;
    return (v);
}

static unsigned V2Code(float f, float RB, float RW, int CR)
{
    unsigned int c = (unsigned int)((((f) * (RW - RB) / (float)CR) + RB) + .5f);
    return (c > 255 ? 255 : c);
}

static void setupLumaTables(void)
{
    lumaRed = setupLuma(LumaRed);
    lumaGreen = setupLuma(LumaGreen);
    lumaBlue = setupLuma(LumaBlue);
    D1 = 1.f / (2.f - 2.f * LumaBlue);
    D2 = 1.f / (2.f - 2.f * LumaRed);
    Yzero = (int)V2Code(0, refBlackWhite[0], refBlackWhite[1], 255);
}

static void cvtClump(unsigned char *op, uint32_t *raster, uint32_t ch,
                     uint32_t cw, uint32_t w)
{
    float Y, Cb = 0, Cr = 0;
    uint32_t j, k;
    /*
     * Convert ch-by-cw block of RGB
     * to YCbCr and sample accordingly.
     */
    for (k = 0; k < ch; k++)
    {
        for (j = 0; j < cw; j++)
        {
            uint32_t RGB = (raster - k * w)[j];
            Y = lumaRed[TIFFGetR(RGB)] + lumaGreen[TIFFGetG(RGB)] +
                lumaBlue[TIFFGetB(RGB)];
            /* accumulate chrominance */
            Cb += ((float)TIFFGetB(RGB) - Y) * D1;
            Cr += ((float)TIFFGetR(RGB) - Y) * D2;
            /* emit luminence */
            *op++ = (unsigned char)V2Code(Y, refBlackWhite[0], refBlackWhite[1],
                                          255);
        }
        for (; j < horizSubSampling; j++)
            *op++ = (unsigned char)Yzero;
    }
    for (; k < vertSubSampling; k++)
    {
        for (j = 0; j < horizSubSampling; j++)
            *op++ = (unsigned char)Yzero;
    }
    /* emit sampled chrominance values */
    *op++ = (unsigned char)V2Code(Cb / ((float)ch * (float)cw),
                                  refBlackWhite[2], refBlackWhite[3], 127);
    *op++ = (unsigned char)V2Code(Cr / ((float)ch * (float)cw),
                                  refBlackWhite[4], refBlackWhite[5], 127);
}
#undef LumaRed
#undef LumaGreen
#undef LumaBlue
#undef V2Code

/*
 * Convert a strip of RGB data to YCbCr and
 * sample to generate the output data.
 */
static int checkedRoundup32(TIFF *tif, uint32_t *result, uint32_t value,
                            uint16_t multiple, const char *where)
{
    uint64_t rounded64;
    uint32_t rounded32;

    if (multiple == 0)
        return 0;
    rounded64 = _TIFFAdd64(tif, value, (uint64_t)multiple - 1U, where);
    if (rounded64 == 0 && value != 0)
        return 0;
    rounded64 = _TIFFMultiply64(tif, rounded64 / multiple, multiple, where);
    rounded32 = _TIFFCastUInt64ToUInt32(tif, rounded64, where);
    if ((rounded64 == 0 && value != 0) || (rounded32 == 0 && rounded64 != 0))
        return 0;
    *result = rounded32;
    return 1;
}

static tmsize_t computeYCbCrStripSize(TIFF *tif, uint32_t rows, uint32_t width,
                                      const char *where)
{
    uint64_t luma64 = _TIFFMultiply64(tif, rows, width, where);
    uint64_t subsampling64 =
        _TIFFMultiply64(tif, horizSubSampling, vertSubSampling, where);
    uint64_t chroma_samples64;
    uint64_t chroma64;
    uint64_t total64;
    tmsize_t total;

    if ((luma64 == 0 && rows != 0 && width != 0) || subsampling64 == 0)
        return 0;
    chroma_samples64 = luma64 / subsampling64;
    chroma64 = _TIFFMultiply64(tif, chroma_samples64, 2, where);
    total64 = _TIFFAdd64(tif, luma64, chroma64, where);
    total = _TIFFCastUInt64ToSSize(tif, total64, where);
    if ((chroma64 == 0 && chroma_samples64 != 0) ||
        (total64 == 0 && (luma64 != 0 || chroma64 != 0)) ||
        (total == 0 && total64 != 0))
        return 0;
    return total;
}

static int cvtStrip(TIFF *tif, unsigned char *op, uint32_t *raster,
                    uint32_t nrows, uint32_t width)
{
    uint32_t x;
    int clumpSize = vertSubSampling * horizSubSampling + 2;
    uint32_t *tp;
    tmsize_t row_advance =
        _TIFFComputeRowOffset(tif, width, vertSubSampling, "raster row offset");

    if (row_advance == 0 && width != 0 && vertSubSampling != 0)
        return 0;

    for (; nrows >= vertSubSampling; nrows -= vertSubSampling)
    {
        tp = raster;
        for (x = width; x >= horizSubSampling; x -= horizSubSampling)
        {
            cvtClump(op, tp, vertSubSampling, horizSubSampling, width);
            op += clumpSize;
            tp += horizSubSampling;
        }
        if (x > 0)
        {
            cvtClump(op, tp, vertSubSampling, x, width);
            op += clumpSize;
        }
        raster -= row_advance;
    }
    if (nrows > 0)
    {
        tp = raster;
        for (x = width; x >= horizSubSampling; x -= horizSubSampling)
        {
            cvtClump(op, tp, nrows, horizSubSampling, width);
            op += clumpSize;
            tp += horizSubSampling;
        }
        if (x > 0)
            cvtClump(op, tp, nrows, x, width);
    }
    return 1;
}

static int cvtRaster(TIFF *tif, uint32_t *raster, uint32_t width,
                     uint32_t height)
{
    uint32_t y;
    tstrip_t strip = 0;
    tsize_t cc, acc;
    unsigned char *buf;
    uint32_t rwidth;
    uint32_t rheight;
    uint32_t nrows;
    uint32_t rnrows;

    if (!checkedRoundup32(tif, &rwidth, width, horizSubSampling,
                          "rounded raster width") ||
        !checkedRoundup32(tif, &rheight, height, vertSubSampling,
                          "rounded raster height"))
        return 0;
    nrows = (rowsperstrip > rheight ? rheight : rowsperstrip);
    if (nrows == 0)
        return 0;
    if (!checkedRoundup32(tif, &rnrows, nrows, vertSubSampling,
                          "rounded strip height"))
        return 0;

    cc = computeYCbCrStripSize(tif, rnrows, rwidth, "YCbCr strip size");
    if (cc == 0)
        return 0;
    buf = (unsigned char *)_TIFFmalloc(cc);
    // FIXME unchecked malloc
    for (y = height; (int32_t)y > 0; y -= nrows)
    {
        uint32_t nr = (y > nrows ? nrows : y);
        tmsize_t raster_offset =
            _TIFFComputeRowOffset(tif, width, y - 1, "raster row offset");
        if ((raster_offset == 0 && y != 1 && width != 0) ||
            !cvtStrip(tif, buf, raster + raster_offset, nr, width))
        {
            _TIFFfree(buf);
            return 0;
        }
        if (!checkedRoundup32(tif, &nr, nr, vertSubSampling,
                              "rounded strip height"))
        {
            _TIFFfree(buf);
            return 0;
        }
        acc = computeYCbCrStripSize(tif, nr, rwidth, "YCbCr strip size");
        if (acc == 0)
        {
            _TIFFfree(buf);
            return 0;
        }
        if (!TIFFWriteEncodedStrip(tif, strip++, buf, acc))
        {
            _TIFFfree(buf);
            return (0);
        }
    }
    _TIFFfree(buf);
    return (1);
}

static int tiffcvt(TIFF *in, TIFF *out)
{
    uint32_t width, height; /* image width & height */
    uint32_t *raster;       /* retrieve RGBA image */
    uint16_t shortv;
    float floatv;
    char *stringv;
    uint32_t longv;
    int result;
    uint64_t pixel_count64;
    tmsize_t pixel_count;

    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &height);
    pixel_count64 =
        _TIFFMultiply64(in, width, height, "raster buffer pixel count");
    pixel_count =
        _TIFFCastUInt64ToSSize(in, pixel_count64, "raster buffer pixel count");

    /* Check for integer overflow or implausibly large image dimensions. */
    if (!width || !height || pixel_count == 0 ||
        pixel_count > (tmsize_t)(INT32_MAX / sizeof(uint32_t)))
    {
        TIFFError(TIFFFileName(in),
                  "Malformed input file; "
                  "can't allocate buffer for raster of %" PRIu32 "x%" PRIu32
                  " size",
                  width, height);
        return 0;
    }

    raster = (uint32_t *)_TIFFCheckMalloc(in, pixel_count, sizeof(uint32_t),
                                          "raster buffer");
    if (raster == 0)
    {
        TIFFError(TIFFFileName(in),
                  "Failed to allocate buffer (%" TIFF_SSIZE_FORMAT
                  " elements of %" TIFF_SIZE_FORMAT " each)",
                  pixel_count, sizeof(uint32_t));
        return (0);
    }

    if (!TIFFReadRGBAImage(in, width, height, raster, 0))
    {
        _TIFFfree(raster);
        return (0);
    }

    CopyField(TIFFTAG_SUBFILETYPE, longv);
    TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
    if (compression == COMPRESSION_JPEG)
        TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RAW);
    CopyField(TIFFTAG_FILLORDER, shortv);
    TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 3);
    CopyFieldFloat(TIFFTAG_XRESOLUTION, floatv);
    CopyFieldFloat(TIFFTAG_YRESOLUTION, floatv);
    CopyField(TIFFTAG_RESOLUTIONUNIT, shortv);
    TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    {
        char buf[2048];
        const char *cp = strrchr(TIFFFileName(in), '/');
        snprintf(buf, sizeof(buf), "YCbCr conversion of %s",
                 cp ? cp + 1 : TIFFFileName(in));
        TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, buf);
    }
    TIFFSetField(out, TIFFTAG_SOFTWARE, TIFFGetVersion());
    CopyField(TIFFTAG_DOCUMENTNAME, stringv);

    TIFFSetField(out, TIFFTAG_REFERENCEBLACKWHITE, refBlackWhite);
    TIFFSetField(out, TIFFTAG_YCBCRSUBSAMPLING, horizSubSampling,
                 vertSubSampling);
    TIFFSetField(out, TIFFTAG_YCBCRPOSITIONING, YCBCRPOSITION_CENTERED);
    TIFFSetField(out, TIFFTAG_YCBCRCOEFFICIENTS, ycbcrCoeffs);
    rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip);
    TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);

    result = cvtRaster(out, raster, width, height);
    _TIFFfree(raster);
    return result;
}

const char *usage_info[] = {
    /* Help information format modified for the sake of consistency with the
       other tiff tools */
    /*    "usage: rgb2ycbcr [-c comp] [-r rows] [-h N] [-v N] input...
       output\n", */
    /*     "where comp is one of the following compression algorithms:\n", */
    "Convert RGB color, greyscale, or bi-level TIFF images to YCbCr images\n\n"
    "usage: rgb2ycbcr [options] input output",
    "where options are:",
#ifdef JPEG_SUPPORT
    " -c jpeg      JPEG encoding",
#endif
#ifdef ZIP_SUPPORT
    " -c zip       Zip/Deflate encoding",
#endif
#ifdef LZW_SUPPORT
    " -c lzw       Lempel-Ziv & Welch encoding",
#endif
#ifdef PACKBITS_SUPPORT
    " -c packbits  PackBits encoding (default)",
#endif
#if defined(JPEG_SUPPORT) || defined(LZW_SUPPORT) || defined(ZIP_SUPPORT) ||   \
    defined(PACKBITS_SUPPORT)
    " -c none      no compression",
#endif
    "",
    /*    "and the other options are:\n", */
    " -r   rows/strip", " -h   horizontal sampling factor (1,2,4)",
    " -v   vertical sampling factor (1,2,4)", NULL};

static void usage(int code)
{
    int i;
    FILE *out = (code == EXIT_SUCCESS) ? stdout : stderr;

    fprintf(out, "%s\n\n", TIFFGetVersion());
    for (i = 0; usage_info[i] != NULL; i++)
        fprintf(out, "%s\n", usage_info[i]);
    exit(code);
}
