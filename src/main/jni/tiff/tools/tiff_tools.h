/*
 * Shared helpers for libtiff command line tools.
 */

#ifndef TIFF_TOOLS_H
#define TIFF_TOOLS_H

#include "tiffio.h"

#ifdef __cplusplus
extern "C"
{
#endif

    int TIFFToolsParseMemoryLimitMiB(const char *arg, tmsize_t *limit);

#ifdef __cplusplus
}
#endif

#endif
