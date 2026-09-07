/*
 * Helper testing routines - header file.
 */

#ifndef TEST_CHECK_TAG_H
#define TEST_CHECK_TAG_H

#include "tiffio.h"

int CheckShortField(TIFF *tif, ttag_t field, uint16_t value);
int CheckShortPairedField(TIFF *tif, ttag_t field, const uint16_t *values);
int CheckLongField(TIFF *tif, ttag_t field, uint32_t value);

#endif /* TEST_CHECK_TAG_H */
