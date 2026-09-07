/* Android configuration for libtiff 4.7.2. */
#ifndef TIF_CONFIG_H
#define TIF_CONFIG_H

#include "tiffconf.h"

#define CCITT_SUPPORT 1
#define CHECK_JPEG_YCBCR_SUBSAMPLING 1
#define DEFER_STRILE_LOAD 1
#define HAVE_ASSERT_H 1
#define HAVE_DECL_OPTARG 1
#define HAVE_FCNTL_H 1
#define HAVE_FSEEKO 1
#define HAVE_GETOPT 1
#define HAVE_MMAP 1
#define HAVE_SNPRINTF 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define JPEG_SUPPORT 1
#define LOGLUV_SUPPORT 1
#define LZW_SUPPORT 1
#define MDI_SUPPORT 1
#define NEXT_SUPPORT 1
#define OJPEG_SUPPORT 1
#define PACKAGE "tiff"
#define PACKAGE_BUGREPORT "tiff@lists.osgeo.org"
#define PACKAGE_NAME "LibTIFF Software"
#define PACKAGE_TARNAME "tiff"
#define PACKAGE_URL "https://libtiff.gitlab.io/libtiff/"
#define PACKBITS_SUPPORT 1
#define PIXARLOG_SUPPORT 1
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#define STRIP_SIZE_DEFAULT 8192
#define TIFF_MAX_DIR_COUNT 1048576
#define THUNDER_SUPPORT 1
#define ZIP_SUPPORT 1
#define WORDS_BIGENDIAN 0
#define _FILE_OFFSET_BITS 64

#define TIFF_SIZE_FORMAT "zu"
#if __SIZEOF_SIZE_T__ == 8
#define TIFF_SSIZE_FORMAT PRId64
#elif __SIZEOF_SIZE_T__ == 4
#define TIFF_SSIZE_FORMAT PRId32
#else
#error "Unsupported size_t size"
#endif

#endif
