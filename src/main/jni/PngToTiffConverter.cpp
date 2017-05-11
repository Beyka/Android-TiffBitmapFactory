//
// Created by beyka on 5/12/17.
//

#include "PngToTiffConverter.h"

PngToTiffConverter::PngToTiffConverter(JNIEnv *e, jclass clazz, jstring in, jstring out, jobject opts)
    : BaseTiffConverter(e, clazz, in, out, opts)
{
    png_ptr_init = 0;
    png_info_init = 0;
}

PngToTiffConverter::~PngToTiffConverter() {
    if (tiffImage) {
        TIFFClose(tiffImage);
        tiffImage = NULL;
    }

    if (png_info_init) {
        png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
    }

    if (png_ptr_init) {
        png_destroy_write_struct(&png_ptr, NULL);
    }

    if (pngFile) {
        fclose(pngFile);
    }
}

jboolean PngToTiffConverter::convert()
{
    readOptions();

    //open tiff file for writing or appending
    int fileDescriptor = -1;
}