#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read a Tiff file in an incremental fashion.
*/
int
readTiffIncremental(TIFF* in, unsigned char** outBuf, uint32 inSampleSize, int32 availableMemoryBytes);

#ifdef __cplusplus
}
#endif
