/*
 * Copyright (c) 2022, Su Laus  @Su_Laus
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * the author may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of the author.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* ===========  Purpose ===============================================
 * Tests the following points:
 *  - Handling of signed tags
 *  - Definition of additional, user-defined tags
 *  - Specification of field name strings or with field_name = NULL
 *  - Prevent reading anonymous tags by specifying them as FIELD_IGNORE
 *    (see https://gitlab.com/libtiff/libtiff/-/issues/532)
 *  - Immediate clearing of the memory for the definition of the additional tags
 *    (allocate memory for TIFFFieldInfo structure and free that memory
 *    immediately after calling TIFFMergeFieldInfo().
 *  - Handling of TIFF_LONG8 and TIFF_IFD8 tags after alignment of write-
 *    and read-functions. Test for BigTIFF and ClassicTIFF.
 *  - Tags for TIFF_LONG8 and TIFF_IFD8 can have exchanged field_type
 *    for reading. E.g: TIFF_LONG, TIFF_LONG8 in the file can be read into a tag
 *    with field_type TIFF_IFD8. Test for BigTIFF and ClassicTIFF.
 *
 */

#include <memory.h> /* necessary for linux compiler (memset) */
#include <stdio.h>
#include <stdlib.h> /* necessary for linux compiler */

#include "tif_config.h" /* necessary for linux compiler to get HAVE_UNISTD_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for unlink() on linux */
#endif

#include <tiffio.h>

#ifdef _MSC_VER
#pragma warning(disable : 4127) /* conditional expression is constant */
#endif

#define FAULT_RETURN 1
#define OK_RETURN 0

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

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define N(a) (sizeof(a) / sizeof(a[0]))

#define FIELD_IGNORE 0 /* same as FIELD_PSEUDO */

enum
{
    SINT8 = 65100,
    SINT16,
    SINT32,
    SINT64,
    C0_SINT8,
    C0_SINT16,
    C0_SINT32,
    C0_SINT64,
    C16_SINT8,
    C16_SINT16,
    C16_SINT32,
    C16_SINT64,
    C32_SINT8,
    C32_SINT16,
    C32_SINT32,
    C32_SINT64,
    C32_SINT64NULL,
    IFD8_Max32,
    IFD8_Max32_u32,
    IFD8_Max64,
    C0_IFD8_Max32,
    C0_IFD8_Max64,
    C16_IFD8_Max32,
    C16_IFD8_Max64,
    C32_IFD8_Max32,
    C32_IFD8_Max64,
    UINT64_Max32,
    UINT64_Max32_u32,
    UINT64_Max64,
    C0_UINT64_Max32,
    C0_UINT64_Max64,
    C16_UINT64_Max32,
    C16_UINT64_Max64,
    C32_UINT64_Max32,
    C32_UINT64_Max64,
};

static const TIFFFieldInfo tiff_field_info[] = {
    {SINT8, 1, 1, TIFF_SBYTE, FIELD_CUSTOM, 0, 0, "SINT8"},
    {SINT16, 1, 1, TIFF_SSHORT, FIELD_CUSTOM, 0, 0, "SINT16"},
    {SINT32, 1, 1, TIFF_SLONG, FIELD_CUSTOM, 0, 0, "SINT32"},
    {SINT64, 1, 1, TIFF_SLONG8, FIELD_CUSTOM, 0, 0, "SINT64"},
    {C0_SINT8, 6, 6, TIFF_SBYTE, FIELD_CUSTOM, 0, 0, "C0_SINT8"},
    {C0_SINT16, 6, 6, TIFF_SSHORT, FIELD_CUSTOM, 0, 0, "C0_SINT16"},
    {C0_SINT32, 6, 6, TIFF_SLONG, FIELD_CUSTOM, 0, 0, "C0_SINT32"},
    {C0_SINT64, 6, 6, TIFF_SLONG8, FIELD_CUSTOM, 0, 0, "C0_SINT64"},
    {C16_SINT8, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SBYTE, FIELD_CUSTOM, 0, 1,
     "C16_SINT8"},
    {C16_SINT16, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SSHORT, FIELD_CUSTOM, 0, 1,
     "C16_SINT16"},
    {C16_SINT32, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SLONG, FIELD_CUSTOM, 0, 1,
     "C16_SINT32"},
    {C16_SINT64, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SLONG8, FIELD_CUSTOM, 0, 1,
     "C16_SINT64"},
    {C32_SINT8, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SBYTE, FIELD_CUSTOM, 0, 1,
     "C32_SINT8"},
    {C32_SINT16, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SSHORT, FIELD_CUSTOM, 0,
     1, "C32_SINT16"},
    {C32_SINT32, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SLONG, FIELD_CUSTOM, 0, 1,
     "C32_SINT32"},
    {C32_SINT64, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SLONG8, FIELD_CUSTOM, 0,
     1, "C32_SINT64"},
    /* Test field_name=NULL in static const array, which is now possible because
     * handled within TIFFMergeFieldInfo(). */
    {C32_SINT64NULL, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_SLONG8, FIELD_CUSTOM,
     0, 1, NULL},
    /* Test for TIFF_IFD8 to be written as TIFF_IFD for ClassicTIFF,
     * as comparison to TIFF_LONG8 */
    {IFD8_Max32, 1, 1, TIFF_IFD8, FIELD_CUSTOM, 0, 0, "IFD8_Max32"},
    {IFD8_Max32_u32, 1, 1, TIFF_IFD8, FIELD_CUSTOM, 0, 0, "IFD8_Max32_u32"},
    {IFD8_Max64, 1, 1, TIFF_IFD8, FIELD_CUSTOM, 0, 0, "IFD8_Max64"},
    {C0_IFD8_Max32, 4, 4, TIFF_IFD8, FIELD_CUSTOM, 0, 0, "C0_IFD8_Max32"},
    {C0_IFD8_Max64, 4, 4, TIFF_IFD8, FIELD_CUSTOM, 0, 0, "C0_IFD8_Max64"},
    {C16_IFD8_Max32, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_IFD8, FIELD_CUSTOM, 0,
     1, "C16_IFD8_Max32"},
    {C16_IFD8_Max64, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_IFD8, FIELD_CUSTOM, 0,
     1, "C16_IFD8_Max64"},
    {C32_IFD8_Max32, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_IFD8, FIELD_CUSTOM, 0,
     1, "C32_IFD8_Max32"},
    {C32_IFD8_Max64, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_IFD8, FIELD_CUSTOM, 0,
     1, "C32_IFD8_Max64"},
    /* Test for TIFF_LONG8 to be written as TIFF_LONG for ClassicTIFF. */
    {UINT64_Max32, 1, 1, TIFF_LONG8, FIELD_CUSTOM, 0, 0, "UINT64_Max32"},
    {UINT64_Max32_u32, 1, 1, TIFF_LONG8, FIELD_CUSTOM, 0, 0,
     "UINT64_Max32_u32"},
    {UINT64_Max64, 1, 1, TIFF_LONG8, FIELD_CUSTOM, 0, 0, "UINT64_Max64"},
    {C0_UINT64_Max32, 4, 4, TIFF_LONG8, FIELD_CUSTOM, 0, 0, "C0_UINT64_Max32"},
    {C0_UINT64_Max64, 4, 4, TIFF_LONG8, FIELD_CUSTOM, 0, 0, "C0_UINT64_Max64"},
    {C16_UINT64_Max32, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_LONG8, FIELD_CUSTOM,
     0, 1, "C16_UINT64_Max32"},
    {C16_UINT64_Max64, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_LONG8, FIELD_CUSTOM,
     0, 1, "C16_UINT64_Max64"},
    {C32_UINT64_Max32, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_LONG8, FIELD_CUSTOM,
     0, 1, "C32_UINT64_Max32"},
    {C32_UINT64_Max64, TIFF_VARIABLE2, TIFF_VARIABLE2, TIFF_LONG8, FIELD_CUSTOM,
     0, 1, "C32_UINT64_Max64"},
};

/* Global parameter for the field array to be passed to extender, which can be
 * changed during runtime. */
static TIFFFieldInfo *p_tiff_field_info = (TIFFFieldInfo *)&tiff_field_info[0];
static uint32_t N_tiff_field_info =
    sizeof(tiff_field_info) / sizeof(tiff_field_info[0]);

static TIFFExtendProc parent = NULL;

static void extender(TIFF *tif)
{
    if (p_tiff_field_info != NULL)
    {
        TIFFMergeFieldInfo(tif, p_tiff_field_info, N_tiff_field_info);
        if (parent)
        {
            (*parent)(tif);
        }
    }
    else
    {
        TIFFErrorExtR(tif, "field_info_extender",
                      "Pointer to tiff_field_info array is NULL.");
    }
}

/*-- Global test arrays for writing and reading of IFD8 and LONG8 arrays. --*/
#define UINT64MAX_IFDTEST (UINT64_MAX - 2)
int8_t s8[] = {-8, -9, -10, -11, INT8_MAX, INT8_MIN};
int16_t s16[] = {-16, -17, -18, -19, INT16_MAX, INT16_MIN};
int32_t s32[] = {-32, -33, -34, -35, INT32_MAX, INT32_MIN};
int64_t s64[] = {-64, -65, -66, -67, INT64_MAX, INT64_MIN};
uint32_t u32[] = {0, 32, INT32_MAX, UINT32_MAX};
uint64_t u64_32[] = {0, 34, INT32_MAX, UINT32_MAX};
uint64_t u64[] = {0, 64, INT64_MAX, UINT64MAX_IFDTEST};

const uint32_t idxSingle = 0;

/*-- Macros to check TIFFSetField() return values. */
#define CHECK_RET_VALUE_SINGLE(ret, strTag, value, fmt)                        \
    if (value <= UINT32_MAX)                                                   \
    {                                                                          \
        if (ret != 1)                                                          \
        {                                                                      \
            fprintf(stdout, "Error writing %s value %" fmt " : ret=%d\n",      \
                    strTag, value, ret);                                       \
            GOTOFAILURE;                                                       \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (ret == 1 && !isBigTiff)                                            \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error: Should not write %s value %" fmt                   \
                    " for ClassicTIFF: ret=%d\n",                              \
                    strTag, value, ret);                                       \
            GOTOFAILURE;                                                       \
        }                                                                      \
        else if (ret != 1 && isBigTiff)                                        \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error writing %s value %" fmt " for BigTIFF: ret=%d\n",   \
                    strTag, value, ret);                                       \
            GOTOFAILURE;                                                       \
        }                                                                      \
    }

#define CHECK_RET_VALUE_ARRAY(ret, strTag, strValues, blnValueIsBig)           \
    if (!blnValueIsBig)                                                        \
    {                                                                          \
        if (ret != 1)                                                          \
        {                                                                      \
            fprintf(stdout, "Error writing %s  %s for BigTIFF: ret=%d\n",      \
                    strTag, strValues, ret);                                   \
            GOTOFAILURE;                                                       \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (ret == 1 && !isBigTiff)                                            \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error: Should not write %s %s for ClassicTIFF: ret=%d\n", \
                    strTag, strValues, ret);                                   \
            GOTOFAILURE;                                                       \
        }                                                                      \
        else if (ret != 1 && isBigTiff)                                        \
        {                                                                      \
            fprintf(stdout, "Error writing %s  %s for BigTIFF: ret=%d\n",      \
                    strTag, strValues, ret);                                   \
            GOTOFAILURE;                                                       \
        }                                                                      \
    }

/* ==== writeTestTiff() =====================================
 * Writes all newly defined tags with predefined values into
 * a dummy file.
 */
static int writeTestTiff(const char *szFileName, int isBigTiff)
{
    int ret;
    TIFF *tif;
    int retcode = FAULT_RETURN;
    uint32_t u32_x;

    unlink(szFileName);
    if (isBigTiff)
    {
        fprintf(stdout,
                "\n-- Writing signed values and Long8, IFD8 to BigTIFF...\n");
        tif = TIFFOpen(szFileName, "w8");
    }
    else
    {
        fprintf(stdout, "\n-- Writing signed values and Long8, IFD8 to "
                        "ClassicTIFF...\n");
        tif = TIFFOpen(szFileName, "w");
    }
    if (!tif)
    {
        fprintf(stdout, "Can't create test TIFF file %s.\n", szFileName);
        return (FAULT_RETURN);
    }

    /*---- Writing signed  data type. ----*/
    ret = TIFFSetField(tif, SINT8, s8[idxSingle]);
    if (ret != 1)
    {
        fprintf(stdout, "Error writing SINT8: ret=%d\n", ret);
        GOTOFAILURE;
    }
    ret = TIFFSetField(tif, SINT16, s16[idxSingle]);
    if (ret != 1)
    {
        fprintf(stdout, "Error writing SINT16: ret=%d\n", ret);
        GOTOFAILURE;
    }
    ret = TIFFSetField(tif, SINT32, s32[idxSingle]);
    if (ret != 1)
    {
        fprintf(stdout, "Error writing SINT32: ret=%d\n", ret);
        GOTOFAILURE;
    }

    TIFFSetField(tif, C0_SINT8, &s8);
    TIFFSetField(tif, C0_SINT16, &s16);
    TIFFSetField(tif, C0_SINT32, &s32);

    TIFFSetField(tif, C16_SINT8, 6, &s8);
    TIFFSetField(tif, C16_SINT16, 6, &s16);
    TIFFSetField(tif, C16_SINT32, 6, &s32);

    TIFFSetField(tif, C16_SINT8, 6, &s8);
    TIFFSetField(tif, C16_SINT16, 6, &s16);
    TIFFSetField(tif, C16_SINT32, 6, &s32);

    TIFFSetField(tif, C32_SINT8, 6, &s8);
    TIFFSetField(tif, C32_SINT16, 6, &s16);
    TIFFSetField(tif, C32_SINT32, 6, &s32);

    if (isBigTiff)
    {
        ret = TIFFSetField(tif, SINT64, s64[0]);
        if (ret != 1)
        {
            fprintf(stdout, "Error writing SINT64: ret=%d\n", ret);
            GOTOFAILURE;
        }
        ret = TIFFSetField(tif, C0_SINT64, &s64);
        if (ret != 1)
        {
            fprintf(stdout, "Error writing C0_SINT64: ret=%d\n", ret);
            GOTOFAILURE;
        }
        ret = TIFFSetField(tif, C16_SINT64, N(s64), &s64);
        if (ret != 1)
        {
            fprintf(stdout, "Error writing C16_SINT64: ret=%d\n", ret);
            GOTOFAILURE;
        }
        ret = TIFFSetField(tif, C32_SINT64, N(s64), &s64);
        if (ret != 1)
        {
            fprintf(stdout, "Error writing C32_SINT64: ret=%d\n", ret);
            GOTOFAILURE;
        }
        ret = TIFFSetField(tif, C32_SINT64NULL, N(s64), &s64);
        if (ret != 1)
        {
            fprintf(stdout, "Error writing C32_SINT64NULL: ret=%d\n", ret);
            GOTOFAILURE;
        }
    }

    /*---- Writing TIFF_IFD8 data type. ----*/
    /* For x32 compilations cast to uint64_t due to different va_arg integer
     * promotion w.r.t. x64 compilation. */
    u32_x = UINT32_MAX;
    ret = TIFFSetField(tif, IFD8_Max32, (uint64_t)UINT32_MAX);
    CHECK_RET_VALUE_SINGLE(ret, "IFD8_Max32", UINT32_MAX, PRIu32);

    ret = TIFFSetField(tif, IFD8_Max32_u32, (uint64_t)u32_x);
    CHECK_RET_VALUE_SINGLE(ret, "IFD8_Max32_u32", u32_x, PRIu32);

    ret = TIFFSetField(tif, IFD8_Max64, UINT64MAX_IFDTEST);
    CHECK_RET_VALUE_SINGLE(ret, "IFD8_Max64", UINT64MAX_IFDTEST, PRIu64);

    ret = TIFFSetField(tif, C0_IFD8_Max32, &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C0_IFD8_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C0_IFD8_Max64, &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C0_IFD8_Max64 array", "with uint64_t values",
                          TRUE);

    ret = TIFFSetField(tif, C16_IFD8_Max32, N(u64_32), &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C16_IFD8_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C16_IFD8_Max64, N(u64), &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C16_IFD8_Max64 array", "with uint64_t values",
                          TRUE);

    ret = TIFFSetField(tif, C32_IFD8_Max32, N(u64_32), &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C32_IFD8_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C32_IFD8_Max64, N(u64), &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C32_IFD8_Max64 array", "with uint64_t values",
                          TRUE);

    /*---- Writing TIFF_LONG8 data type. ----*/
    ret = TIFFSetField(tif, UINT64_Max32, (uint64_t)UINT32_MAX);
    CHECK_RET_VALUE_SINGLE(ret, "UINT64_Max32", UINT32_MAX, PRIu32);

    ret = TIFFSetField(tif, UINT64_Max32_u32, (uint64_t)u32_x);
    CHECK_RET_VALUE_SINGLE(ret, "UINT64_Max32_u32", u32_x, PRIu32);

    ret = TIFFSetField(tif, UINT64_Max64, UINT64MAX_IFDTEST);
    CHECK_RET_VALUE_SINGLE(ret, "UINT64_Max64", UINT64MAX_IFDTEST, PRIu64);

    ret = TIFFSetField(tif, C0_UINT64_Max32, &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C0_UINT64_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C0_UINT64_Max64, &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C0_UINT64_Max64 array", "with uint64_t values",
                          TRUE);

    ret = TIFFSetField(tif, C16_UINT64_Max32, N(u64_32), &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C16_UINT64_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C16_UINT64_Max64, N(u64), &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C16_UINT64_Max64 array", "with uint64_t values",
                          TRUE);

    ret = TIFFSetField(tif, C32_UINT64_Max32, N(u64_32), &u64_32);
    CHECK_RET_VALUE_ARRAY(ret, "C32_UINT64_Max32 array", "with uint32_t values",
                          FALSE);

    ret = TIFFSetField(tif, C32_UINT64_Max64, N(u64), &u64);
    CHECK_RET_VALUE_ARRAY(ret, "C32_UINT64_Max64 array", "with uint64_t values",
                          TRUE);

    /*---- Writing dummy image data. ----*/
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, 1);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, 1);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    ret = (int)TIFFWriteEncodedStrip(tif, 0, (void *)"\0", 1);
    if (ret != 1)
    {
        fprintf(stdout, "Error TIFFWriteEncodedStrip: ret=%d\n", ret);
        GOTOFAILURE;
    }

    retcode = OK_RETURN;
failure:
    fprintf(stdout, "------- Closing TIFF file with retcode=%d --------\n\n",
            ret);
    TIFFClose(tif);
    return (retcode);
} /*-- writeTestTiff() --*/

/*-- Macros for reading and comparing tags --*/
#define READ_CHECK_SINGLE_VALUE(tif, tag, strTag, var, value, strValueFmt1,    \
                                strValueFmt2, blnValueIsBig)                   \
    ret = TIFFGetField(tif, tag, &var);                                        \
    if (ret != 1)                                                              \
    {                                                                          \
        if ((!blnValueIsBig || (blnValueIsBig && isBigTiff)))                  \
        {                                                                      \
            fprintf(stdout, "Error reading %s: ret=%d\n", strTag, ret);        \
            GOTOFAILURE                                                        \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (blnValueIsBig && !isBigTiff)                                       \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error: Should not read %s value %" strValueFmt1           \
                    " for ClassicTIFF. (set value was %" strValueFmt2          \
                    "): ret=%d\n",                                             \
                    strTag, var, value, ret);                                  \
            GOTOFAILURE;                                                       \
        }                                                                      \
        if (var != value)                                                      \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Read value of %s  %" strValueFmt1                         \
                    " differs from set value %" strValueFmt2 " \n",            \
                    strTag, var, value);                                       \
            GOTOFAILURE                                                        \
        }                                                                      \
    }

#define CHECK_ARRAY(tif, tag, strTag, varArrPtr, count, valueArr,              \
                    strValueFmt1, strValueFmt2, blnValueIsBig)                 \
    if (ret != 1 || varArrPtr == NULL)                                         \
    {                                                                          \
        if ((!blnValueIsBig || (blnValueIsBig && isBigTiff)))                  \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error reading %s: ret=%d; count=%" PRIu64                 \
                    "; pointer=%p\n",                                          \
                    strTag, ret, (uint64_t)count, (void *)varArrPtr);          \
            GOTOFAILURE                                                        \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        if (blnValueIsBig && !isBigTiff)                                       \
        {                                                                      \
            fprintf(stdout,                                                    \
                    "Error: Should not read %s for ClassicTIFF array with "    \
                    "big (uint64_t) values: ret=%d\n",                         \
                    strTag, ret);                                              \
            GOTOFAILURE;                                                       \
        }                                                                      \
        uint64_t k;                                                            \
        for (k = 0; k < count; k++)                                            \
        {                                                                      \
            if (varArrPtr[k] != valueArr[k])                                   \
            {                                                                  \
                fprintf(stdout,                                                \
                        "Read value %d of %s-Array %" strValueFmt1             \
                        " differs from set value %" strValueFmt2 "\n",         \
                        (int)k, strTag, varArrPtr[k], valueArr[k]);            \
                GOTOFAILURE                                                    \
            }                                                                  \
        }                                                                      \
    }

#define READ_CHECK_C0_ARRAY(tif, tag, strTag, varArrPtr, count, valueArr,      \
                            strValueFmt1, strValueFmt2, blnValueIsBig)         \
    varArrPtr = NULL;                                                          \
    ret = TIFFGetField(tif, tag, &varArrPtr);                                  \
    CHECK_ARRAY(tif, tag, strTag, varArrPtr, count, valueArr, strValueFmt1,    \
                strValueFmt2, blnValueIsBig)

#define READ_CHECK_Cxx_ARRAY(tif, tag, strTag, varArrPtr, count, valueArr,     \
                             strValueFmt1, strValueFmt2, blnValueIsBig)        \
    varArrPtr = NULL;                                                          \
    ret = TIFFGetField(tif, tag, &count, &varArrPtr);                          \
    CHECK_ARRAY(tif, tag, strTag, varArrPtr, count, valueArr, strValueFmt1,    \
                strValueFmt2, blnValueIsBig)

/* ==== readTestTiff() =====================================
 * Open file with all written, newly defined tags and read
 * and compare the value of the tags with the written value.
 */
static int readTestTiff(const char *szFileName, int isBigTiff,
                        int isIFD8LONG8Exchange)
{
    int ret;
    int i;
    int8_t s8l, *s8p;
    int16_t s16l, *s16p;
    int32_t s32l, *s32p;
    int64_t s64l, *s64p;
    uint64_t u64l, *u64p;
    uint16_t count;
    uint32_t count32;
    int retcode = FAULT_RETURN;

    /* Copy const array to be manipulated and freed just after TIFFMergeFields()
     * within the "extender()" called by TIFFOpen(). */
    TIFFFieldInfo *tiff_field_info2 = NULL;
    TIFFFieldInfo *tiff_field_info_sav = NULL;
    const char *strAux = "";
    if (isIFD8LONG8Exchange)
    {
        tiff_field_info2 = (TIFFFieldInfo *)malloc(sizeof(tiff_field_info));
        if (tiff_field_info2 == (TIFFFieldInfo *)NULL)
        {
            fprintf(stdout,
                    "Can't allocate memoy for tiff_field_info2 structure.\n");
            return (FAULT_RETURN);
        }
        memcpy(tiff_field_info2, tiff_field_info, sizeof(tiff_field_info));
        /* Switch field array for extender callback. */
        tiff_field_info_sav = p_tiff_field_info;
        p_tiff_field_info = tiff_field_info2;

        /*-- Adapt tiff_field_info array for TIFF_IFD8 with TIFF_LONG8
         *   (exchange them) to test reading of other types. --*/
        for (i = 17; i < 17 + 9; i++)
        {
            tiff_field_info2[i].field_type = TIFF_LONG8;
            tiff_field_info2[i + 9].field_type = TIFF_IFD8;
        }
        strAux = "with exchanged IFD8, LONG8 field_types";
    }

    fprintf(stdout, "-- Reading signed values ...\n");
    TIFF *tif = TIFFOpen(szFileName, "r");
    if (!tif)
    {
        fprintf(stdout, "Can't open test TIFF file %s.\n", szFileName);
        return (FAULT_RETURN);
    }
    if (isIFD8LONG8Exchange)
    {
        /* tiff_field_info2 should not be needed anymore, as long as the still
         * active extender() is not called again. Therefore, the extender
         * callback should be disabled by resetting it to the saved one. */
        free(tiff_field_info2);
        tiff_field_info2 = NULL;
        p_tiff_field_info = tiff_field_info_sav;
    }

    ret = TIFFGetField(tif, SINT8, &s8l);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading SINT8: ret=%d\n", ret);
        GOTOFAILURE
    }
    else
    {
        if (s8l != s8[idxSingle])
        {
            fprintf(stdout,
                    "Read value of SINT8  %d differs from set value %d\n", s8l,
                    s8[idxSingle]);
            GOTOFAILURE
        }
    }
    ret = TIFFGetField(tif, SINT16, &s16l);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading SINT16: ret=%d\n", ret);
        GOTOFAILURE
    }
    else
    {
        if (s16l != s16[idxSingle])
        {
            fprintf(stdout,
                    "Read value of SINT16  %d differs from set value %d\n",
                    s16l, s16[idxSingle]);
            GOTOFAILURE
        }
    }
    ret = TIFFGetField(tif, SINT32, &s32l);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading SINT32: ret=%d\n", ret);
        GOTOFAILURE
    }
    else
    {
        if (s32l != s32[idxSingle])
        {
            fprintf(stdout,
                    "Read value of SINT32  %d differs from set value %d\n",
                    s32l, s32[idxSingle]);
            GOTOFAILURE
        }
    }

    ret = TIFFGetField(tif, C0_SINT8, &s8p);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading C0_SINT8: ret=%d\n", ret);
        GOTOFAILURE
    }
    count = N(s8);
    for (i = 0; i < count; i++)
    {
        if (s8p[i] != s8[i])
        {
            fprintf(stdout,
                    "Read value %d of C0_SINT8-Array %d differs from set value "
                    "%d\n",
                    i, s8p[i], s8[i]);
            GOTOFAILURE
        }
    }

    ret = TIFFGetField(tif, C0_SINT16, &s16p);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading C0_SINT16: ret=%d\n", ret);
        GOTOFAILURE
    }
    count = N(s16);
    for (i = 0; i < count; i++)
    {
        if (s16p[i] != s16[i])
        {
            fprintf(stdout,
                    "Read value %d of C0_SINT16-Array %d differs from set "
                    "value %d\n",
                    i, s16p[i], s16[i]);
            GOTOFAILURE
        }
    }

    ret = TIFFGetField(tif, C0_SINT32, &s32p);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading C0_SINT32: ret=%d\n", ret);
        GOTOFAILURE
    }
    count = N(s32);
    for (i = 0; i < count; i++)
    {
        if (s32p[i] != s32[i])
        {
            fprintf(stdout,
                    "Read value %d of C0_SINT32-Array %d differs from set "
                    "value %d\n",
                    i, s32p[i], s32[i]);
            GOTOFAILURE
        }
    }

    s8p = NULL;
    ret = TIFFGetField(tif, C16_SINT8, &count, &s8p);
    if (ret != 1 || s8p == NULL)
    {
        fprintf(stdout,
                "Error reading C16_SINT8: ret=%d; count=%d; pointer=%p\n", ret,
                count, (void *)s8p);
        GOTOFAILURE
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            if (s8p[i] != s8[i])
            {
                fprintf(
                    stdout,
                    "Read value %d of s8-Array %d differs from set value %d\n",
                    i, s8p[i], s8[i]);
                GOTOFAILURE
            }
        }
    }

    s16p = NULL;
    ret = TIFFGetField(tif, C16_SINT16, &count, &s16p);
    if (ret != 1 || s16p == NULL)
    {
        fprintf(stdout,
                "Error reading C16_SINT16: ret=%d; count=%d; pointer=%p\n", ret,
                count, (void *)s16p);
        GOTOFAILURE
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            if (s16p[i] != s16[i])
            {
                fprintf(stdout,
                        "Read value %d of C16_SINT16-Array %d differs from set "
                        "value %d\n",
                        i, s16p[i], s16[i]);
                GOTOFAILURE
            }
        }
    }

    s32p = NULL;
    ret = TIFFGetField(tif, C16_SINT32, &count, &s32p);
    if (ret != 1 || s32p == NULL)
    {
        fprintf(stdout,
                "Error reading C16_SINT32: ret=%d; count=%d; pointer=%p\n", ret,
                count, (void *)s32p);
        GOTOFAILURE
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            if (s32p[i] != s32[i])
            {
                fprintf(stdout,
                        "Read value %d of C16_SINT32-Array %d differs from set "
                        "value %d\n",
                        i, s32p[i], s32[i]);
                GOTOFAILURE
            }
        }
    }

    if (isBigTiff)
    {
        ret = TIFFGetField(tif, SINT64, &s64l);
        if (ret != 1)
        {
            fprintf(stdout, "Error reading SINT64: ret=%d\n", ret);
            GOTOFAILURE
        }
        else
        {
            if (s64l != s64[idxSingle])
            {
                fprintf(stdout,
                        "Read value of SINT64  %" PRIi64
                        " differs from set value %" PRIi64 "\n",
                        s64l, s64[idxSingle]);
                GOTOFAILURE
            }
        }

        s64p = NULL;
        ret = TIFFGetField(tif, C0_SINT64, &s64p);
        count = N(s64);
        if (ret != 1)
        {
            fprintf(stdout, "Error reading C0_SINT64: ret=%d\n", ret);
            GOTOFAILURE
        }
        else
        {
            for (i = 0; i < count; i++)
            {
                if (s64p[i] != s64[i])
                {
                    fprintf(stdout,
                            "Read value %d of C0_SINT64-Array %" PRIi64
                            " differs from set value %" PRIi64 "\n",
                            i, s64p[i], s64[i]);
                    GOTOFAILURE
                }
            }
        }

        s64p = NULL;
        ret = TIFFGetField(tif, C16_SINT64, &count, &s64p);
        if (ret != 1 || s64p == NULL)
        {
            fprintf(stdout,
                    "Error reading C16_SINT64: ret=%d; count=%d; pointer=%p\n",
                    ret, count, (void *)s64p);
            GOTOFAILURE
        }
        else
        {
            for (i = 0; i < count; i++)
            {
                if (s64p[i] != s64[i])
                {
                    fprintf(stdout,
                            "Read value %d of C16_SINT64-Array %" PRIi64
                            " differs from set value %" PRIi64 "\n",
                            i, s64p[i], s64[i]);
                    GOTOFAILURE
                }
            }
        }

        s64p = NULL;
        ret = TIFFGetField(tif, C32_SINT64, &count32, &s64p);
        if (ret != 1 || s64p == NULL)
        {
            fprintf(stdout,
                    "Error reading C32_SINT64: ret=%d; count=%d; pointer=%p\n",
                    ret, count, (void *)s64p);
            GOTOFAILURE
        }
        else
        {
            for (i = 0; i < (int)count32; i++)
            {
                if (s64p[i] != s64[i])
                {
                    fprintf(stdout,
                            "Read value %d of C32_SINT64-Array %" PRIi64
                            " differs from set value %" PRIi64 "\n",
                            i, s64p[i], s64[i]);
                    GOTOFAILURE
                }
            }
        }
    } /*-- if(isBigTiff) --*/

    /*---- Reading IFD8 and LONG8 values ----*/
    fprintf(stdout, "-- Reading IFD8 and Long8 values %s from %s ...\n", strAux,
            isBigTiff ? "BigTIFF" : "ClassicTIFF");

    /* Macros do not attempt to read tags only possible for BigTIFF
     * if it is ClassicTIFF (controlled by last bool parameter) */

    /* Single values IFD8 and LONG8 (UINT64) */
    READ_CHECK_SINGLE_VALUE(tif, IFD8_Max32, "IFD8_Max32", u64l, UINT32_MAX,
                            PRIu64, PRIu32, FALSE);
    READ_CHECK_SINGLE_VALUE(tif, IFD8_Max32_u32, "IFD8_Max32_u32", u64l,
                            UINT32_MAX, PRIu64, PRIu32, FALSE);
    READ_CHECK_SINGLE_VALUE(tif, IFD8_Max64, "IFD8_Max64", u64l,
                            UINT64MAX_IFDTEST, PRIu64, PRIu64, TRUE);

    READ_CHECK_SINGLE_VALUE(tif, UINT64_Max32, "UINT64_Max32", u64l, UINT32_MAX,
                            PRIu64, PRIu32, FALSE);
    READ_CHECK_SINGLE_VALUE(tif, UINT64_Max32_u32, "UINT64_Max32_u32", u64l,
                            UINT32_MAX, PRIu64, PRIu32, FALSE);
    READ_CHECK_SINGLE_VALUE(tif, UINT64_Max64, "UINT64_Max64", u64l,
                            UINT64MAX_IFDTEST, PRIu64, PRIu64, TRUE);

    u64p = NULL;
    count = N(u32);

    /* Arrays IFD8 */
    READ_CHECK_C0_ARRAY(tif, C0_IFD8_Max32, "C0_IFD8_Max32", u64p, N(u64_32),
                        u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_C0_ARRAY(tif, C0_IFD8_Max64, "C0_IFD8_Max64", u64p, N(u64), u64,
                        PRIu64, PRIu64, TRUE);

    READ_CHECK_Cxx_ARRAY(tif, C16_IFD8_Max32, "C16_IFD8_Max32", u64p, count,
                         u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_Cxx_ARRAY(tif, C16_IFD8_Max64, "C16_IFD8_Max64", u64p, count,
                         u64, PRIu64, PRIu64, TRUE);

    READ_CHECK_Cxx_ARRAY(tif, C32_IFD8_Max32, "C32_IFD8_Max32", u64p, count32,
                         u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_Cxx_ARRAY(tif, C32_IFD8_Max64, "C32_IFD8_Max64", u64p, count32,
                         u64, PRIu64, PRIu64, TRUE);

    /* Arrays LONG8 (UINT64) */
    READ_CHECK_C0_ARRAY(tif, C0_UINT64_Max32, "C0_UINT64_Max32", u64p,
                        N(u64_32), u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_C0_ARRAY(tif, C0_UINT64_Max64, "C0_UINT64_Max64", u64p, N(u64),
                        u64, PRIu64, PRIu64, TRUE);

    READ_CHECK_Cxx_ARRAY(tif, C16_UINT64_Max32, "C16_UINT64_Max32", u64p, count,
                         u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_Cxx_ARRAY(tif, C16_UINT64_Max64, "C16_UINT64_Max64", u64p, count,
                         u64, PRIu64, PRIu64, TRUE);

    READ_CHECK_Cxx_ARRAY(tif, C32_UINT64_Max32, "C32_UINT64_Max32", u64p,
                         count32, u64_32, PRIu64, PRIu64, FALSE);
    READ_CHECK_Cxx_ARRAY(tif, C32_UINT64_Max64, "C32_UINT64_Max64", u64p,
                         count32, u64, PRIu64, PRIu64, TRUE);

    retcode = OK_RETURN;
failure:
    fprintf(stdout, "-- End of test. Closing TIFF file. --\n");
    TIFFClose(tif);
    return (retcode);
}
/*-- readTestTiff() --*/

static int readTestTiff_ignore_some_tags(const char *szFileName)
{
    int ret;
    int8_t s8l;
    int16_t s16l;
    int32_t s32l;
    int retcode = FAULT_RETURN;

    /* There is a use case, where LibTIFF shall be prevented from reading
     * unknown tags that are present in the file as anonymous tags. This can be
     * achieved by defining these tags with ".field_bit = FIELD_IGNORE". */

    /* Copy const array to be manipulated and freed just after TIFFMergeFields()
     * within the "extender()" called by TIFFOpen(). */
    TIFFFieldInfo *tiff_field_info2;
    tiff_field_info2 = (TIFFFieldInfo *)malloc(sizeof(tiff_field_info));
    if (tiff_field_info2 == (TIFFFieldInfo *)NULL)
    {
        fprintf(stdout,
                "Can't allocate memoy for tiff_field_info2 structure.\n");
        return (FAULT_RETURN);
    }
    memcpy(tiff_field_info2, tiff_field_info, sizeof(tiff_field_info));
    /* Switch field array for extender callback. */
    p_tiff_field_info = tiff_field_info2;

    /*-- Adapt tiff_field_info array for ignoring unknown tags to LibTIFF, which
     * have been written to file before. --*/
    /* a.) Just set field_bit to FIELD_IGNORE = 0 */
    tiff_field_info2[2].field_bit = FIELD_IGNORE;
    /* b.) Usecase with all field array infos zero but the tag value. */
    ttag_t tag = tiff_field_info2[4].field_tag;
    memset(&tiff_field_info2[4], 0, sizeof(tiff_field_info2[4]));
    tiff_field_info2[4].field_tag = tag;

    fprintf(stdout, "\n-- Reading file with unknown tags to be ignored ...\n");
    TIFF *tif = TIFFOpen(szFileName, "r");

    /* tiff_field_info2 should not be needed anymore, as long as the still
     * active extender() is not called again. Therefore, the extender callback
     * should be disabled by resetting it to the saved one. */
    free(tiff_field_info2);
    tiff_field_info2 = NULL;
    TIFFSetTagExtender(parent);

    if (!tif)
    {
        fprintf(stdout, "Can't open test TIFF file %s.\n", szFileName);
        return (FAULT_RETURN);
    }

    /* Read the first two known tags for testing */
    ret = TIFFGetField(tif, SINT8, &s8l);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading SINT8: ret=%d\n", ret);
        GOTOFAILURE
    }
    else
    {
        if (s8l != s8[idxSingle])
        {
            fprintf(stdout,
                    "Read value of SINT8  %d differs from set value %d\n", s8l,
                    s8[idxSingle]);
            GOTOFAILURE
        }
    }
    ret = TIFFGetField(tif, SINT16, &s16l);
    if (ret != 1)
    {
        fprintf(stdout, "Error reading SINT16: ret=%d\n", ret);
        GOTOFAILURE
    }
    else
    {
        if (s16l != s16[idxSingle])
        {
            fprintf(stdout,
                    "Read value of SINT16  %d differs from set value %d\n",
                    s16l, s16[idxSingle]);
            GOTOFAILURE
        }
    }

    /* The two ignored tags shall not be present. */
    ret = TIFFGetField(tif, tiff_field_info[2].field_tag, &s32l);
    if (ret != 0)
    {
        fprintf(stdout,
                "Error: Tag %u, set to be ignored, has been read from file.\n",
                tiff_field_info[2].field_tag);
        GOTOFAILURE
    }

    ret = TIFFGetField(tif, tiff_field_info[4].field_tag, &s32l);
    if (ret != 0)
    {
        fprintf(stdout,
                "Error: Tag %u, set to be ignored, has been read from file.\n",
                tiff_field_info[4].field_tag);
        GOTOFAILURE
    }

    retcode = OK_RETURN;
failure:

    fprintf(stdout,
            "-- End of test for ignored unknown tags. Closing TIFF file. --\n");
    TIFFClose(tif);
    return (retcode);
}
/*-- readTestTiff_ignore_some_tags() --*/

/* ==== main() =============================== */
int main(void)
{
    /*-- Signed tags and LONG8, IFD8 tags test --*/
    parent = TIFFSetTagExtender(&extender);
    if (writeTestTiff("temp.tif", 0) != OK_RETURN)
        return (-1);
    if (readTestTiff("temp.tif", 0, 0) != OK_RETURN)
        return (-1);
    if (readTestTiff("temp.tif", 0, 1) != OK_RETURN)
        return (-1);

    if (writeTestTiff("tempBig.tif", 1) != OK_RETURN)
        return (-1);
    if (readTestTiff("tempBig.tif", 1, 0) != OK_RETURN)
        return (-1);
    if (readTestTiff("tempBig.tif", 1, 1) != OK_RETURN)
        return (-1);
#ifndef DEBUG_TESTING
    unlink("tempBig.tif");
#endif
    fprintf(stdout, "---------- Signed tag and Long8, IFD8 tag test "
                    "finished OK -----------\n");

    /*-- Adapt tiff_field_info array for ignoring unknown tags to LibTIFF, which
     * have been written to file. --*/
    if (readTestTiff_ignore_some_tags("temp.tif") != OK_RETURN)
        return (-1);
#ifndef DEBUG_TESTING
    unlink("temp.tif");
#endif
    fprintf(stdout,
            "---------- Ignoring unknown tag test finished OK -----------\n");

    return 0;
}
