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
  (JNIEnv *env, jclass clazz, jstring filePath, jintArray img, jobject options, jint img_width, jint img_height) {

    const char *strPath = NULL;
    strPath = env->GetStringUTFChars(filePath, 0);
    LOGIS("nativeTiffOpenForSave", strPath);

    //Get array of jint from jintArray
    jint *c_array;
    c_array = env->GetIntArrayElements(img, NULL);
    if (c_array == NULL) {
        //if array is null - nothing to save
        LOGE("array is null");
        return;
    }

    //Get options
    jclass jSaveOptionsClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/TiffSaver$SaveOptions");
    jfieldID gOptions_CompressionModeFieldID = env->GetFieldID(jSaveOptionsClass,
                "compressionMode",
                "Lorg/beyka/tiffbitmapfactory/TiffSaver$CompressionMode;");
    jobject compressionMode = env->GetObjectField(options, gOptions_CompressionModeFieldID);

    jclass compressionModeClass = env->FindClass(
                "org/beyka/tiffbitmapfactory/TiffSaver$CompressionMode");
    jfieldID ordinalFieldID = env->GetFieldID(compressionModeClass, "ordinal", "I");
    jint compressionInt = env->GetIntField(compressionMode, ordinalFieldID);


        /*if (compressionMode == NULL) {
            LOGI("config is NULL, creating default options");
            jclass bitmapConfig = env->FindClass(
                    "org/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig");
            jfieldID argb8888FieldID = env->GetStaticFieldID(bitmapConfig, "ARGB_8888",
                                                             "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig;");
            config = env->GetStaticObjectField(bitmapConfig, argb8888FieldID);
            env->DeleteLocalRef(bitmapConfig);
        }*/

    int pixelsBufferSize = img_width * img_height;
    int* array = (int *) malloc(sizeof(int) * pixelsBufferSize);
    for (int i = 0; i < img_width; i++) {
        for (int j = 0; j < img_height; j++) {
            jint crPix = c_array[j * img_width + i];
            int red = colorMask & crPix >> 16;
            int green = colorMask & crPix >> 8;
            int blue = colorMask & crPix;

            crPix = (blue << 16) | (green << 8) | (red);
            array[j * img_width + i] = crPix;
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
        //TIFFSetField(output_image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);

        TIFFSetField(output_image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
        //TIFFSetField(output_image, TIFFTAG_COMPRESSION, compressionInt);
        TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

        // Write the information to the file
        TIFFWriteEncodedStrip(output_image, 0, &array[0], img_width*img_height * 4);

        // Close the file
        TIFFClose(output_image);

        free (array);

    env->ReleaseStringUTFChars(filePath, strPath);
    env->ReleaseIntArrayElements(img, c_array, 0);

  }








#ifdef __cplusplus
}
#endif
