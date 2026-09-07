/*
 * Copyright (c) 2022, Even Rouault <even.rouault at spatialys.com>
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

/* ===========  Purpose ===============================================
 * TIFF Library - Test IFD loop detection -
 * Tests the following points:
 *  - Detection of IFD loops when reading through the IFDs in the file,
 *    sequentially or arbitrarily.
 *  - Detection of IDF loops within chained Sub-IFDs.
 *  - Detection of IFD loops in TIFFLinkDirectory() when writing (appending)
 *    IFDs for Classic-TIFF and BigTIFF.
 *    (see  https://gitlab.com/libtiff/libtiff/-/work_items/788)
 *
 */

#include <assert.h>
#include <stdbool.h> /* used for boolean true and false */
#include <stdio.h>
#include <stdlib.h> /* necessary for linux compiler */
#include <string.h>

#include "tif_config.h" /* necessary for linux compiler to get HAVE_UNISTD_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for unlink() on linux */
#endif

#include "tiffio.h"

// #define DEBUG_TESTING
#ifdef DEBUG_TESTING
#define GOTOFAILURE                                                            \
    {                                                                          \
    }
#else
/*  Only for automake and CMake infrastructure the test should:
    a.) delete any written testfiles when test passed
        (otherwise autotest will fail)
    b.) goto failure, if any failure is detected, which is not
        necessary when test is initiated manually for debugging.
*/
#define GOTOFAILURE goto failure;
#endif

const char *openModeStrings[] = {"wl", "wb", "w8l", "w8b"};
const char *openModeText[] = {"non-BigTIFF and LE", "non-BigTIFF and BE",
                              "BigTIFF and LE", "BigTIFF and BE"};

#define TIFFWriteDirectory_M(tif, filename, line)                              \
    if (!TIFFWriteDirectory(tif))                                              \
    {                                                                          \
        fprintf(stderr, "Can't write directory to %s at line %d\n", filename,  \
                line);                                                         \
        goto failure;                                                          \
    }

#define TIFFSetDirectory_M(tif, dirnum, filename, line)                        \
    if (!TIFFSetDirectory(tif, dirnum))                                        \
    {                                                                          \
        fprintf(stderr, "Can't set directory %u of %s at line %d\n", dirnum,   \
                filename, line);                                               \
        goto failure;                                                          \
    }

#define TIFFSetField_M(tif, tag, val, line)                                    \
    if (!TIFFSetField(tif, tag, val))                                          \
    {                                                                          \
        fprintf(stderr, "Can't set tag %d at line %d.\n", tag, line);          \
        return 1;                                                              \
    }

/* Writes basic tags to current directory (IFD) as well one pixel to the file.
 * For is_corrupted = TRUE a corrupted IFD (missing image width tag) is
 * generated. */
#define SPP 3
static int write_data_to_current_directory(TIFF *tif, int i, bool is_corrupted)
{
    const uint16_t width = 1;
    const uint16_t length = 1;
    const uint16_t bps = 8;
    const uint16_t photometric = PHOTOMETRIC_RGB;
    const uint16_t rows_per_strip = 1;
    const uint16_t planarconfig = PLANARCONFIG_CONTIG;
    unsigned char buf[SPP] = {0, 127, 255};
    char auxString[128];

    if (!tif)
    {
        fprintf(stderr, "Invalid TIFF handle.\n");
        return 1;
    }
    if (!is_corrupted)
    {
        TIFFSetField_M(tif, TIFFTAG_IMAGEWIDTH, width, __LINE__);
    }
    TIFFSetField_M(tif, TIFFTAG_IMAGELENGTH, length, __LINE__);
    TIFFSetField_M(tif, TIFFTAG_BITSPERSAMPLE, bps, __LINE__);
    TIFFSetField_M(tif, TIFFTAG_SAMPLESPERPIXEL, SPP, __LINE__);
    TIFFSetField_M(tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip, __LINE__);
    TIFFSetField_M(tif, TIFFTAG_PLANARCONFIG, planarconfig, __LINE__);
    TIFFSetField_M(tif, TIFFTAG_PHOTOMETRIC, photometric, __LINE__);
    /* Write IFD identification number to ASCII string of PageName tag. */
    sprintf(auxString, "%d th. IFD", i);
    TIFFSetField_M(tif, TIFFTAG_PAGENAME, auxString, __LINE__);
    /* Write dummy pixel data. */
    if (TIFFWriteScanline(tif, buf, 0, 0) == -1 && !is_corrupted)
    {
        fprintf(stderr, "Can't write image data.\n");
        return 1;
    }
    return 0;
} /*--- write_data_to_current_directory() ---*/

/* Patch the next directory offset of IFD to a new value.
 * The IFD is given by its dir_offset.
 */
static int patch_IFD_next_offset(TIFF *tif, uint64_t dir_off,
                                 uint64_t new_next_off_value)
{
#define TIFFReadFile_M(tif, buf, size)                                         \
    ((uint64_t)(*TIFFGetReadProc(tif))(TIFFClientdata(tif), (buf), (size)));
#define TIFFWriteFile_M(tif, buf, size)                                        \
    ((uint64_t)(*TIFFGetWriteProc(tif))(TIFFClientdata(tif), (buf), (size)));
#define TIFFSeekFile_M(tif, off, whence)                                       \
    ((*TIFFGetSeekProc(tif))(TIFFClientdata(tif), (off), (whence)));

    if (!TIFFIsBigTIFF(tif))
    {
        /*-- Classic TIFF --*/
        /* Get location of nextIFDOffset of IFD. */
        uint64_t ss = TIFFSeekFile_M(tif, dir_off, 0);
        uint16_t cnt = 0;
        uint64_t rr = TIFFReadFile_M(tif, &cnt, 2);
        if (TIFFIsByteSwapped(tif))
            TIFFSwabShort(&cnt);
        ss = TIFFSeekFile_M(tif, dir_off + cnt * 12U + 2U, 0);
        /* Patch offset with new value. */
        uint32_t wt;
        if (new_next_off_value <= UINT32_MAX)
        {
            wt = (uint32_t)new_next_off_value;
        }
        else
        {
            fprintf(stderr,
                    "Error: Next offset exceeds Classic TIFF uint32 size.\n");
            return 1;
        }
        if (TIFFIsByteSwapped(tif))
            TIFFSwabLong(&wt);
        rr = TIFFWriteFile_M(tif, &wt, 4);
        (void)ss; /* ss, rr for debugging purposes - avoiding warnings */
        (void)rr;
    }
    else
    {
        /*-- BigTIFF --*/
        /* Get location of nextIFDOffset of IFD. */
        uint64_t ss = TIFFSeekFile_M(tif, dir_off, 0);
        uint64_t cnt64 = 0;
        uint64_t rr = TIFFReadFile_M(tif, &cnt64, 8);
        if (TIFFIsByteSwapped(tif))
            TIFFSwabLong8(&cnt64);
        ss = TIFFSeekFile_M(tif, dir_off + cnt64 * 20U + 8U, 0);
        /* Patch offset with new value. */
        uint64_t wt64;
        wt64 = new_next_off_value;
        if (TIFFIsByteSwapped(tif))
            TIFFSwabLong8(&wt64);
        rr = TIFFWriteFile_M(tif, &wt64, 8);
        (void)ss; /* ss, rr for debugging purposes - avoiding warnings */
        (void)rr;
    }
    return 0;
} /*--- patch_IFD_next_offset() ---*/

/* Compare 'requested_dir_number' with number written in PageName tag
 * into the IFD to identify that IFD.  */
static int is_requested_directory(TIFF *tif, int requested_dir_number,
                                  const char *filename)
{
    char *ptr = NULL;
    char *auxStr = NULL;

    if (!TIFFGetField(tif, TIFFTAG_PAGENAME, &ptr))
    {
        fprintf(stderr, "Can't get TIFFTAG_PAGENAME tag.\n");
        return 0;
    }

    /* Check for reading errors */
    if (ptr != NULL)
        auxStr = strchr(ptr, ' ');

    if (ptr == NULL || auxStr == NULL || strncmp(auxStr, " th.", 4))
    {
        fprintf(stderr,
                "Error reading IFD directory number from PageName tag: %s\n",
                ptr == NULL ? "(null)" : ptr);
        return 0;
    }

    /* Retrieve IFD identification number from ASCII string */
    const int nthIFD = atoi(ptr);
    if (nthIFD == requested_dir_number)
    {
        return 1;
    }
    fprintf(stderr, "Expected directory %d from %s was not loaded but: %s\n",
            requested_dir_number, filename, ptr);

    return 0;
} /*--- is_requested_directory() ---*/

/* Test loop detection with different pre-generated test files for reading.
 * Different movements through the IFD chain are tested. */
static int test_ifd_loop(void)
{
    int ret = 0;
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_self.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(1) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_self.tif", "r");
        assert(tif);
        int n = (int)TIFFNumberOfDirectories(tif);
        if (n != 1)
        {
            fprintf(stderr,
                    "(2) Expected TIFFNumberOfDirectories() to return 1. "
                    "Got %d\n",
                    n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_first.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(3) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(4) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(5) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(6) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(7) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(8) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(9) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif =
            TIFFOpen(SOURCE_DIR "/images/test_ifd_loop_to_first.tif", "r");
        assert(tif);
        int n = (int)TIFFNumberOfDirectories(tif);
        if (n != 2)
        {
            fprintf(stderr,
                    "(10) Expected TIFFNumberOfDirectories() to return 2. "
                    "Got %d\n",
                    n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif = TIFFOpen(SOURCE_DIR "/images/test_two_ifds.tif", "r");
        assert(tif);
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(11) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(12) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(13) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(14) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(15) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif) != 1)
        {
            fprintf(stderr, "(16) Expected TIFFReadDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFReadDirectory(tif))
        {
            fprintf(stderr, "(17) Expected TIFFReadDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(18) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(19) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 2))
        {
            fprintf(stderr, "(20) Expected TIFFSetDirectory() to fail\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 1) != 1)
        {
            fprintf(stderr, "(21) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        if (TIFFSetDirectory(tif, 0) != 1)
        {
            fprintf(stderr, "(22) Expected TIFFSetDirectory() to succeed\n");
            ret = 1;
        }
        TIFFClose(tif);
    }
    {
        TIFF *tif = TIFFOpen(SOURCE_DIR "/images/test_two_ifds.tif", "r");
        assert(tif);
        int n = (int)TIFFNumberOfDirectories(tif);
        if (n != 2)
        {
            fprintf(stderr,
                    "(23) Expected TIFFNumberOfDirectories() to return 2. "
                    "Got %d\n",
                    n);
            ret = 1;
        }
        TIFFClose(tif);
    }
    return ret;
} /*--- test_ifd_loop() ---*/

/* Test loop detection within chained SubIFDs.
 * test_ifd_loop_subifd.tif contains seven main-IFDs (0 to 6) and within IFD
 * 1 there are three SubIFDs (0 to 2). Main IFD 4 loops back to main IFD 2.
 * SubIFD 2 loops back to SubIFD 1.
 * Within each IFD the tag PageName is filled with a string, indicating the
 * IFD. The main IFDs are numbered 0 to 6 and the SubIFDs 200 to 202. */
static int test_subifd_loop(void)
{
    const char *filename = SOURCE_DIR "/images/test_ifd_loop_subifd.tif";
    TIFF *tif;
    int i, n;
    int ret = 0;
#define NUMBER_OF_SUBIFDs 3
    toff_t sub_IFDs_offsets[NUMBER_OF_SUBIFDs] = {
        0UL}; /* array for SubIFD tag */
    void *ptr;
    uint16_t number_of_sub_IFDs = 0;

    tif = TIFFOpen(filename, "r");
    if (!tif)
    {
        fprintf(stderr, "Can't open  %s\n", filename);
        return 1;
    }

    /* Try to read six further main directories. Fifth read shall fail. */
    for (i = 0; i < 6; i++)
    {
        if (!TIFFReadDirectory(tif))
            break;
    }
    if (i != 4)
    {
        fprintf(stderr, "(30) Expected fifth TIFFReadDirectory() to fail\n");
        ret = 1;
    }
    if (!is_requested_directory(tif, 4, filename))
    {
        fprintf(stderr, "(31) Expected fifth main IFD to be loaded\n");
        ret = 1;
    }

    /* Switch to IFD 1 and get SubIFDs.
     * Then read through SubIFDs and detect SubIFD loop.
     * Finally go back to main-IFD and check if right IFD is loaded.
     */
    if (!TIFFSetDirectory(tif, 1))
        ret = 1;

    /* Check if there are SubIFD subfiles */
    if (TIFFGetField(tif, TIFFTAG_SUBIFD, &number_of_sub_IFDs, &ptr) &&
        (number_of_sub_IFDs == 3))
    {
        /* Copy SubIFD array from pointer */
        memcpy(sub_IFDs_offsets, ptr,
               number_of_sub_IFDs * sizeof(sub_IFDs_offsets[0]));

        for (i = 0; i < number_of_sub_IFDs; i++)
        {
            /* Read SubIFD directory directly via offset.
             * SubIFDs PageName string contains numbers 200 to 202. */
            if (!TIFFSetSubDirectory(tif, sub_IFDs_offsets[i]))
                ret = 1;
            if (!is_requested_directory(tif, 200 + i, filename))
            {
                fprintf(stderr, "(32) Expected SubIFD %d to be loaded.\n", i);
                ret = 1;
            }
            /* Now test SubIFD loop detection.
             * The (i+n).th read in the SubIFD chain shall fail. */
            for (n = 0; n < number_of_sub_IFDs; n++)
            {
                if (!TIFFReadDirectory(tif))
                    break;
            }
            if ((i + n) != 2)
            {
                fprintf(stderr, "(33) Expected third "
                                "SubIFD-TIFFReadDirectory() to fail\n");
                ret = 1;
            }
        }
        /* Go back to main-IFD chain and re-read that main-IFD directory */
        if (!TIFFSetDirectory(tif, 3))
            ret = 1;
        if (!is_requested_directory(tif, 3, filename))
        {
            fprintf(stderr, "(34) Expected fourth main IFD to be loaded\n");
            ret = 1;
        }
    }
    else
    {
        fprintf(stderr, "(35) No or wrong expected SubIFDs within main IFD\n");
        ret = 1;
    }

    TIFFClose(tif);
    return ret;
} /*-- test_subifd_loop() --*/

/* Test IFD loop detection in TIFFLinkDirectory() when writing (appending) IFDs.
 */
static int test_write_link_loop(unsigned int openMode)
{
    char filename[128] = {0};
    TIFF *tif;
    uint64_t offsetBase[4];

    if (openMode >= (sizeof(openModeStrings) / sizeof(openModeStrings[0])))
    {
        fprintf(stderr, "Index %u for openMode parameter out of range.\n",
                openMode);
        return 1;
    }

    /* Get individual filenames and delete existent ones. */
    sprintf(filename, "test_ifd_loop_write_link_%s.tif",
            openModeStrings[openMode]);
#ifndef DEBUG_TESTING
    unlink(filename);
#endif

    /*-- Test for IFD loop in first and in second IFD --*/
    for (int i = 0; i < 2; i++)
    {
        fprintf(stderr, "  ... Loop in %d IFD ...\n", i);
        /* Prepare file with number "i" IFDs for later IFD loop patching. */
        tif = TIFFOpen(filename, openModeStrings[openMode]);
        if (!tif)
        {
            fprintf(stderr, "Can't open  %s\n", filename);
            return 1;
        }
        for (int k = 0; k <= i; k++)
        {
            if (write_data_to_current_directory(tif, k, false))
                goto failure;
            TIFFWriteDirectory_M(tif, filename, __LINE__);
            TIFFSetDirectory_M(tif, (tdir_t)k, filename, __LINE__);
            /* Get IFD offsets to patch next offset link. */
            offsetBase[k] = TIFFCurrentDirOffset(tif);
            TIFFCreateDirectory(tif);
        }
        /* Link i.th IFD back to the first IFD. */
        if (patch_IFD_next_offset(tif, offsetBase[i], offsetBase[0]))
        {
            fprintf(stderr, "Can't patch IFD offset.\n");
            goto failure;
        }
        TIFFClose(tif);

        /* Test to append another IFD - causing issue #788.
         * TIFFOpen() in append does not read the first, looped IFD
         * but setup a default-IFD for writing! */
        tif = TIFFOpen(filename, "a");
        if (tif == NULL)
        {
            fprintf(stderr, "TIFFOpen(%s, \"a\") failed\n", filename);
            goto failure;
        }
        /* Trigger TIFFLinkDirectory() from write path.
         * Error from TIFFWriteDirectory() expected. */
        if (TIFFWriteDirectory(tif))
        {
            fprintf(
                stderr,
                "Error return expected but appending directory to %s at line "
                "%d succeedes for IFD %d\n",
                filename, __LINE__, i);
            GOTOFAILURE;
        }
        TIFFClose(tif);
    }

#ifndef DEBUG_TESTING
    unlink(filename);
#endif
    return 0;

failure:
    fprintf(stderr,
            "--- Error in test_write_link_loop() line %d at file %s ---\n",
            __LINE__, filename);
    if (tif)
    {
        TIFFClose(tif);
    }
    return 1;
} /*--- test_write_link_loop() ---*/

int main(void)
{
    int ret = 0;

    /*--- IFD loop testing ---*/
    fprintf(stderr, "\n--- Test for IFD loops. ---\n");
    ret += test_ifd_loop();

    /*--- IFD loop testing for Sub-IFDs ---*/
    fprintf(stderr, "\n--- Test for Sub-IFD loops. ---\n");
    ret += test_subifd_loop();

    /*--- IFD loop testing for writing ---*/
    fprintf(stderr, "\n--- Test for IFD loops when writing (appending). ---\n");
    unsigned int openModeMax =
        (sizeof(openModeStrings) / sizeof(openModeStrings[0]));
    for (unsigned int openMode = 0; openMode < openModeMax; openMode++)
    {
        fprintf(stderr, "\n  -- testing with %s open option. --\n",
                openModeText[openMode]);
        ret += test_write_link_loop(openMode);
    }

    return ret;
}
