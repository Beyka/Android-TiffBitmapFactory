/* 
 ReadTiffIncremental. Read large Tiff Files without reading entire file into memory.
 Modified 1-Nov-2015. Modifications Copyright 2015 by Dennis Damico.

 This code is based on tifffastcrop v1.3.4
 http://www.imnc.in2p3.fr/pagesperso/deroulers/software/largetifftools/
 Copyright (c) 2013-2015 Christophe Deroulers
 Portions are based on libtiff's tiffcp code. tiffcp is:
 Copyright (c) 1988-1997 Sam Leffler
 Copyright (c) 1991-1997 Silicon Graphics, Inc.
 Distributed under the GNU General Public License v3 -- contact the author for commercial use 
 */

#ifdef __cplusplus
extern "C" {
#endif
    
#include <tiffio.h>
#include "readTiffIncremental.h"

// Return codes.
#define EXIT_SUCCESS              0
#define ERROR_TILE                1
#define ERROR_STRIP               2
#define ERROR_MEMORY              3

// Input characteristics (ARGB):
#define inBytesPerPixel           4

// Output characterics (ARGB):
#define outBytesPerPixel          4
#define outBitsPerSample          8
#define outSamplesPerPixel        4

// End of output buffer. Used to guard against buffer overflow.
static unsigned char* outBufMax = 0;

/**
 * Fix orientation on a strip or tile.
 * Original comment: For some reason the TIFFReadRGBATile() function chooses the
 * lower left corner as the origin.  Vertically mirror scanlines.
 * Copied from tiff2rgba.c and modified to support all orientations.
 * @param orientation The image orientation constant.
 * @param width The width of the strip or tile.
 * @param height The height of the strip or tile.
 * @param buf The input buffer.
 * @param temp The temp buffer.
 */
static void fixOrientationPre(int orientation, int width, int height, unsigned char* buf, uint32* temp) {
    switch (orientation) {
      case 0: case 1: case 2: case 5: case 6:
            // Mirror top and bottom.
            for (uint32 row = 0; row < height / 2; row++) {
                unsigned char* topLine = buf + width * inBytesPerPixel * row;
                unsigned char* bottomLine = buf + width * inBytesPerPixel * (height - row - 1);
                _TIFFmemcpy(temp, topLine, inBytesPerPixel * width);
                _TIFFmemcpy(topLine, bottomLine, inBytesPerPixel * width);
                _TIFFmemcpy(bottomLine, temp, inBytesPerPixel * width);
            }
            break;
      case 3: case 4: case 7: case 8:
            // Mirror left and right.
            for (uint32 row = 0; row < height; row++) {
                for (uint32 col = 0; col < width / 2; col++) {
                    unsigned char* firstBytePtr = buf + (col * inBytesPerPixel) + (width * inBytesPerPixel * row);
                    unsigned char* lastBytePtr = buf + ((width - col - 1) * inBytesPerPixel) + (width * inBytesPerPixel * row);
                    for (int bp = 0; bp < inBytesPerPixel; bp++) {
                        unsigned char aByte = *firstBytePtr;
                        *firstBytePtr++ = *lastBytePtr;
                        *lastBytePtr++ = aByte;
                    }
                }
            }
            break;
    }
}

/**
 * Fix orientation on the final sampled file.
 * Original comment: For some reason the TIFFReadRGBATile() function chooses the
 * lower left corner as the origin.  Vertically mirror scanlines.
 * Copied from tiff2rgba.c and modified to support all orientations.
 * @param orientation The image orientation constant.
 * @param width The width of the image.
 * @param height The height of the image.
 * @param buf The input buffer.
 * @param temp The temp buffer.
 */
static void fixOrientationPost(int orientation, int width, int height, unsigned char* buf, uint32* temp) {
    switch (orientation) {
      case 3: case 4: case 7: case 8:
	// Rotate 180
	for (uint32 row = 0; row < height / 2; row++) {
                for (uint32 aCol = 0; aCol < width; aCol++) {
                    unsigned char* firstBytePtr = buf + (aCol * outBytesPerPixel) + (width * outBytesPerPixel * row);
                    unsigned char* lastBytePtr = buf + ((width - aCol - 1) * outBytesPerPixel) + (width * outBytesPerPixel * (height - row - 1));
                    for (int bp = 0; bp < outBytesPerPixel; bp++) {
                        unsigned char aByte = *firstBytePtr;
                        *firstBytePtr++ = *lastBytePtr;
                        *lastBytePtr++ = aByte;
                    }
                }
            }
            break;
    }
}
/**
 * Given one row of image data, copy appropriate samples to an output buffer.
 * @param inbuf A pointer to the row buffer.
 * @param inbufsize The size in bytes of the row buffer.
 * @param outbuf A pointer to the destination buffer.
 * @param sampleSize The sample size.
 */
static void copyRGBASamples(uint8* inbuf, uint32 inbufsize, uint8* outbuf, uint32 sampleSize) {
    // Test each column in the row for sampling.
    for (uint32 col = 0; col < (inbufsize / inBytesPerPixel); col++) {
        if (col % sampleSize == 0) {
            // Sample this column. Determine the output column to receive this pixel.
            uint32 outCol = col / sampleSize;
            
            // Move the pixels from the input buffer to the output buffer.
            uint8* inPtr = inbuf + col * inBytesPerPixel;
            uint8* outPtr = outbuf + outCol * outBytesPerPixel;
            
            // Check for buffer overflow. Should never happen.
            if ((outPtr + outBytesPerPixel) > (outBufMax + 1)) {
                return;
            }
            // Move RGBA from in to out.
            for (uint32 b = 0; b < outBytesPerPixel; b++) {
                *outPtr++ = *inPtr++;
            }
        }
    }
}

/**
 * Sample pixels from a tiled file and copy to a single strip output buffer.
 * @param TIFFin Tiff input image file.
 * @param inBuf buffer allocated for reading a tile.
 * @param outSampleSize Requested sample size.
 * @param imageWidth input image width.
 * @param imageHeight input image height.
 * @param outBuf buffer allocated for entire output file.
 * @param tempBuf buffer allocated for row manipulation.
 * @return An exit code.
 */
static int
cpTiles2Strip(TIFF* TIFFin, unsigned char* inBuf, int outSampleSize, 
    uint32 imageWidth, uint32 imageHeight, unsigned char* outBuf, uint32* tempBuf) {

    // Get information from input file.
    uint32 inTileWidth;                // Width in pixels of a tile.
    uint32 inTileHeight;               // Height in pixels of a tile.
    uint16 orientation = 1; // Image orientation. Default to 1 if there is no tag.
    TIFFGetField(TIFFin, TIFFTAG_TILEWIDTH, &inTileWidth);
    TIFFGetField(TIFFin, TIFFTAG_TILELENGTH, &inTileHeight);
    TIFFGetField(TIFFin, TIFFTAG_ORIENTATION, &orientation);

    // Compute sizes.
    tsize_t outBytesPerRow = (imageWidth / outSampleSize) * outBytesPerPixel;

    // Compute the number of horizontal (x) and vertical (y) tiles.
    uint32 xTileCount = imageWidth / inTileWidth;
    if (imageWidth % inTileWidth != 0) xTileCount++;
    uint32 yTileCount = imageHeight  / inTileHeight;
    if (imageHeight % inTileHeight != 0) yTileCount++;

    // Iterate through the vertical and horizontal tiles to build rows of the output image.
    // For each vertical tile...
    for (uint32 yt = 0; yt < yTileCount; yt++) {
        // Compute coordinates of this tile.
        uint32 yOfTile = yt * inTileHeight;           // Absolute y beginning of a tile.
        uint32 yEndOfTile = yOfTile + inTileHeight;   // Absolute y end of a full tile (+1).
        if (yEndOfTile > imageHeight) yEndOfTile = imageHeight; // Last tile may be a short tile.

        // Compute number of rows in this tile.
        uint32 tileRowCount = yEndOfTile - yOfTile;

        // For each horizontal tile...
        for (uint32 xt = 0; xt < xTileCount; xt++) {
            uint32 xOfTile = xt * inTileWidth;          // Absolute x beginning of a tile.
            uint32 xEndOfTile = xOfTile + inTileWidth;  // Absolute x end of a full tile (+1).
            if (xEndOfTile > imageWidth) xEndOfTile = imageWidth; // Last tile may be a short tile.

            // Compute number of columns and bytes in this tile.
            uint32 tileColCount = xEndOfTile - xOfTile;
            tsize_t tileRowBytes = tileColCount * inBytesPerPixel;

            // Read the tile containing coordinates xOfTile, yOfTile.
            // Reads a full-size tile padded with nulls for short tiles.
            if ( ! TIFFReadRGBATile(TIFFin, xOfTile, yOfTile, (uint32*) inBuf)) {
                return (ERROR_TILE);
            }

	    // Raster array has 4-byte unsigned integer type, that is why
	    // we should rearrange it here. Copied from tiff2rgba.c.
            #if HOST_BIGENDIAN
            uint32* raster = (uint32*) inBuf;
            TIFFSwabArrayOfLong(raster, inTileWidth * inTileHeight);
            #endif
            
            // Fix orientaton on the tile.
            fixOrientationPre(orientation, inTileWidth, inTileHeight, inBuf, tempBuf);

            // Sample the tile. Iterate through the rows of the tile.
            // Be careful not to exceed output row count.
            for (uint32 rr = 0; rr < (tileRowCount / outSampleSize) * outSampleSize; rr++) {
                // Test row number for sampling.
                if (rr % outSampleSize == 0) {
                    unsigned char* inPtr = inBuf + rr * inTileHeight * inBytesPerPixel;
                    unsigned char* outPtr = outBuf 
                            + (xOfTile/outSampleSize) * outBytesPerPixel
                            + ((yOfTile + rr)/outSampleSize) * outBytesPerRow;
                    copyRGBASamples(inPtr, tileRowBytes, outPtr, outSampleSize);
                }
            }
        }
    }
    
    // Fix orientation on the final file buffer.
    fixOrientationPost(orientation, imageWidth / outSampleSize, imageHeight / outSampleSize, outBuf, tempBuf);
    return EXIT_SUCCESS;
}

/**
 * Sample pixels from a stripped file and copy to a single strip output buffer.
 * @param TIFFin Tiff input image file.
 * @param inBuf buffer allocated for reading a scanline.
 * @param outSampleSize Requested sample size.
 * @param imageWidth input image width.
 * @param imageHeight input image height.
 * @param outbuf buffer allocated for entire output file.
 * @param tempBuf buffer allocated for row manipulation.
 * @return An exit code.
 */
static int
cpStrips2Strip(TIFF* TIFFin, unsigned char* inBuf, uint32 outSampleSize, 
        uint32 imageWidth, uint32 imageHeight, unsigned char* outBuf, uint32* tempBuf) {

    // Get information from input file.
    uint32 rowsPerStrip;
    uint16 orientation = 1; // Image orientation. Default to 1 if there is no tag.
    TIFFGetField(TIFFin, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
    TIFFGetField(TIFFin, TIFFTAG_ORIENTATION, &orientation);
    
    // Compute sizes.
    tsize_t outBytesPerRow = (imageWidth / outSampleSize) * outBytesPerPixel;

    // Loop over the strips.
    for (uint32 stripRow = 0; stripRow < imageHeight; stripRow += rowsPerStrip) {
        /* Read a strip into an RGBA array */
        if ( ! TIFFReadRGBAStrip(TIFFin, stripRow, (uint32*) inBuf)) {
            return (ERROR_STRIP);
        }

	// Raster array has 4-byte unsigned integer type, that is why
	// we should rearrange it here. Copied from tiff2rgba.c.
        #if HOST_BIGENDIAN
        uint32* raster = (uint32*) inBuf;
	TIFFSwabArrayOfLong(raster, imageWidth * rowsPerStrip);
        #endif

        // Figure out the number of scanlines actually in this strip.
        int stripRowCount;
        if(stripRow + rowsPerStrip > imageHeight)
            stripRowCount = imageHeight - stripRow;
        else
            stripRowCount = rowsPerStrip;

        // Fix orientaton on the strip.
        fixOrientationPre(orientation, imageWidth, stripRowCount, inBuf, tempBuf);

        // Sample the strip. Iterate through the rows of the strip.
        for (uint32 y = stripRow; y < stripRow + stripRowCount; y++) {
            // Test row number for sampling.
            if (y % outSampleSize == 0) {
                uint8* inBufPtr = inBuf + (y - stripRow) * imageWidth * inBytesPerPixel;
                // Determine the output row number and row address to receive sampled pixels.
                uint32 outRow = y / outSampleSize;
                uint8* outRowPtr = outBuf + outRow * outBytesPerRow;
                copyRGBASamples(inBufPtr, imageWidth * inBytesPerPixel, outRowPtr, outSampleSize);
            }
        }
    }
    
    // Fix orientation on the final file buffer.
    fixOrientationPost(orientation, imageWidth / outSampleSize, imageHeight / outSampleSize, outBuf, tempBuf);
    return EXIT_SUCCESS;
}

/**
 * Read a Tiff file in an incremental fashion. This means Tiled files are read
 * one tile at a time, Stripped files are read one strip at a time. Tiles and
 * strips are sampled after reading and before being copied to the output 
 * buffer.
 * @param in The input and already opened Tiff file.
 * @param outBuf The pointer to the unallocated buffer pointer to receive the entire sampled Tiff file. 
 * @param inSampleSize The requested sample size.
 * @param availableMemoryBytes The number of memory bytes that may be allocated.
 * @return Success or Error code. Success == 0.
 */
int
readTiffIncremental(JNIEnv *env, TIFF* in, unsigned char** outBuf, uint32 inSampleSize, int32 availableMemoryBytes, jstring path) {
    // Get input file information.
    uint32 fileImageWidth;
    uint32 fileImageHeight;
    TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &fileImageWidth);
    TIFFGetField(in, TIFFTAG_IMAGELENGTH, &fileImageHeight);

    // Compute memory needed to hold output file buffer.
    uint64_t outBufSize = (fileImageWidth / inSampleSize) * (fileImageHeight / inSampleSize)
            * outSamplesPerPixel * (outBitsPerSample / 8);
       
    // Allocate memory to hold input and output file buffers and temp buffer.
    uint32 tempBufSize;
    uint64_t inBufSize;
    unsigned char* inBuf;
    
    // Input and temp buffers.
    if (TIFFIsTiled(in)) { // Tiles.
        // Allocate a buffer to hold one tile.
        uint32 inTileWidth;                // Width in pixels of a tile.
        uint32 inTileHeight;               // Height in pixels of a tile.
        TIFFGetField(in, TIFFTAG_TILEWIDTH, &inTileWidth);
        TIFFGetField(in, TIFFTAG_TILELENGTH, &inTileHeight);
        inBufSize = inTileWidth * inTileHeight * inBytesPerPixel;
        // Allocate a temp buffer to hold one line of one tile.
        tempBufSize = inTileWidth * inBytesPerPixel;
    }
    else { // Strips
        // Allocate a buffer to hold one input strip.
        uint32 rowsPerStrip = 0;
        TIFFGetField(in, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
	if (rowsPerStrip == 0) {
            // Rows per strip is not present. Cannot continue.
            return (ERROR_STRIP);
        }
        inBufSize = fileImageWidth * rowsPerStrip * inBytesPerPixel;
        // Allocate a buffer to hold one line of one strip.
        tempBufSize = fileImageWidth * inBytesPerPixel;
    }
    
    // Check memory requests against limit.
    if (availableMemoryBytes <= (inBufSize + outBufSize + tempBufSize)) {
        throw_not_enought_memory_exception(env, availableMemoryBytes);
        return (ERROR_MEMORY);
    }
    
    // Allocate input buffer.
    inBuf = (unsigned char*)_TIFFmalloc(inBufSize);
    if (!inBuf) {
        throw_not_enought_memory_exception(env, availableMemoryBytes);
        return (ERROR_MEMORY);
    }
    
    // Allocate output buffer.
    *outBuf = (unsigned char*) _TIFFmalloc(outBufSize);
    if (!*outBuf) {
        _TIFFfree(inBuf);
        throw_not_enought_memory_exception(env, availableMemoryBytes);
        return (ERROR_MEMORY);
    }
    // Last byte of output buffer for overflow protection.
    outBufMax = *outBuf + outBufSize;
    
    // Allocate a temporary buffer for swapping during the vertical mirroring pass.
    uint32* tempBuf = (uint32*)_TIFFmalloc(tempBufSize);
    if (!tempBuf) {
        _TIFFfree(inBuf);
        _TIFFfree(*outBuf);
        throw_not_enought_memory_exception(env, availableMemoryBytes);
        return (ERROR_MEMORY);
    }

    // Read and sample the input file into one strip.
    int success;
    if (TIFFIsTiled(in)) { // Tiles.
        // Copy the tiles to one strip.
        success = cpTiles2Strip(in, inBuf, inSampleSize, fileImageWidth, fileImageHeight, *outBuf, tempBuf);
    }
    else { // Strips
        // Copy the file strips to one strip.
        success = cpStrips2Strip(in, inBuf, inSampleSize, fileImageWidth, fileImageHeight, *outBuf, tempBuf);
    }
    
    if (success != EXIT_SUCCESS) {
        _TIFFfree(inBuf);
        _TIFFfree(*outBuf);
        _TIFFfree(tempBuf);

        throw_read_file_exception(env, path);

        return (success);
    }
    
    // Free memory and return outbuf.
    _TIFFfree(inBuf);
    _TIFFfree(tempBuf);
    return success;
}

#ifdef __cplusplus
}
#endif