/******************************************************************************
 * tif_overview.c,v 1.9 2005/05/25 09:03:16 dron Exp
 *
 * Project:  TIFF Overview Builder
 * Purpose:  Library function for building overviews in a TIFF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * Notes:
 *  o Currently only images with bits_per_sample of a multiple of eight
 *    will work.
 *
 *  o The downsampler currently just takes the top left pixel from the
 *    source rectangle.  Eventually sampling options of averaging, mode, and
 *    ``center pixel'' should be offered.
 *
 *  o The code will attempt to use the same kind of compression,
 *    photometric interpretation, and organization as the source image, but
 *    it doesn't copy geotiff tags to the reduced resolution images.
 *
 *  o Reduced resolution overviews for multi-sample files will currently
 *    always be generated as PLANARCONFIG_SEPARATE.  This could be fixed
 *    reasonable easily if needed to improve compatibility with other
 *    packages.  Many don't properly support PLANARCONFIG_SEPARATE.
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */

/* TODO: update notes in header above */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tif_ovrcache.h"
#include "tiffio.h"

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#ifndef MAX
#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)
#endif

#define TIFF_DIR_MAX 65534

static int TIFFOverviewToSSize(uint64_t value, tmsize_t *out)
{
    if (value > (uint64_t)TIFF_TMSIZE_T_MAX)
        return 0;
    *out = (tmsize_t)value;
    return 1;
}

static int TIFFOverviewMulSSize(tmsize_t a, tmsize_t b, tmsize_t *out)
{
    if (a < 0 || b < 0)
        return 0;
    if (b != 0 && a > TIFF_TMSIZE_T_MAX / b)
        return 0;
    *out = a * b;
    return 1;
}

static int TIFFOverviewMul64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (b != 0 && a > UINT64_MAX / b)
        return 0;
    *out = a * b;
    return 1;
}

static int TIFFOverviewAdd64(uint64_t a, uint64_t b, uint64_t *out)
{
    if (a > UINT64_MAX - b)
        return 0;
    *out = a + b;
    return 1;
}

static int TIFFOverviewToSize(uint64_t value, size_t *out)
{
    if (value > (uint64_t)((size_t)-1))
        return 0;
    *out = (size_t)value;
    return 1;
}

static int TIFFOverviewPackedOffset(uint32_t y, uint32_t x, uint32_t hSub,
                                    uint32_t vSub, uint32_t sampleRowSize,
                                    uint32_t sampleBlockSize, size_t *out)
{
    uint64_t offset;
    uint64_t term;

    if (!TIFFOverviewMul64((uint64_t)(y / vSub), sampleRowSize, &offset))
        return 0;
    if (!TIFFOverviewMul64((uint64_t)(y % vSub), hSub, &term) ||
        !TIFFOverviewAdd64(offset, term, &offset))
        return 0;
    if (!TIFFOverviewMul64((uint64_t)(x / hSub), sampleBlockSize, &term) ||
        !TIFFOverviewAdd64(offset, term, &offset))
        return 0;
    if (!TIFFOverviewAdd64(offset, x % hSub, &offset))
        return 0;
    return TIFFOverviewToSize(offset, out);
}

static int TIFFOverviewSampleOffset(uint32_t y, uint32_t x,
                                    uint32_t sampleRowSize,
                                    uint32_t sampleBlockSize,
                                    uint32_t sampleOffset, size_t *out)
{
    uint64_t offset;
    uint64_t term;

    if (!TIFFOverviewMul64(y, sampleRowSize, &offset))
        return 0;
    if (!TIFFOverviewMul64(x, sampleBlockSize, &term) ||
        !TIFFOverviewAdd64(offset, term, &offset))
        return 0;
    if (!TIFFOverviewAdd64(offset, sampleOffset, &offset))
        return 0;
    return TIFFOverviewToSize(offset, out);
}

static int TIFFOverviewTileOffset(uint32_t srcOff, uint32_t ovrBlockOff,
                                  uint32_t ovrMult, uint32_t ovrBlockSize,
                                  uint32_t *tileOff)
{
    uint64_t blockSpan;
    uint64_t baseOff;

    if (ovrMult == 0)
        return 0;

    if (!TIFFOverviewMul64(ovrMult, ovrBlockSize, &blockSpan) ||
        !TIFFOverviewMul64(ovrBlockOff, blockSpan, &baseOff))
        return 0;

    if (baseOff > srcOff)
        return 0;

    *tileOff = (uint32_t)(((uint64_t)srcOff - baseOff) / ovrMult);
    return 1;
}

static int TIFFOverviewAdvanceUInt32(uint32_t *value, uint32_t step)
{
    if (step == 0 || *value > UINT32_MAX - step)
        return 0;
    *value += step;
    return 1;
}

static int TIFFOverviewHowMany64(uint64_t value, uint64_t divisor,
                                 uint64_t *out)
{
    if (divisor == 0)
        return 0;

    *out = value / divisor;
    if ((value % divisor) != 0)
    {
        if (*out == UINT64_MAX)
            return 0;
        (*out)++;
    }

    return 1;
}

/************************************************************************/
/*                         TIFF_WriteOverview()                         */
/*                                                                      */
/*      Create a new directory, without any image data for an overview. */
/*      Returns offset of newly created overview directory, but the     */
/*      current directory is reset to be the one in used when this      */
/*      function is called.                                             */
/************************************************************************/

uint32_t TIFF_WriteOverview(TIFF *hTIFF, uint32_t nXSize, uint32_t nYSize,
                            int nBitsPerPixel, int nPlanarConfig, int nSamples,
                            int nBlockXSize, int nBlockYSize, int bTiled,
                            int nCompressFlag, int nPhotometric,
                            int nSampleFormat, unsigned short *panRed,
                            unsigned short *panGreen, unsigned short *panBlue,
                            int bUseSubIFDs, int nHorSubsampling,
                            int nVerSubsampling)

{
    toff_t nBaseDirOffset;
    toff_t nOffset;
    tdir_t iNumDir;

    (void)bUseSubIFDs;

    nBaseDirOffset = TIFFCurrentDirOffset(hTIFF);

    TIFFCreateDirectory(hTIFF);

    /* -------------------------------------------------------------------- */
    /*      Setup TIFF fields.                                              */
    /* -------------------------------------------------------------------- */
    TIFFSetField(hTIFF, TIFFTAG_IMAGEWIDTH, nXSize);
    TIFFSetField(hTIFF, TIFFTAG_IMAGELENGTH, nYSize);
    if (nSamples == 1)
        TIFFSetField(hTIFF, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    else
        TIFFSetField(hTIFF, TIFFTAG_PLANARCONFIG, nPlanarConfig);

    TIFFSetField(hTIFF, TIFFTAG_BITSPERSAMPLE, nBitsPerPixel);
    TIFFSetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, nSamples);
    TIFFSetField(hTIFF, TIFFTAG_COMPRESSION, nCompressFlag);
    TIFFSetField(hTIFF, TIFFTAG_PHOTOMETRIC, nPhotometric);
    TIFFSetField(hTIFF, TIFFTAG_SAMPLEFORMAT, nSampleFormat);

    if (bTiled)
    {
        TIFFSetField(hTIFF, TIFFTAG_TILEWIDTH, nBlockXSize);
        TIFFSetField(hTIFF, TIFFTAG_TILELENGTH, nBlockYSize);
    }
    else
        TIFFSetField(hTIFF, TIFFTAG_ROWSPERSTRIP, nBlockYSize);

    TIFFSetField(hTIFF, TIFFTAG_SUBFILETYPE, FILETYPE_REDUCEDIMAGE);

    if (nPhotometric == PHOTOMETRIC_YCBCR || nPhotometric == PHOTOMETRIC_ITULAB)
    {
        TIFFSetField(hTIFF, TIFFTAG_YCBCRSUBSAMPLING, nHorSubsampling,
                     nVerSubsampling);
        /* TODO: also write YCbCrPositioning and YCbCrCoefficients tag identical
         * to source IFD */
    }
    /* TODO: add command-line parameter for selecting jpeg compression quality
     * that gets ignored when compression isn't jpeg */

    /* -------------------------------------------------------------------- */
    /*	Write color table if one is present.				*/
    /* -------------------------------------------------------------------- */
    if (panRed != NULL)
    {
        TIFFSetField(hTIFF, TIFFTAG_COLORMAP, panRed, panGreen, panBlue);
    }

    /* -------------------------------------------------------------------- */
    /*      Write directory, and return byte offset.                        */
    /* -------------------------------------------------------------------- */
    if (TIFFWriteCheck(hTIFF, bTiled, "TIFFBuildOverviews") == 0)
        return 0;

    if (!TIFFWriteDirectory(hTIFF))
        return 0;
    iNumDir = TIFFNumberOfDirectories(hTIFF);
    if (iNumDir > TIFF_DIR_MAX)
    {
        TIFFErrorExt(TIFFClientdata(hTIFF), "TIFF_WriteOverview",
                     "File `%s' has too many directories.\n",
                     TIFFFileName(hTIFF));
        exit(-1);
    }
    TIFFSetDirectory(hTIFF, (tdir_t)(iNumDir - 1));

    nOffset = TIFFCurrentDirOffset(hTIFF);
    if (nOffset > UINT32_MAX)
    {
        TIFFErrorExt(TIFFClientdata(hTIFF), "TIFF_WriteOverview",
                     "Overview directory offset exceeds classic TIFF range.");
        return 0;
    }

    TIFFSetSubDirectory(hTIFF, nBaseDirOffset);

    return (uint32_t)nOffset;
}

/************************************************************************/
/*                       TIFF_GetSourceSamples()                        */
/************************************************************************/

static void TIFF_GetSourceSamples(double *padfSamples, unsigned char *pabySrc,
                                  int nPixelBytes, int nSampleFormat,
                                  uint32_t nXSize, uint32_t nYSize,
                                  int nPixelOffset, int nLineOffset)
{
    uint32_t iXOff, iYOff;
    int iSample;

    iSample = 0;

    for (iYOff = 0; iYOff < nYSize; iYOff++)
    {
        for (iXOff = 0; iXOff < nXSize; iXOff++)
        {
            unsigned char *pabyData;

            pabyData = pabySrc + iYOff * (uint32_t)nLineOffset +
                       iXOff * (uint32_t)nPixelOffset;

            if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 1)
            {
                padfSamples[iSample++] = *pabyData;
            }
            else if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 2)
            {
                padfSamples[iSample++] = ((uint16_t *)pabyData)[0];
            }
            else if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 4)
            {
                padfSamples[iSample++] = ((uint32_t *)pabyData)[0];
            }
            else if (nSampleFormat == SAMPLEFORMAT_INT && nPixelBytes == 2)
            {
                padfSamples[iSample++] = ((int16_t *)pabyData)[0];
            }
            else if (nSampleFormat == SAMPLEFORMAT_INT && nPixelBytes == 32)
            {
                padfSamples[iSample++] = ((int32_t *)pabyData)[0];
            }
            else if (nSampleFormat == SAMPLEFORMAT_IEEEFP && nPixelBytes == 4)
            {
                padfSamples[iSample++] = (double)((float *)pabyData)[0];
            }
            else if (nSampleFormat == SAMPLEFORMAT_IEEEFP && nPixelBytes == 8)
            {
                padfSamples[iSample++] = ((double *)pabyData)[0];
            }
        }
    }
}

/************************************************************************/
/*                           TIFF_SetSample()                           */
/************************************************************************/

static void TIFF_SetSample(unsigned char *pabyData, int nPixelBytes,
                           int nSampleFormat, double dfValue)

{
    if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 1)
    {
        *pabyData = (unsigned char)MAX(0, MIN(255, dfValue));
    }
    else if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 2)
    {
        *((uint16_t *)pabyData) = (uint16_t)MAX(0, MIN(65535, dfValue));
    }
    else if (nSampleFormat == SAMPLEFORMAT_UINT && nPixelBytes == 4)
    {
        *((uint32_t *)pabyData) = (uint32_t)dfValue;
    }
    else if (nSampleFormat == SAMPLEFORMAT_INT && nPixelBytes == 2)
    {
        *((int16_t *)pabyData) = (int16_t)MAX(-32768, MIN(32767, dfValue));
    }
    else if (nSampleFormat == SAMPLEFORMAT_INT && nPixelBytes == 32)
    {
        *((int32_t *)pabyData) = (int32_t)dfValue;
    }
    else if (nSampleFormat == SAMPLEFORMAT_IEEEFP && nPixelBytes == 4)
    {
        *((float *)pabyData) = (float)dfValue;
    }
    else if (nSampleFormat == SAMPLEFORMAT_IEEEFP && nPixelBytes == 8)
    {
        *((double *)pabyData) = dfValue;
    }
}

/************************************************************************/
/*                          TIFF_DownSample()                           */
/*                                                                      */
/*      Down sample a tile of full res data into a window of a tile     */
/*      of downsampled data.                                            */
/************************************************************************/

static void TIFF_DownSample(unsigned char *pabySrcTile, uint32_t nBlockXSize,
                            uint32_t nBlockYSize, int nPixelSkewBits,
                            int nBitsPerPixel, unsigned char *pabyOTile,
                            uint32_t nOBlockXSize, uint32_t nOBlockYSize,
                            uint32_t nTXOff, uint32_t nTYOff, int nOMult,
                            int nSampleFormat, const char *pszResampling)

{
    uint32_t i, j;
    int k, nPixelBytes = (nBitsPerPixel) / 8;
    int nPixelGroupBytes = (nBitsPerPixel + nPixelSkewBits) / 8;
    int nLineOffset;
    unsigned char *pabySrc, *pabyDst;
    double *padfSamples;
    size_t nDstPixelStep;
    size_t padfSamples_count, padfSamples_size;
    uint32_t nXIterations, nYIterations;
    uint64_t nLineOffset64;

    assert(nBitsPerPixel >= 8);
    if (nOMult <= 0 || nPixelBytes <= 0 || nPixelGroupBytes <= 0)
        return;

    {
        uint64_t nXIterations64;
        uint64_t nYIterations64;

        if (!TIFFOverviewHowMany64(nBlockXSize, (uint32_t)nOMult,
                                   &nXIterations64) ||
            !TIFFOverviewHowMany64(nBlockYSize, (uint32_t)nOMult,
                                   &nYIterations64) ||
            nXIterations64 > UINT32_MAX || nYIterations64 > UINT32_MAX)
            return;

        nXIterations = (uint32_t)nXIterations64;
        nYIterations = (uint32_t)nYIterations64;
    }

    nLineOffset64 = (uint64_t)(uint32_t)nPixelGroupBytes * nBlockXSize;
    if (nLineOffset64 > INT_MAX)
        return;
    nLineOffset = (int)nLineOffset64;
    if ((size_t)nPixelBytes > ((size_t)-1) / (size_t)nPixelGroupBytes)
        return;
    nDstPixelStep = (size_t)nPixelBytes * (size_t)nPixelGroupBytes;

    /* sizeof(double) * nOMult * nOMult */
    if ((size_t)nOMult > ((size_t)-1) / (size_t)nOMult)
        return;
    padfSamples_count = (size_t)nOMult * (size_t)nOMult;
    if (padfSamples_count > ((size_t)-1) / sizeof(double))
    {
        /* TODO: This is an error condition */
        return;
    }
    padfSamples_size = padfSamples_count * sizeof(double);
    padfSamples = (double *)malloc(padfSamples_size);

    /* ==================================================================== */
    /*      Loop over scanline chunks to process, establishing where the    */
    /*      data is going.                                                  */
    /* ==================================================================== */
    for (j = 0; j < nYIterations; j++)
    {
        uint64_t dstOffset64;
        uint64_t dstY64;
        uint64_t srcY64 = (uint64_t)j * (uint32_t)nOMult;
        size_t dstOffset;
        uint32_t srcY;

        if (srcY64 >= nBlockYSize)
            break;
        srcY = (uint32_t)srcY64;

        dstY64 = (uint64_t)j + nTYOff;
        if (dstY64 >= nOBlockYSize || nTXOff >= nOBlockXSize)
            break;

        if (!TIFFOverviewMul64(dstY64, nOBlockXSize, &dstOffset64) ||
            !TIFFOverviewAdd64(dstOffset64, nTXOff, &dstOffset64) ||
            !TIFFOverviewMul64(dstOffset64, (uint64_t)nDstPixelStep,
                               &dstOffset64) ||
            !TIFFOverviewToSize(dstOffset64, &dstOffset))
            break;
        pabyDst = pabyOTile + dstOffset;

        /* --------------------------------------------------------------------
         */
        /*      Handler nearest resampling ... we don't even care about the */
        /*      data type, we just do a bytewise copy. */
        /* --------------------------------------------------------------------
         */
        if (strncmp(pszResampling, "nearest", 4) == 0 ||
            strncmp(pszResampling, "NEAR", 4) == 0)
        {
            for (i = 0; i < nXIterations; i++)
            {
                uint64_t srcX64 = (uint64_t)i * (uint32_t)nOMult;
                uint32_t srcX;
                uint64_t pixelOffset;
                uint64_t srcOffset;

                if ((uint64_t)i + nTXOff >= nOBlockXSize)
                    break;
                if (srcX64 >= nBlockXSize)
                    break;
                srcX = (uint32_t)srcX64;
                pixelOffset = (uint64_t)srcY * nBlockXSize + srcX;
                if ((uint32_t)nPixelGroupBytes != 0 &&
                    pixelOffset > UINT64_MAX / (uint32_t)nPixelGroupBytes)
                    break;
                srcOffset = pixelOffset * (uint32_t)nPixelGroupBytes;
                if (srcOffset > (uint64_t)((size_t)-1))
                    break;
                pabySrc = pabySrcTile + (size_t)srcOffset;

                /*
                 * For now use simple subsampling, from the top left corner
                 * of the source block of pixels.
                 */

                for (k = 0; k < nPixelBytes; k++)
                    pabyDst[k] = pabySrc[k];

                pabyDst += nDstPixelStep;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Handle the case of averaging.  For this we also have to */
        /*      handle each sample format we are concerned with. */
        /* --------------------------------------------------------------------
         */
        else if (strncmp(pszResampling, "averag", 6) == 0 ||
                 strncmp(pszResampling, "AVERAG", 6) == 0)
        {
            for (i = 0; i < nXIterations; i++)
            {
                double dfTotal;
                uint32_t srcX, nXSize, nYSize;
                uint64_t srcX64 = (uint64_t)i * (uint32_t)nOMult;
                uint64_t pixelOffset;
                uint64_t srcOffset;
                uint64_t iSample, nSampleCount;

                if ((uint64_t)i + nTXOff >= nOBlockXSize)
                    break;
                if (srcX64 >= nBlockXSize)
                    break;
                srcX = (uint32_t)srcX64;

                nXSize = MIN((uint32_t)nOMult, nBlockXSize - srcX);
                nYSize = MIN((uint32_t)nOMult, nBlockYSize - srcY);
                nSampleCount = (uint64_t)nXSize * nYSize;
                pixelOffset = (uint64_t)srcY * nBlockXSize + srcX;
                if ((uint32_t)nPixelGroupBytes != 0 &&
                    pixelOffset > UINT64_MAX / (uint32_t)nPixelGroupBytes)
                    break;
                srcOffset = pixelOffset * (uint32_t)nPixelGroupBytes;
                if (srcOffset > (uint64_t)((size_t)-1))
                    break;
                pabySrc = pabySrcTile + (size_t)srcOffset;

                TIFF_GetSourceSamples(padfSamples, pabySrc, nPixelBytes,
                                      nSampleFormat, nXSize, nYSize,
                                      nPixelGroupBytes, nLineOffset);

                dfTotal = 0;
                for (iSample = 0; iSample < nSampleCount; iSample++)
                {
                    dfTotal += padfSamples[iSample];
                }

                TIFF_SetSample(pabyDst, nPixelBytes, nSampleFormat,
                               dfTotal / (double)nSampleCount);

                pabyDst += (uint32_t)nPixelBytes;
            }
        }
    }

    free(padfSamples);
}

/************************************************************************/
/*                     TIFF_DownSample_Subsampled()                     */
/************************************************************************/
static void TIFF_DownSample_Subsampled(
    unsigned char *pabySrcTile, int nSample, uint32_t nBlockXSize,
    uint32_t nBlockYSize, unsigned char *pabyOTile, uint32_t nOBlockXSize,
    uint32_t nOBlockYSize, uint32_t nTXOff, uint32_t nTYOff, int nOMult,
    const char *pszResampling, int nHorSubsampling, int nVerSubsampling)
{
    /* TODO: test with variety of subsampling values, and incovinient tile/strip
     * sizes */
    int nSampleBlockSize;
    int nSourceSampleRowSize;
    int nDestSampleRowSize;
    uint32_t nSampleBlockSizeU;
    uint32_t nSourceSampleRowSizeU;
    uint32_t nDestSampleRowSizeU;
    uint32_t nOMultU;
    uint32_t nHorSubsamplingU;
    uint32_t nVerSubsamplingU;
    uint32_t nSourceX, nSourceY;
    uint32_t nSourceXSec, nSourceYSec;
    uint32_t nSourceXSecEnd, nSourceYSecEnd;
    uint32_t nDestX, nDestY;
    int nSampleOffsetInSampleBlock;
    uint64_t nCummulator;
    uint64_t nCummulatorCount;
    uint64_t nTmp;

    if (nOMult <= 0 || nHorSubsampling <= 0 || nVerSubsampling <= 0)
        return;
    nOMultU = (uint32_t)nOMult;
    nHorSubsamplingU = (uint32_t)nHorSubsampling;
    nVerSubsamplingU = (uint32_t)nVerSubsampling;

    if (!TIFFOverviewMul64(nHorSubsamplingU, nVerSubsamplingU, &nTmp) ||
        !TIFFOverviewAdd64(nTmp, 2, &nTmp) || nTmp > INT_MAX)
        return;
    nSampleBlockSize = (int)nTmp;
    nSampleBlockSizeU = (uint32_t)nSampleBlockSize;

    {
        uint64_t nSourceSamplesPerRow;

        if (!TIFFOverviewHowMany64(nBlockXSize, nHorSubsamplingU,
                                   &nSourceSamplesPerRow) ||
            !TIFFOverviewMul64(nSourceSamplesPerRow, nSampleBlockSizeU,
                               &nTmp) ||
            nTmp > INT_MAX)
            return;
    }
    nSourceSampleRowSize = (int)nTmp;
    nSourceSampleRowSizeU = (uint32_t)nSourceSampleRowSize;

    {
        uint64_t nDestSamplesPerRow;

        if (!TIFFOverviewHowMany64(nOBlockXSize, nHorSubsamplingU,
                                   &nDestSamplesPerRow) ||
            !TIFFOverviewMul64(nDestSamplesPerRow, nSampleBlockSizeU, &nTmp) ||
            nTmp > INT_MAX)
            return;
    }
    nDestSampleRowSize = (int)nTmp;
    nDestSampleRowSizeU = (uint32_t)nDestSampleRowSize;

    if (strncmp(pszResampling, "nearest", 4) == 0 ||
        strncmp(pszResampling, "NEAR", 4) == 0)
    {
        if (nSample == 0)
        {
            for (nSourceY = 0, nDestY = nTYOff; nSourceY < nBlockYSize;
                 nDestY++)
            {
                if (nDestY >= nOBlockYSize)
                    break;

                for (nSourceX = 0, nDestX = nTXOff; nSourceX < nBlockXSize;
                     nDestX++)
                {
                    size_t nDstOffset;
                    size_t nSrcOffset;

                    if (nDestX >= nOBlockXSize)
                        break;

                    if (!TIFFOverviewPackedOffset(
                            nDestY, nDestX, nHorSubsamplingU, nVerSubsamplingU,
                            nDestSampleRowSizeU, nSampleBlockSizeU,
                            &nDstOffset) ||
                        !TIFFOverviewPackedOffset(
                            nSourceY, nSourceX, nHorSubsamplingU,
                            nVerSubsamplingU, nSourceSampleRowSizeU,
                            nSampleBlockSizeU, &nSrcOffset))
                        return;

                    pabyOTile[nDstOffset] = pabySrcTile[nSrcOffset];

                    if (!TIFFOverviewAdvanceUInt32(&nSourceX, nOMultU))
                        break;
                }

                if (!TIFFOverviewAdvanceUInt32(&nSourceY, nOMultU))
                    break;
            }
        }
        else
        {
            uint64_t nSampleOffset64;
            if (nSample <= 0 ||
                !TIFFOverviewMul64(nHorSubsamplingU, nVerSubsamplingU,
                                   &nSampleOffset64) ||
                !TIFFOverviewAdd64(nSampleOffset64, (uint32_t)(nSample - 1),
                                   &nSampleOffset64) ||
                nSampleOffset64 >= nSampleBlockSizeU ||
                nSampleOffset64 > INT_MAX)
                return;
            nSampleOffsetInSampleBlock = (int)nSampleOffset64;
            for (nSourceY = 0, nDestY = (nTYOff / nVerSubsamplingU);
                 nSourceY < (nBlockYSize / nVerSubsamplingU); nDestY++)
            {
                uint64_t nDestYPixel;

                if (!TIFFOverviewMul64(nDestY, nVerSubsamplingU, &nDestYPixel))
                    return;
                if (nDestYPixel >= nOBlockYSize)
                    break;

                for (nSourceX = 0, nDestX = (nTXOff / nHorSubsamplingU);
                     nSourceX < (nBlockXSize / nHorSubsamplingU); nDestX++)
                {
                    uint64_t nDestXPixel;
                    size_t nDstOffset;
                    size_t nSrcOffset;

                    if (!TIFFOverviewMul64(nDestX, nHorSubsamplingU,
                                           &nDestXPixel))
                        return;
                    if (nDestXPixel >= nOBlockXSize)
                        break;

                    if (!TIFFOverviewSampleOffset(
                            nDestY, nDestX, nDestSampleRowSizeU,
                            nSampleBlockSizeU,
                            (uint32_t)nSampleOffsetInSampleBlock,
                            &nDstOffset) ||
                        !TIFFOverviewSampleOffset(
                            nSourceY, nSourceX, nSourceSampleRowSizeU,
                            nSampleBlockSizeU,
                            (uint32_t)nSampleOffsetInSampleBlock, &nSrcOffset))
                        return;

                    pabyOTile[nDstOffset] = pabySrcTile[nSrcOffset];

                    if (!TIFFOverviewAdvanceUInt32(&nSourceX, nOMultU))
                        break;
                }

                if (!TIFFOverviewAdvanceUInt32(&nSourceY, nOMultU))
                    break;
            }
        }
    }
    else if (strncmp(pszResampling, "averag", 6) == 0 ||
             strncmp(pszResampling, "AVERAG", 6) == 0)
    {
        if (nSample == 0)
        {
            for (nSourceY = 0, nDestY = nTYOff; nSourceY < nBlockYSize;
                 nDestY++)
            {
                if (nDestY >= nOBlockYSize)
                    break;

                for (nSourceX = 0, nDestX = nTXOff; nSourceX < nBlockXSize;
                     nDestX++)
                {
                    uint64_t nSectionEnd64;
                    size_t nDstOffset;

                    if (nDestX >= nOBlockXSize)
                        break;

                    nSectionEnd64 = (uint64_t)nSourceX + nOMultU;
                    if (nSectionEnd64 > nBlockXSize)
                        nSourceXSecEnd = nBlockXSize;
                    else
                        nSourceXSecEnd = (uint32_t)nSectionEnd64;
                    nSectionEnd64 = (uint64_t)nSourceY + nOMultU;
                    if (nSectionEnd64 > nBlockYSize)
                        nSourceYSecEnd = nBlockYSize;
                    else
                        nSourceYSecEnd = (uint32_t)nSectionEnd64;
                    nCummulator = 0;
                    for (nSourceYSec = nSourceY; nSourceYSec < nSourceYSecEnd;
                         nSourceYSec++)
                    {
                        for (nSourceXSec = nSourceX;
                             nSourceXSec < nSourceXSecEnd; nSourceXSec++)
                        {
                            size_t nSrcOffset;
                            if (!TIFFOverviewPackedOffset(
                                    nSourceYSec, nSourceXSec, nHorSubsamplingU,
                                    nVerSubsamplingU, nSourceSampleRowSizeU,
                                    nSampleBlockSizeU, &nSrcOffset) ||
                                nCummulator >
                                    UINT64_MAX - pabySrcTile[nSrcOffset])
                                return;
                            nCummulator += pabySrcTile[nSrcOffset];
                        }
                    }
                    if (!TIFFOverviewMul64(nSourceXSecEnd - nSourceX,
                                           nSourceYSecEnd - nSourceY,
                                           &nCummulatorCount) ||
                        nCummulatorCount == 0 ||
                        !TIFFOverviewPackedOffset(
                            nDestY, nDestX, nHorSubsamplingU, nVerSubsamplingU,
                            nDestSampleRowSizeU, nSampleBlockSizeU,
                            &nDstOffset))
                        return;
                    pabyOTile[nDstOffset] =
                        (unsigned char)((nCummulator +
                                         (nCummulatorCount >> 1)) /
                                        nCummulatorCount);

                    if (!TIFFOverviewAdvanceUInt32(&nSourceX, nOMultU))
                        break;
                }

                if (!TIFFOverviewAdvanceUInt32(&nSourceY, nOMultU))
                    break;
            }
        }
        else
        {
            uint64_t nSampleOffset64;
            if (nSample <= 0 ||
                !TIFFOverviewMul64(nHorSubsamplingU, nVerSubsamplingU,
                                   &nSampleOffset64) ||
                !TIFFOverviewAdd64(nSampleOffset64, (uint32_t)(nSample - 1),
                                   &nSampleOffset64) ||
                nSampleOffset64 >= nSampleBlockSizeU ||
                nSampleOffset64 > INT_MAX)
                return;
            nSampleOffsetInSampleBlock = (int)nSampleOffset64;
            for (nSourceY = 0, nDestY = (nTYOff / nVerSubsamplingU);
                 nSourceY < (nBlockYSize / nVerSubsamplingU); nDestY++)
            {
                uint64_t nDestYPixel;
                if (!TIFFOverviewMul64(nDestY, nVerSubsamplingU, &nDestYPixel))
                    return;
                if (nDestYPixel >= nOBlockYSize)
                    break;

                for (nSourceX = 0, nDestX = (nTXOff / nHorSubsamplingU);
                     nSourceX < (nBlockXSize / nHorSubsamplingU); nDestX++)
                {
                    uint32_t nSourceXLimit = nBlockXSize / nHorSubsamplingU;
                    uint32_t nSourceYLimit = nBlockYSize / nVerSubsamplingU;
                    uint64_t nDestXPixel;
                    uint64_t nSectionEnd64;
                    size_t nDstOffset;

                    if (!TIFFOverviewMul64(nDestX, nHorSubsamplingU,
                                           &nDestXPixel))
                        return;
                    if (nDestXPixel >= nOBlockXSize)
                        break;

                    nSectionEnd64 = (uint64_t)nSourceX + nOMultU;
                    if (nSectionEnd64 > nSourceXLimit)
                        nSourceXSecEnd = nSourceXLimit;
                    else
                        nSourceXSecEnd = (uint32_t)nSectionEnd64;
                    nSectionEnd64 = (uint64_t)nSourceY + nOMultU;
                    if (nSectionEnd64 > nSourceYLimit)
                        nSourceYSecEnd = nSourceYLimit;
                    else
                        nSourceYSecEnd = (uint32_t)nSectionEnd64;
                    nCummulator = 0;
                    for (nSourceYSec = nSourceY; nSourceYSec < nSourceYSecEnd;
                         nSourceYSec++)
                    {
                        for (nSourceXSec = nSourceX;
                             nSourceXSec < nSourceXSecEnd; nSourceXSec++)
                        {
                            size_t nSrcOffset;
                            if (!TIFFOverviewSampleOffset(
                                    nSourceYSec, nSourceXSec,
                                    nSourceSampleRowSizeU, nSampleBlockSizeU,
                                    (uint32_t)nSampleOffsetInSampleBlock,
                                    &nSrcOffset) ||
                                nCummulator >
                                    UINT64_MAX - pabySrcTile[nSrcOffset])
                                return;
                            nCummulator += pabySrcTile[nSrcOffset];
                        }
                    }
                    if (!TIFFOverviewMul64(nSourceXSecEnd - nSourceX,
                                           nSourceYSecEnd - nSourceY,
                                           &nCummulatorCount) ||
                        nCummulatorCount == 0 ||
                        !TIFFOverviewSampleOffset(
                            nDestY, nDestX, nDestSampleRowSizeU,
                            nSampleBlockSizeU,
                            (uint32_t)nSampleOffsetInSampleBlock, &nDstOffset))
                        return;
                    pabyOTile[nDstOffset] =
                        (unsigned char)((nCummulator +
                                         (nCummulatorCount >> 1)) /
                                        nCummulatorCount);

                    if (!TIFFOverviewAdvanceUInt32(&nSourceX, nOMultU))
                        break;
                }

                if (!TIFFOverviewAdvanceUInt32(&nSourceY, nOMultU))
                    break;
            }
        }
    }
}

/************************************************************************/
/*                      TIFF_ProcessFullResBlock()                      */
/*                                                                      */
/*      Process one block of full res data, downsampling into each      */
/*      of the overviews.                                               */
/************************************************************************/

void TIFF_ProcessFullResBlock(TIFF *hTIFF, int nPlanarConfig, int bSubsampled,
                              int nHorSubsampling, int nVerSubsampling,
                              int nOverviews, int *panOvList, int nBitsPerPixel,
                              int nSamples, TIFFOvrCache **papoRawBIs,
                              uint32_t nSXOff, uint32_t nSYOff,
                              unsigned char *pabySrcTile, uint32_t nBlockXSize,
                              uint32_t nBlockYSize, int nSampleFormat,
                              const char *pszResampling)

{
    int iOverview, iSample;

    for (iSample = 0; iSample < nSamples; iSample++)
    {
        /*
         * We have to read a tile/strip for each sample for
         * PLANARCONFIG_SEPARATE.  Otherwise, we just read all the samples
         * at once when handling the first sample.
         */
        if (nPlanarConfig == PLANARCONFIG_SEPARATE || iSample == 0)
        {
            if (TIFFIsTiled(hTIFF))
            {
                tmsize_t nTileSize = TIFFTileSize(hTIFF);
                if (nTileSize <= 0)
                    return;
                TIFFReadEncodedTile(hTIFF,
                                    TIFFComputeTile(hTIFF, nSXOff, nSYOff, 0,
                                                    (tsample_t)iSample),
                                    pabySrcTile, nTileSize);
            }
            else
            {
                tmsize_t nStripSize = TIFFStripSize(hTIFF);
                if (nStripSize <= 0)
                    return;

                TIFFReadEncodedStrip(
                    hTIFF, TIFFComputeStrip(hTIFF, nSYOff, (tsample_t)iSample),
                    pabySrcTile, nStripSize);
            }
        }

        /*
         * Loop over destination overview layers
         */
        for (iOverview = 0; iOverview < nOverviews; iOverview++)
        {
            TIFFOvrCache *poRBI = papoRawBIs[iOverview];
            unsigned char *pabyOTile;
            uint32_t nTXOff, nTYOff, nOXOff, nOYOff, nOMult;
            uint32_t nOBlockXSize = poRBI->nBlockXSize;
            uint32_t nOBlockYSize = poRBI->nBlockYSize;
            int nSkewBits, nSampleByteOffset;

            /*
             * Fetch the destination overview tile
             */
            nOMult = (uint32_t)panOvList[iOverview];
            if (nOMult == 0 || nOBlockXSize == 0 || nOBlockYSize == 0)
                return;
            nOXOff = (nSXOff / nOMult) / nOBlockXSize;
            nOYOff = (nSYOff / nOMult) / nOBlockYSize;
            if (!TIFFOverviewTileOffset(nSXOff, nOXOff, nOMult, nOBlockXSize,
                                        &nTXOff) ||
                !TIFFOverviewTileOffset(nSYOff, nOYOff, nOMult, nOBlockYSize,
                                        &nTYOff))
                return;

            if (bSubsampled)
            {
                pabyOTile =
                    TIFFGetOvrBlock_Subsampled(poRBI, (int)nOXOff, (int)nOYOff);

                /*
                 * Establish the offset into this tile at which we should
                 * start placing data.
                 */

#ifdef DBMALLOC
                malloc_chain_check(1);
#endif
                TIFF_DownSample_Subsampled(
                    pabySrcTile, iSample, nBlockXSize, nBlockYSize, pabyOTile,
                    poRBI->nBlockXSize, poRBI->nBlockYSize, nTXOff, nTYOff,
                    (int)nOMult, pszResampling, nHorSubsampling,
                    nVerSubsampling);
#ifdef DBMALLOC
                malloc_chain_check(1);
#endif
            }
            else
            {

                pabyOTile =
                    TIFFGetOvrBlock(poRBI, (int)nOXOff, (int)nOYOff, iSample);

                /*
                 * Establish the offset into this tile at which we should
                 * start placing data.
                 */

                /*
                 * Figure out the skew (extra space between ``our samples'') and
                 * the byte offset to the first sample.
                 */
                assert((nBitsPerPixel % 8) == 0);
                if (nPlanarConfig == PLANARCONFIG_SEPARATE)
                {
                    nSkewBits = 0;
                    nSampleByteOffset = 0;
                }
                else
                {
                    uint64_t nSkewBits64;
                    uint64_t nSampleByteOffset64;

                    if (!TIFFOverviewMul64((uint64_t)nBitsPerPixel,
                                           (uint64_t)(nSamples - 1),
                                           &nSkewBits64) ||
                        nSkewBits64 > INT_MAX ||
                        !TIFFOverviewMul64((uint64_t)(nBitsPerPixel / 8),
                                           (uint64_t)iSample,
                                           &nSampleByteOffset64) ||
                        nSampleByteOffset64 > INT_MAX)
                        return;
                    nSkewBits = (int)nSkewBits64;
                    nSampleByteOffset = (int)nSampleByteOffset64;
                }

                /*
                 * Perform the downsampling.
                 */
#ifdef DBMALLOC
                malloc_chain_check(1);
#endif
                TIFF_DownSample(pabySrcTile + nSampleByteOffset, nBlockXSize,
                                nBlockYSize, nSkewBits, nBitsPerPixel,
                                pabyOTile, poRBI->nBlockXSize,
                                poRBI->nBlockYSize, nTXOff, nTYOff, (int)nOMult,
                                nSampleFormat, pszResampling);
#ifdef DBMALLOC
                malloc_chain_check(1);
#endif
            }
        }
    }
}

/************************************************************************/
/*                        TIFF_BuildOverviews()                         */
/*                                                                      */
/*      Build the requested list of overviews.  Overviews are           */
/*      maintained in a bunch of temporary files and then these are     */
/*      written back to the TIFF file.  Only one pass through the       */
/*      source TIFF file is made for any number of output               */
/*      overviews.                                                      */
/************************************************************************/

void TIFFBuildOverviews(TIFF *hTIFF, int nOverviews, int *panOvList,
                        int bUseSubIFDs, const char *pszResampleMethod,
                        int (*pfnProgress)(double, void *), void *pProgressData)

{
    TIFFOvrCache **papoRawBIs = NULL;
    uint32_t nXSize, nYSize, nBlockXSize, nBlockYSize;
    uint16_t nBitsPerPixel, nPhotometric, nCompressFlag, nSamples,
        nPlanarConfig, nSampleFormat;
    int bSubsampled;
    uint16_t nHorSubsampling, nVerSubsampling;
    int bTiled, i;
    uint32_t nSXOff, nSYOff;
    unsigned char *pabySrcTile = NULL;
    uint16_t *panRedMap = NULL, *panGreenMap = NULL, *panBlueMap = NULL;
    uint16_t *panRedMapOwned = NULL;
    uint16_t *panGreenMapOwned = NULL;
    uint16_t *panBlueMapOwned = NULL;
    tmsize_t nOverviewListSize = 0;
    TIFFErrorHandler pfnWarning = NULL;
    int bWarningHandlerChanged = FALSE;

    (void)pfnProgress;
    (void)pProgressData;

    /* -------------------------------------------------------------------- */
    /*      Get the base raster size.                                       */
    /* -------------------------------------------------------------------- */
    TIFFGetField(hTIFF, TIFFTAG_IMAGEWIDTH, &nXSize);
    TIFFGetField(hTIFF, TIFFTAG_IMAGELENGTH, &nYSize);

    TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &nBitsPerPixel);
    /* TODO: nBitsPerPixel seems misnomer and may need renaming to
     * nBitsPerSample */
    TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &nSamples);
    TIFFGetFieldDefaulted(hTIFF, TIFFTAG_PLANARCONFIG, &nPlanarConfig);

    TIFFGetFieldDefaulted(hTIFF, TIFFTAG_PHOTOMETRIC, &nPhotometric);
    TIFFGetFieldDefaulted(hTIFF, TIFFTAG_COMPRESSION, &nCompressFlag);
    TIFFGetFieldDefaulted(hTIFF, TIFFTAG_SAMPLEFORMAT, &nSampleFormat);

    if (nPhotometric == PHOTOMETRIC_YCBCR || nPhotometric == PHOTOMETRIC_ITULAB)
    {
        if (nBitsPerPixel != 8 || nSamples != 3 ||
            nPlanarConfig != PLANARCONFIG_CONTIG ||
            nSampleFormat != SAMPLEFORMAT_UINT)
        {
            /* TODO: use of TIFFError is inconsistent with use of fprintf in
             * addtiffo.c, sort out */
            TIFFErrorExt(
                TIFFClientdata(hTIFF), "TIFFBuildOverviews",
                "File `%s' has an unsupported subsampling configuration.\n",
                TIFFFileName(hTIFF));
            /* If you need support for this particular flavor, please contact
             * either Frank Warmerdam warmerdam@pobox.com Joris Van Damme
             * info@awaresystems.be
             */
            goto cleanup;
        }
        bSubsampled = 1;
        TIFFGetField(hTIFF, TIFFTAG_YCBCRSUBSAMPLING, &nHorSubsampling,
                     &nVerSubsampling);
        /* TODO: find out if maybe TIFFGetFieldDefaulted is better choice for
         * YCbCrSubsampling tag */
    }
    else
    {
        if (nBitsPerPixel < 8)
        {
            /* TODO: use of TIFFError is inconsistent with use of fprintf in
             * addtiffo.c, sort out */
            TIFFErrorExt(
                TIFFClientdata(hTIFF), "TIFFBuildOverviews",
                "File `%s' has samples of %d bits per sample.  Sample\n"
                "sizes of less than 8 bits per sample are not supported.\n",
                TIFFFileName(hTIFF), nBitsPerPixel);
            goto cleanup;
        }
        bSubsampled = 0;
        nHorSubsampling = 1;
        nVerSubsampling = 1;
    }

    /* -------------------------------------------------------------------- */
    /*      Turn off warnings to avoid a lot of repeated warnings while      */
    /*      rereading directories.                                          */
    /* -------------------------------------------------------------------- */
    pfnWarning = TIFFSetWarningHandler(NULL);
    bWarningHandlerChanged = TRUE;

    /* -------------------------------------------------------------------- */
    /*      Get the base raster block size.                                 */
    /* -------------------------------------------------------------------- */
    if (TIFFGetField(hTIFF, TIFFTAG_ROWSPERSTRIP, &(nBlockYSize)))
    {
        nBlockXSize = nXSize;
        bTiled = FALSE;
    }
    else
    {
        TIFFGetField(hTIFF, TIFFTAG_TILEWIDTH, &nBlockXSize);
        TIFFGetField(hTIFF, TIFFTAG_TILELENGTH, &nBlockYSize);
        bTiled = TRUE;
    }
    if (nBlockXSize == 0 || nBlockYSize == 0)
    {
        TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFBuildOverviews",
                     "File `%s' has an invalid block size.\n",
                     TIFFFileName(hTIFF));
        goto cleanup;
    }

    /* -------------------------------------------------------------------- */
    /*	Capture the palette if there is one.				*/
    /* -------------------------------------------------------------------- */
    if (TIFFGetField(hTIFF, TIFFTAG_COLORMAP, &panRedMap, &panGreenMap,
                     &panBlueMap))
    {
        uint16_t *panRed2, *panGreen2, *panBlue2;
        uint64_t nColorCount;
        uint64_t nColorBytes64;
        tmsize_t nColorBytes;

        if (nBitsPerPixel >= 64)
        {
            TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFBuildOverviews",
                         "BitsPerSample is too large for a palette.\n");
            goto cleanup;
        }
        nColorCount = 1ULL << nBitsPerPixel;
        if (!TIFFOverviewMul64(nColorCount, sizeof(uint16_t), &nColorBytes64) ||
            !TIFFOverviewToSSize(nColorBytes64, &nColorBytes))
        {
            goto cleanup;
        }

        panRed2 = (uint16_t *)_TIFFmalloc(nColorBytes);
        panGreen2 = (uint16_t *)_TIFFmalloc(nColorBytes);
        panBlue2 = (uint16_t *)_TIFFmalloc(nColorBytes);
        if (panRed2 == NULL || panGreen2 == NULL || panBlue2 == NULL)
        {
            _TIFFfree(panRed2);
            _TIFFfree(panGreen2);
            _TIFFfree(panBlue2);
            goto cleanup;
        }

        memcpy(panRed2, panRedMap, (size_t)nColorBytes);
        memcpy(panGreen2, panGreenMap, (size_t)nColorBytes);
        memcpy(panBlue2, panBlueMap, (size_t)nColorBytes);

        panRedMapOwned = panRed2;
        panGreenMapOwned = panGreen2;
        panBlueMapOwned = panBlue2;

        panRed2 = NULL;
        panGreen2 = NULL;
        panBlue2 = NULL;

        panRedMap = panRedMapOwned;
        panGreenMap = panGreenMapOwned;
        panBlueMap = panBlueMapOwned;
    }
    else
    {
        panRedMap = panGreenMap = panBlueMap = NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize overviews.                                           */
    /* -------------------------------------------------------------------- */
    if (nOverviews <= 0)
    {
        goto cleanup;
    }
    if (!TIFFOverviewMulSSize((tmsize_t)nOverviews, (tmsize_t)sizeof(void *),
                              &nOverviewListSize))
        goto cleanup;
    papoRawBIs = (TIFFOvrCache **)_TIFFmalloc(nOverviewListSize);
    if (papoRawBIs == NULL)
    {
        goto cleanup;
    }
    _TIFFmemset(papoRawBIs, 0, nOverviewListSize);

    for (i = 0; i < nOverviews; i++)
    {
        uint32_t nOXSize, nOYSize, nOBlockXSize, nOBlockYSize;
        toff_t nDirOffset;

        if (panOvList[i] <= 0)
            goto cleanup;

        {
            uint64_t nOXSize64;
            uint64_t nOYSize64;
            uint32_t nOverviewFactor = (uint32_t)panOvList[i];

            if (!TIFFOverviewHowMany64(nXSize, nOverviewFactor, &nOXSize64) ||
                !TIFFOverviewHowMany64(nYSize, nOverviewFactor, &nOYSize64) ||
                nOXSize64 > UINT32_MAX || nOYSize64 > UINT32_MAX)
                goto cleanup;

            nOXSize = (uint32_t)nOXSize64;
            nOYSize = (uint32_t)nOYSize64;
        }

        nOBlockXSize = MIN(nBlockXSize, nOXSize);
        nOBlockYSize = MIN(nBlockYSize, nOYSize);

        if (bTiled)
        {
            if ((nOBlockXSize % 16) != 0)
            {
                uint32_t nAdjust = 16 - (nOBlockXSize % 16);
                if (nOBlockXSize > UINT32_MAX - nAdjust)
                    goto cleanup;
                nOBlockXSize = nOBlockXSize + 16 - (nOBlockXSize % 16);
            }

            if ((nOBlockYSize % 16) != 0)
            {
                uint32_t nAdjust = 16 - (nOBlockYSize % 16);
                if (nOBlockYSize > UINT32_MAX - nAdjust)
                    goto cleanup;
                nOBlockYSize = nOBlockYSize + 16 - (nOBlockYSize % 16);
            }
        }
        if (nOBlockXSize > INT_MAX || nOBlockYSize > INT_MAX)
            goto cleanup;

        nDirOffset = TIFF_WriteOverview(
            hTIFF, nOXSize, nOYSize, nBitsPerPixel, nPlanarConfig, nSamples,
            (int)nOBlockXSize, (int)nOBlockYSize, bTiled, nCompressFlag,
            nPhotometric, nSampleFormat, panRedMap, panGreenMap, panBlueMap,
            bUseSubIFDs, nHorSubsampling, nVerSubsampling);

        papoRawBIs[i] = TIFFCreateOvrCache(hTIFF, nDirOffset);
        if (papoRawBIs[i] == NULL)
            goto cleanup;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate a buffer to hold a source block.                       */
    /* -------------------------------------------------------------------- */
    if (bTiled)
    {
        tmsize_t nTileSize = TIFFTileSize(hTIFF);
        if (nTileSize <= 0)
            goto cleanup;
        pabySrcTile = (unsigned char *)_TIFFmalloc(nTileSize);
    }
    else
    {
        tmsize_t nStripSize = TIFFStripSize(hTIFF);
        if (nStripSize <= 0)
            goto cleanup;
        pabySrcTile = (unsigned char *)_TIFFmalloc(nStripSize);
    }
    if (pabySrcTile == NULL)
        goto cleanup;

    /* -------------------------------------------------------------------- */
    /*      Loop over the source raster, applying data to the               */
    /*      destination raster.                                             */
    /* -------------------------------------------------------------------- */
    for (nSYOff = 0; nSYOff < nYSize; nSYOff += nBlockYSize)
    {
        for (nSXOff = 0; nSXOff < nXSize; nSXOff += nBlockXSize)
        {
            /*
             * Read and resample into the various overview images.
             */

            TIFF_ProcessFullResBlock(
                hTIFF, nPlanarConfig, bSubsampled, nHorSubsampling,
                nVerSubsampling, nOverviews, panOvList, nBitsPerPixel, nSamples,
                papoRawBIs, nSXOff, nSYOff, pabySrcTile, nBlockXSize,
                nBlockYSize, nSampleFormat, pszResampleMethod);
            if (nXSize - nSXOff <= nBlockXSize)
                break;
        }
        if (nYSize - nSYOff <= nBlockYSize)
            break;
    }

cleanup:
    _TIFFfree(pabySrcTile);
    _TIFFfree(panRedMapOwned);
    _TIFFfree(panGreenMapOwned);
    _TIFFfree(panBlueMapOwned);

    /* -------------------------------------------------------------------- */
    /*      Cleanup the rawblockedimage files.                              */
    /* -------------------------------------------------------------------- */
    for (i = 0; i < nOverviews; i++)
    {
        if (papoRawBIs != NULL && papoRawBIs[i] != NULL)
            TIFFDestroyOvrCache(papoRawBIs[i]);
    }

    if (papoRawBIs != NULL)
        _TIFFfree(papoRawBIs);

    if (bWarningHandlerChanged)
        TIFFSetWarningHandler(pfnWarning);
}
