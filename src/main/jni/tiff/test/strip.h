/*
 * Strip test routines - header file.
 */

#ifndef TEST_STRIP_H
#define TEST_STRIP_H

#include "tiffio.h"

int write_strips(TIFF *tif, const tdata_t array, const tsize_t size);
int read_strips(TIFF *tif, const tdata_t array, const tsize_t size);
int create_image_striped(const char *name, uint32_t width, uint32_t length,
                         uint32_t rowsperstrip, uint16_t compression,
                         uint16_t spp, uint16_t bps, uint16_t photometric,
                         uint16_t sampleformat, uint16_t planarconfig,
                         const tdata_t array, const tsize_t size);
int read_image_striped(const char *name, uint32_t width, uint32_t length,
                       uint32_t rowsperstrip, uint16_t compression,
                       uint16_t spp, uint16_t bps, uint16_t photometric,
                       uint16_t sampleformat, uint16_t planarconfig,
                       const tdata_t array, const tsize_t size);

#endif /* TEST_STRIP_H */
