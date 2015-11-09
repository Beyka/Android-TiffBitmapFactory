#ifdef __cplusplus
extern "C" {
#endif

#include "NativeExceptions.h"

/**
 * Read a Tiff file in an incremental fashion.
*/
int
readTiffIncremental(JNIEnv *env, TIFF* in, unsigned char** outBuf, uint32 inSampleSize, int32 availableMemoryBytes, jstring path);

#ifdef __cplusplus
}
#endif
