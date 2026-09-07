/******************************************************************************
 * Project:  TIFF Overview Builder
 * Purpose:  Library functions to maintain two rows of tiles or two strips
 *           of data for output overviews as an output cache.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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

#include "tif_ovrcache.h"
#include "tiffiop.h"
#include <assert.h>

/************************************************************************/
/*                         TIFFCreateOvrCache()                         */
/*                                                                      */
/*      Create an overview cache to hold two rows of blocks from an     */
/*      existing TIFF directory.                                        */
/************************************************************************/

TIFFOvrCache *TIFFCreateOvrCache(TIFF *hTIFF, toff_t nDirOffset)

{
    TIFFOvrCache *psCache;
    tmsize_t nBytesPerRowSize = 0;
    toff_t nBaseDirOffset;
    int nRet;

    psCache = (TIFFOvrCache *)_TIFFmalloc(sizeof(TIFFOvrCache));
    psCache->nDirOffset = nDirOffset;
    psCache->hTIFF = hTIFF;

    /* -------------------------------------------------------------------- */
    /*      Get definition of this raster from the TIFF file itself.        */
    /* -------------------------------------------------------------------- */
    nBaseDirOffset = TIFFCurrentDirOffset(psCache->hTIFF);
    nRet = TIFFSetSubDirectory(hTIFF, nDirOffset);
    (void)nRet;
    assert(nRet == 1);

    TIFFGetField(hTIFF, TIFFTAG_IMAGEWIDTH, &(psCache->nXSize));
    TIFFGetField(hTIFF, TIFFTAG_IMAGELENGTH, &(psCache->nYSize));

    TIFFGetField(hTIFF, TIFFTAG_BITSPERSAMPLE, &(psCache->nBitsPerPixel));
    TIFFGetField(hTIFF, TIFFTAG_SAMPLESPERPIXEL, &(psCache->nSamples));
    TIFFGetField(hTIFF, TIFFTAG_PLANARCONFIG, &(psCache->nPlanarConfig));

    if (!TIFFIsTiled(hTIFF))
    {
        tmsize_t nBlockSize;
        TIFFGetField(hTIFF, TIFFTAG_ROWSPERSTRIP, &(psCache->nBlockYSize));
        psCache->nBlockXSize = psCache->nXSize;
        nBlockSize = TIFFStripSize(hTIFF);
        if (nBlockSize <= 0)
        {
            TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFCreateOvrCache",
                         "Invalid overview strip size.");
            _TIFFfree(psCache);
            return NULL;
        }
        psCache->nBytesPerBlock = (toff_t)nBlockSize;
        psCache->bTiled = FALSE;
    }
    else
    {
        tmsize_t nBlockSize;
        TIFFGetField(hTIFF, TIFFTAG_TILEWIDTH, &(psCache->nBlockXSize));
        TIFFGetField(hTIFF, TIFFTAG_TILELENGTH, &(psCache->nBlockYSize));
        nBlockSize = TIFFTileSize(hTIFF);
        if (nBlockSize <= 0)
        {
            TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFCreateOvrCache",
                         "Invalid overview tile size.");
            _TIFFfree(psCache);
            return NULL;
        }
        psCache->nBytesPerBlock = (toff_t)nBlockSize;
        psCache->bTiled = TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      Compute some values from this.                                  */
    /* -------------------------------------------------------------------- */

    if (psCache->nBlockXSize == 0 || psCache->nBlockYSize == 0)
    {
        TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFCreateOvrCache",
                     "Invalid overview block size.");
        _TIFFfree(psCache);
        return NULL;
    }
    {
        uint64_t nBlocksPerRow =
            TIFFhowmany_64(psCache->nXSize, psCache->nBlockXSize);
        uint64_t nBlocksPerColumn =
            TIFFhowmany_64(psCache->nYSize, psCache->nBlockYSize);
        if (nBlocksPerRow > INT_MAX || nBlocksPerColumn > INT_MAX)
        {
            TIFFErrorExt(TIFFClientdata(hTIFF), "TIFFCreateOvrCache",
                         "Too many overview blocks.");
            _TIFFfree(psCache);
            return NULL;
        }
        psCache->nBlocksPerRow = (int)nBlocksPerRow;
        psCache->nBlocksPerColumn = (int)nBlocksPerColumn;
    }

    {
        uint64_t nBytesPerRow = _TIFFMultiply64(
            hTIFF, psCache->nBytesPerBlock, (uint64_t)psCache->nBlocksPerRow,
            "overview cache row");
        if (nBytesPerRow == 0 && psCache->nBytesPerBlock != 0 &&
            psCache->nBlocksPerRow != 0)
        {
            _TIFFfree(psCache);
            return NULL;
        }

        if (psCache->nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            nBytesPerRow = _TIFFMultiply64(
                hTIFF, nBytesPerRow, psCache->nSamples, "overview cache row");
            if (nBytesPerRow == 0 && psCache->nSamples != 0)
            {
                _TIFFfree(psCache);
                return NULL;
            }
        }

        nBytesPerRowSize =
            _TIFFCastUInt64ToSSize(hTIFF, nBytesPerRow, "overview cache row");
        if (nBytesPerRowSize == 0 && nBytesPerRow != 0)
        {
            _TIFFfree(psCache);
            return NULL;
        }

        psCache->nBytesPerRow = (toff_t)nBytesPerRow;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate and initialize the data buffers.                       */
    /* -------------------------------------------------------------------- */

    psCache->pabyRow1Blocks = (unsigned char *)_TIFFmalloc(nBytesPerRowSize);
    psCache->pabyRow2Blocks = (unsigned char *)_TIFFmalloc(nBytesPerRowSize);

    if (psCache->pabyRow1Blocks == NULL || psCache->pabyRow2Blocks == NULL)
    {
        TIFFErrorExt(hTIFF->tif_clientdata, hTIFF->tif_name,
                     "Can't allocate memory for overview cache.");
        /* TODO: use of TIFFError is inconsistent with use of fprintf in
         * addtiffo.c, sort out */
        if (psCache->pabyRow1Blocks)
            _TIFFfree(psCache->pabyRow1Blocks);
        if (psCache->pabyRow2Blocks)
            _TIFFfree(psCache->pabyRow2Blocks);
        _TIFFfree(psCache);
        return NULL;
    }

    _TIFFmemset(psCache->pabyRow1Blocks, 0, nBytesPerRowSize);
    _TIFFmemset(psCache->pabyRow2Blocks, 0, nBytesPerRowSize);

    psCache->nBlockOffset = 0;

    nRet = TIFFSetSubDirectory(psCache->hTIFF, nBaseDirOffset);
    (void)nRet;
    assert(nRet == 1);

    return psCache;
}

/************************************************************************/
/*                          TIFFWriteOvrRow()                           */
/*                                                                      */
/*      Write one entire row of blocks (row 1) to the tiff file, and    */
/*      then rotate the block buffers, essentially moving things        */
/*      down by one block.                                              */
/************************************************************************/

static void TIFFWriteOvrRow(TIFFOvrCache *psCache)

{
    int nRet, iTileX, iTileY = psCache->nBlockOffset;
    unsigned char *pabyData;
    toff_t nBaseDirOffset;
    uint32_t RowsInStrip;

    /* -------------------------------------------------------------------- */
    /*      If the output cache is multi-byte per sample, and the file      */
    /*      being written to is of a different byte order than the current  */
    /*      platform, we will need to byte swap the data.                   */
    /* -------------------------------------------------------------------- */
    if (TIFFIsByteSwapped(psCache->hTIFF))
    {
        uint64_t n;
        if (psCache->nBitsPerPixel == 16)
        {
            n = _TIFFMultiply64(psCache->hTIFF, psCache->nBytesPerBlock,
                                psCache->nSamples, "overview swab");
            if (n == 0 && psCache->nBytesPerBlock != 0 &&
                psCache->nSamples != 0)
                return;
            n /= 2;
#ifdef SIZEOF_SIZE_T
#if SIZEOF_SIZE_T <= 4
            if (n > INT32_MAX)
#else
            if (n > INT64_MAX)
#endif
#else
#pragma message(                                                               \
    "---- Error: SIZEOF_SIZE_T not defined. Generate a compile error. ----")
            SIZEOF_SIZE_T
#endif
            {
                TIFFErrorExt(TIFFClientdata(psCache->hTIFF),
                             "TIFFWriteOvrRow()",
                             "Integer overflow of number of 'short' to swap");
                n = 0;
            }
            TIFFSwabArrayOfShort((uint16_t *)psCache->pabyRow1Blocks,
                                 (tmsize_t)n);
        }

        else if (psCache->nBitsPerPixel == 32)
        {
            n = _TIFFMultiply64(psCache->hTIFF, psCache->nBytesPerBlock,
                                psCache->nSamples, "overview swab");
            if (n == 0 && psCache->nBytesPerBlock != 0 &&
                psCache->nSamples != 0)
                return;
            n /= 4;
#ifdef SIZEOF_SIZE_T
#if SIZEOF_SIZE_T <= 4
            if (n > INT32_MAX)
#else
            if (n > INT64_MAX)
#endif
#else
#pragma message(                                                               \
    "---- Error: SIZEOF_SIZE_T not defined. Generate a compile error. ----")
            SIZEOF_SIZE_T
#endif
            {
                TIFFErrorExt(TIFFClientdata(psCache->hTIFF),
                             "TIFFWriteOvrRow()",
                             "Integer overflow of number of 'long' to swap");
                n = 0;
            }
            TIFFSwabArrayOfLong((uint32_t *)psCache->pabyRow1Blocks,
                                (tmsize_t)n);
        }

        else if (psCache->nBitsPerPixel == 64)
        {
            n = _TIFFMultiply64(psCache->hTIFF, psCache->nBytesPerBlock,
                                psCache->nSamples, "overview swab");
            if (n == 0 && psCache->nBytesPerBlock != 0 &&
                psCache->nSamples != 0)
                return;
            n /= 8;
#ifdef SIZEOF_SIZE_T
#if SIZEOF_SIZE_T <= 4
            if (n > INT32_MAX)
#else
            if (n > INT64_MAX)
#endif
#else
#pragma message(                                                               \
    "---- Error: SIZEOF_SIZE_T not defined. Generate a compile error. ----")
            SIZEOF_SIZE_T
#endif
            {
                TIFFErrorExt(TIFFClientdata(psCache->hTIFF),
                             "TIFFWriteOvrRow()",
                             "Integer overflow of number of 'double' to swap");
                n = 0;
            }
            TIFFSwabArrayOfDouble((double *)psCache->pabyRow1Blocks,
                                  (tmsize_t)n);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Record original directory position, so we can restore it at     */
    /*      end.                                                            */
    /* -------------------------------------------------------------------- */
    nBaseDirOffset = TIFFCurrentDirOffset(psCache->hTIFF);
    nRet = TIFFSetSubDirectory(psCache->hTIFF, psCache->nDirOffset);
    (void)nRet;
    assert(nRet == 1);

    /* -------------------------------------------------------------------- */
    /*      Write blocks to TIFF file.                                      */
    /* -------------------------------------------------------------------- */
    for (iTileX = 0; iTileX < psCache->nBlocksPerRow; iTileX++)
    {
        uint32_t nTileID;

        if (psCache->nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            int iSample;

            for (iSample = 0; iSample < psCache->nSamples; iSample++)
            {
                pabyData = TIFFGetOvrBlock(psCache, iTileX, iTileY, iSample);

                if (psCache->bTiled)
                {
                    nTileID = TIFFComputeTile(
                        psCache->hTIFF, (uint32_t)iTileX * psCache->nBlockXSize,
                        (uint32_t)iTileY * psCache->nBlockYSize, 0,
                        (tsample_t)iSample);
                    if (TIFFWriteEncodedTile(psCache->hTIFF, nTileID, pabyData,
                                             TIFFTileSize(psCache->hTIFF)) < 0)
                    {
                        fprintf(stderr, "TIFFWriteEncodedTile() failed\n");
                    }
                }
                else
                {
                    uint64_t nTileY = _TIFFMultiply64(
                        psCache->hTIFF, (uint32_t)iTileY, psCache->nBlockYSize,
                        "overview strip offset");
                    uint64_t nNextTileY =
                        _TIFFAdd64(psCache->hTIFF, nTileY, psCache->nBlockYSize,
                                   "overview strip offset");
                    if ((nTileY == 0 && iTileY != 0) || nNextTileY == 0 ||
                        nTileY > UINT32_MAX || nTileY >= psCache->nYSize)
                        return;
                    nTileID = TIFFComputeStrip(psCache->hTIFF, (uint32_t)nTileY,
                                               (tsample_t)iSample);
                    RowsInStrip = psCache->nBlockYSize;
                    if (nNextTileY > psCache->nYSize)
                        RowsInStrip = (uint32_t)(psCache->nYSize - nTileY);
                    if (TIFFWriteEncodedStrip(
                            psCache->hTIFF, nTileID, pabyData,
                            TIFFVStripSize(psCache->hTIFF, RowsInStrip)) < 0)
                    {
                        fprintf(stderr, "TIFFWriteEncodedStrip() failed\n");
                    }
                }
            }
        }
        else
        {
            pabyData = TIFFGetOvrBlock(psCache, iTileX, iTileY, 0);

            if (psCache->bTiled)
            {
                nTileID = TIFFComputeTile(
                    psCache->hTIFF, (uint32_t)iTileX * psCache->nBlockXSize,
                    (uint32_t)iTileY * psCache->nBlockYSize, 0, 0);
                if (TIFFWriteEncodedTile(psCache->hTIFF, nTileID, pabyData,
                                         TIFFTileSize(psCache->hTIFF)) < 0)
                {
                    fprintf(stderr, "TIFFWriteEncodedTile() failed\n");
                }
            }
            else
            {
                uint64_t nTileY = _TIFFMultiply64(
                    psCache->hTIFF, (uint32_t)iTileY, psCache->nBlockYSize,
                    "overview strip offset");
                uint64_t nNextTileY =
                    _TIFFAdd64(psCache->hTIFF, nTileY, psCache->nBlockYSize,
                               "overview strip offset");
                if ((nTileY == 0 && iTileY != 0) || nNextTileY == 0 ||
                    nTileY > UINT32_MAX || nTileY >= psCache->nYSize)
                    return;
                nTileID = TIFFComputeStrip(psCache->hTIFF, (uint32_t)nTileY, 0);
                RowsInStrip = psCache->nBlockYSize;
                if (nNextTileY > psCache->nYSize)
                    RowsInStrip = (uint32_t)(psCache->nYSize - nTileY);
                if (TIFFWriteEncodedStrip(
                        psCache->hTIFF, nTileID, pabyData,
                        TIFFVStripSize(psCache->hTIFF, RowsInStrip)) < 0)
                {
                    fprintf(stderr, "TIFFWriteEncodedStrip() failed\n");
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Rotate buffers.                                                 */
    /* -------------------------------------------------------------------- */
    pabyData = psCache->pabyRow1Blocks;
    psCache->pabyRow1Blocks = psCache->pabyRow2Blocks;
    psCache->pabyRow2Blocks = pabyData;

    {
        tmsize_t nBytesPerRow = _TIFFCastUInt64ToSSize(
            psCache->hTIFF, psCache->nBytesPerRow, "overview cache row");
        if (nBytesPerRow == 0 && psCache->nBytesPerRow != 0)
            return;
        _TIFFmemset(pabyData, 0, nBytesPerRow);
    }

    psCache->nBlockOffset++;

    /* -------------------------------------------------------------------- */
    /*      Restore access to original directory.                           */
    /* -------------------------------------------------------------------- */
    TIFFFlush(psCache->hTIFF);
    /* TODO: add checks on error status return of TIFFFlush */
    TIFFSetSubDirectory(psCache->hTIFF, nBaseDirOffset);
    /* TODO: add checks on error status return of TIFFSetSubDirectory */
}

/************************************************************************/
/*                          TIFFGetOvrBlock()                           */
/************************************************************************/

/* TODO: make TIFF_Downsample handle iSample offset, so that we can
 * do with a single TIFFGetOvrBlock and no longer need
 * TIFFGetOvrBlock_Subsampled */
unsigned char *TIFFGetOvrBlock(TIFFOvrCache *psCache, int iTileX, int iTileY,
                               int iSample)

{
    uint64_t nBlockOffset;
    uint64_t nRowOffset;
    tmsize_t nRowOffsetSize;

    if (iTileY > psCache->nBlockOffset + 1)
        TIFFWriteOvrRow(psCache);

    assert(iTileX >= 0 && iTileX < psCache->nBlocksPerRow);
    assert(iTileY >= 0 && iTileY < psCache->nBlocksPerColumn);
    assert(iTileY >= psCache->nBlockOffset &&
           iTileY < psCache->nBlockOffset + 2);
    assert(iSample >= 0 && iSample < psCache->nSamples);

    if (psCache->nPlanarConfig == PLANARCONFIG_SEPARATE)
    {
        uint64_t nOffset =
            _TIFFMultiply64(psCache->hTIFF, (uint32_t)iTileX, psCache->nSamples,
                            "overview block offset");
        if (nOffset == 0 && iTileX != 0 && psCache->nSamples != 0)
            return NULL;
        nOffset = _TIFFAdd64(psCache->hTIFF, nOffset, (uint32_t)iSample,
                             "overview block offset");
        if (nOffset == 0 && (iTileX != 0 || iSample != 0))
            return NULL;
        nOffset =
            _TIFFMultiply64(psCache->hTIFF, nOffset, psCache->nBytesPerBlock,
                            "overview block offset");
        if (nOffset == 0 && psCache->nBytesPerBlock != 0 &&
            (iTileX != 0 || iSample != 0))
            return NULL;
        nBlockOffset = nOffset;
        nRowOffset = nOffset;
    }
    else
    {
        uint64_t nSampleOffset;
        uint64_t nOffset =
            _TIFFMultiply64(psCache->hTIFF, (uint32_t)iTileX,
                            psCache->nBytesPerBlock, "overview block offset");
        if (nOffset == 0 && iTileX != 0 && psCache->nBytesPerBlock != 0)
            return NULL;
        nBlockOffset = nOffset;
        nSampleOffset = _TIFFMultiply64(
            psCache->hTIFF, (uint32_t)((psCache->nBitsPerPixel + 7) / 8),
            (uint16_t)iSample, "overview block offset");
        if (nSampleOffset == 0 && iSample != 0)
            return NULL;
        nOffset = _TIFFAdd64(psCache->hTIFF, nOffset, nSampleOffset,
                             "overview block offset");
        if (nOffset == 0 && (iTileX != 0 || iSample != 0))
            return NULL;
        nRowOffset = nOffset;
    }

    {
        uint64_t nBlockEnd =
            _TIFFAdd64(psCache->hTIFF, nBlockOffset, psCache->nBytesPerBlock,
                       "overview block offset");
        if ((nBlockEnd == 0 &&
             (nBlockOffset != 0 || psCache->nBytesPerBlock != 0)) ||
            nBlockEnd > psCache->nBytesPerRow || nRowOffset < nBlockOffset ||
            nRowOffset >= nBlockEnd)
            return NULL;
    }

    nRowOffsetSize = _TIFFCastUInt64ToSSize(psCache->hTIFF, nRowOffset,
                                            "overview block offset");
    if (nRowOffsetSize == 0 && nRowOffset != 0)
        return NULL;

    if (iTileY == psCache->nBlockOffset)
        return psCache->pabyRow1Blocks + nRowOffsetSize;
    else
        return psCache->pabyRow2Blocks + nRowOffsetSize;
}

/************************************************************************/
/*                     TIFFGetOvrBlock_Subsampled()                     */
/************************************************************************/

unsigned char *TIFFGetOvrBlock_Subsampled(TIFFOvrCache *psCache, int iTileX,
                                          int iTileY)

{
    uint64_t nRowOffset;
    tmsize_t nRowOffsetSize;

    if (iTileY > psCache->nBlockOffset + 1)
        TIFFWriteOvrRow(psCache);

    assert(iTileX >= 0 && iTileX < psCache->nBlocksPerRow);
    assert(iTileY >= 0 && iTileY < psCache->nBlocksPerColumn);
    assert(iTileY >= psCache->nBlockOffset &&
           iTileY < psCache->nBlockOffset + 2);
    assert(psCache->nPlanarConfig != PLANARCONFIG_SEPARATE);

    nRowOffset =
        _TIFFMultiply64(psCache->hTIFF, (uint32_t)iTileX,
                        psCache->nBytesPerBlock, "overview block offset");
    if (nRowOffset == 0 && iTileX != 0)
        return NULL;

    {
        uint64_t nBlockEnd =
            _TIFFAdd64(psCache->hTIFF, nRowOffset, psCache->nBytesPerBlock,
                       "overview block offset");
        if ((nBlockEnd == 0 &&
             (nRowOffset != 0 || psCache->nBytesPerBlock != 0)) ||
            nBlockEnd > psCache->nBytesPerRow)
            return NULL;
    }

    nRowOffsetSize = _TIFFCastUInt64ToSSize(psCache->hTIFF, nRowOffset,
                                            "overview block offset");
    if (nRowOffsetSize == 0 && nRowOffset != 0)
        return NULL;

    if (iTileY == psCache->nBlockOffset)
        return psCache->pabyRow1Blocks + nRowOffsetSize;
    else
        return psCache->pabyRow2Blocks + nRowOffsetSize;
}

/************************************************************************/
/*                        TIFFDestroyOvrCache()                         */
/************************************************************************/

void TIFFDestroyOvrCache(TIFFOvrCache *psCache)

{
    while (psCache->nBlockOffset < psCache->nBlocksPerColumn)
        TIFFWriteOvrRow(psCache);

    _TIFFfree(psCache->pabyRow1Blocks);
    _TIFFfree(psCache->pabyRow2Blocks);
    _TIFFfree(psCache);
}
