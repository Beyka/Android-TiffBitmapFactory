//
// Created by beyka on 18.2.16.
//
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#include "NativeTiffSaver.h"

int const colorMask = 0xFF;

JNIEXPORT void JNICALL Java_org_beyka_tiffbitmapfactory_TiffSaver_save
  (JNIEnv *env, jclass clazz, jstring filePath, jintArray img, jint img_width, jint img_height) {

    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(filePath, 0);
    LOGIS("nativeTiffOpenForSave", strPath);

    jint *c_array;
    c_array = env->GetIntArrayElements(img, NULL);
    if (c_array == NULL) {
        LOGE("array is null");
        return;
    } else {
        LOGE("array is not null");
    }
    //int pixelsBufferSize = img_width * img_height;
    //int *res_array = (int *) malloc(sizeof(int) * pixelsBufferSize);

    for (int i = 0; i < img_width; i++) {
        for (int j = 0; j < img_height; j++) {
            jint crPix = c_array[j * img_width + i];
            //int alpha = colorMask & crPix >> 24;
            int red = colorMask & crPix >> 16;
            int green = colorMask & crPix >> 8;
            int blue = colorMask & crPix;

            crPix = (blue << 16) | (green << 8) | (red);
            c_array[j * img_width + i] = crPix;
        }
    }

    TIFF *output_image;
    // Open the TIFF file
    if((output_image = TIFFOpen(strPath, "w")) == NULL){
        LOGE("Unable to write tif file");
        return;
    }

    // We need to set some values for basic tags before we can add any data
        TIFFSetField(output_image, TIFFTAG_IMAGEWIDTH, img_width);
        TIFFSetField(output_image, TIFFTAG_IMAGELENGTH, img_height);
        TIFFSetField(output_image, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(output_image, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(output_image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

        TIFFSetField(output_image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
        TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

        // Write the information to the file
        TIFFWriteEncodedStrip(output_image, 0, &c_array[0], img_width*img_height * 4);

        // Close the file
        TIFFClose(output_image);

        //free(res_array);

    env->ReleaseStringUTFChars(filePath, strPath);
    env->ReleaseIntArrayElements(img, c_array, 0);

  }








#ifdef __cplusplus
}
#endif
