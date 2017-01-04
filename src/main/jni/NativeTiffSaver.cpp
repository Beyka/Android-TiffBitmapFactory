//
// Created by beyka on 18.2.16.
//
using namespace std;

#ifdef __cplusplus
extern "C" {
    #endif

    #include "NativeTiffSaver.h"
    #include "NativeExceptions.h"

    int const colorMask = 0xFF;

    int const paramCompression = 0;
    int const paramOrientation = 1;

    JNIEXPORT jboolean JNICALL Java_org_beyka_tiffbitmapfactory_TiffSaver_save
    (JNIEnv *env, jclass clazz, jstring filePath, jintArray img, jobject options, jint img_width, jint img_height, jboolean append) {

        const char *strPath = NULL;
        strPath = env->GetStringUTFChars(filePath, 0);
        LOGIS("nativeTiffOpenForSave", strPath);

        //Get array of jint from jintArray
        jint *c_array;
        c_array = env->GetIntArrayElements(img, NULL);
        if (c_array == NULL) {
            //if array is null - nothing to save
            LOGE("array is null");
            return JNI_FALSE;
        }

        //Get options
        jclass jSaveOptionsClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/TiffSaver$SaveOptions");
        //Get compression mode from options object
        jfieldID gOptions_CompressionModeFieldID = env->GetFieldID(jSaveOptionsClass,
        "compressionMode",
        "Lorg/beyka/tiffbitmapfactory/TiffSaver$CompressionMode;");
        jobject compressionMode = env->GetObjectField(options, gOptions_CompressionModeFieldID);

        jclass compressionModeClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/TiffSaver$CompressionMode");
        jfieldID ordinalFieldID = env->GetFieldID(compressionModeClass, "ordinal", "I");
        jint compressionInt = env->GetIntField(compressionMode, ordinalFieldID);

        //Get image orientation from options object
        jfieldID gOptions_OrientationFieldID = env->GetFieldID(jSaveOptionsClass,
        "orientation",
        "Lorg/beyka/tiffbitmapfactory/TiffSaver$Orientation;");
        jobject orientation = env->GetObjectField(options, gOptions_OrientationFieldID);

        jclass orientationClass = env->FindClass(
        "org/beyka/tiffbitmapfactory/TiffSaver$Orientation");
        jfieldID orientationOrdinalFieldID = env->GetFieldID(orientationClass, "ordinal", "I");
        jint orientationInt = env->GetIntField(orientation, orientationOrdinalFieldID);

        //Get author field if exist
        jfieldID gOptions_authorFieldID = env->GetFieldID(jSaveOptionsClass, "author", "Ljava/lang/String;");
        jstring jAuthor = (jstring)env->GetObjectField(options, gOptions_authorFieldID);
        const char *authorString = NULL;
        if (jAuthor) {
            authorString = env->GetStringUTFChars(jAuthor, 0);
            LOGIS("Author: ", authorString);
        }

        //Get copyright field if exist
        jfieldID gOptions_copyrightFieldID = env->GetFieldID(jSaveOptionsClass, "copyright", "Ljava/lang/String;");
        jstring jCopyright = (jstring)env->GetObjectField(options, gOptions_copyrightFieldID);
        const char *copyrightString = NULL;
        if (jCopyright) {
            copyrightString = env->GetStringUTFChars(jCopyright, 0);
            LOGIS("Copyright: ", copyrightString);
        }

        int pixelsBufferSize = img_width * img_height;
        int* array = (int *) malloc(sizeof(int) * pixelsBufferSize);
        if (!array) {
            throw_not_enought_memory_exception(env, sizeof(int) * pixelsBufferSize);
            return JNI_FALSE;
        }
        for (int i = 0; i < img_width; i++) {
            for (int j = 0; j < img_height; j++) {
                jint crPix = c_array[j * img_width + i];
                int alpha = colorMask & crPix >> 24;
                int red = colorMask & crPix >> 16;
                int green = colorMask & crPix >> 8;
                int blue = colorMask & crPix;

                crPix = (alpha << 24) | (blue << 16) | (green << 8) | (red);
                array[j * img_width + i] = crPix;
            }
        }

        TIFF *output_image;
        int fileDescriptor = -1;

        // Open the TIFF file
        if (!append) {
            if((output_image = TIFFOpen(strPath, "w")) == NULL){
                LOGE("can not open file. Trying file descriptor");
                //if TIFFOpen returns null then try to open file from descriptor
                int mode = O_RDWR | O_CREAT | O_TRUNC | 0;
                fileDescriptor = open(strPath, mode, 0666);
                if (fileDescriptor < 0) {
                    LOGE("Unable to create tif file descriptor");
                    throw_no_such_file_exception(env, filePath);
                    return JNI_FALSE;
                } else {
                    if ((output_image = TIFFFdOpen(fileDescriptor, strPath, "w")) == NULL) {
                        close(fileDescriptor);
                        LOGE("Unable to write tif file");
                        throw_no_such_file_exception(env, filePath);
                        return JNI_FALSE;
                    }
                }
            }
        } else {
            if((output_image = TIFFOpen(strPath, "a")) == NULL){
                LOGE("can not open file. Trying file descriptor");
                //if TIFFOpen returns null then try to open file from descriptor
                int mode = O_RDWR|O_CREAT;
                fileDescriptor = open(strPath, mode, 0666);
                if (fileDescriptor < 0) {
                    LOGE("Unable to create tif file descriptor");
                    throw_no_such_file_exception(env, filePath);
                    return JNI_FALSE;
                } else {
                    if ((output_image = TIFFFdOpen(fileDescriptor, strPath, "a")) == NULL) {
                        close(fileDescriptor);
                        LOGE("Unable to write tif file");
                        throw_no_such_file_exception(env, filePath);
                        return JNI_FALSE;
                    }
                }
            }
        }

        //Write tiff tags for saveing
        TIFFSetField(output_image, TIFFTAG_IMAGEWIDTH, img_width);
        TIFFSetField(output_image, TIFFTAG_IMAGELENGTH, img_height);
        TIFFSetField(output_image, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(output_image, TIFFTAG_SAMPLESPERPIXEL, 4);
        TIFFSetField(output_image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(output_image, TIFFTAG_COMPRESSION, compressionInt);
        TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(output_image, TIFFTAG_ORIENTATION, orientationInt);

        //Write additiona tags
        if (authorString) {
            TIFFSetField(output_image, TIFFTAG_ARTIST, authorString);
        }
        if (copyrightString) {
            TIFFSetField(output_image, TIFFTAG_COPYRIGHT, copyrightString);
        }

        // Write the information to the file
        for (int row = 0; row < img_height; row++) {
            TIFFWriteScanline(output_image, &array[row * img_width], row, 0);
        }
        int ret = TIFFWriteDirectory(output_image);
        LOGII("ret = ", ret);

        // Close the file
        TIFFClose(output_image);

        //if file descriptor was openned then close it

        if (fileDescriptor >= 0) {
            close(fileDescriptor);
        }

        //free temp array
        free (array);
        //Remove variables
        if (authorString) {
            env->ReleaseStringUTFChars(jAuthor, authorString);
        }
        if (copyrightString) {
            env->ReleaseStringUTFChars(jCopyright, copyrightString);
        }
        env->ReleaseStringUTFChars(filePath, strPath);
        env->ReleaseIntArrayElements(img, c_array, 0);

        if (ret == -1) return JNI_FALSE;
        return JNI_TRUE;
    }


    #ifdef __cplusplus
}
#endif