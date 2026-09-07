/*
 * Shared helpers for libtiff command line tools.
 */

#include "tiff_tools.h"
#include "tif_config.h"
#include "tiffiop.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

int TIFFToolsParseMemoryLimitMiB(const char *arg, tmsize_t *limit)
{
    char *end = NULL;
    const char *p;
    unsigned long long value;
    uint64_t bytes;
    tmsize_t limit_value;

    if (arg == NULL || limit == NULL)
        return 0;

    p = arg;
    while (*p != '\0' && isspace((unsigned char)*p))
        ++p;
    if (*p == '-')
        return 0;

    errno = 0;
    value = strtoull(p, &end, 0);

    while (*end != '\0' && isspace((unsigned char)*end))
        ++end;

    if (errno != 0 || end == p || *end != '\0')
        return 0;

    bytes =
        _TIFFMultiply64(NULL, (uint64_t)value, 1024U * 1024U, "memory limit");
    if (bytes == 0 && value != 0)
        return 0;

    limit_value = _TIFFCastUInt64ToSSize(NULL, bytes, "memory limit");
    if (limit_value == 0 && bytes != 0)
        return 0;

    *limit = limit_value;
    return 1;
}
