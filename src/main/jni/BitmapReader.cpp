//
// Created by beyka on 9/21/17.
//
using namespace std;
#ifdef __cplusplus
extern "C" {
#endif

#include "BitmapReader.h"
#include "BMP.h"
#include <android/bitmap.h>


jobject readBmp
  (JNIEnv *env, jclass clazz, jstring tiffPath, jstring inPath, jobject options, jobject listener)
  {
    FILE *inFile;
       //open bmp file fow reading
            const char *inCPath = NULL;
            inCPath = env->GetStringUTFChars(inPath, 0);
            LOGIS("IN path", inCPath);
            inFile = fopen(inCPath, "rb");
            if (!inFile) {
                //if (throwException) {
                    throw_cant_open_file_exception(env, inPath);
                //}
                LOGES("Can\'t open out file", inCPath);
                env->ReleaseStringUTFChars(inPath, inCPath);
                return JNI_FALSE;
            } else {
                env->ReleaseStringUTFChars(inPath, inCPath);
            }

            //read file header
            size_t byte_count = 54;
            unsigned char *header = (unsigned char *)malloc(sizeof(unsigned char) * byte_count);
            fread(header, 1, byte_count, inFile);
            rewind(inFile);

            //Check is file is JPG image
            bool is_bmp = !strncmp( (const char*)header, "BM", 2 );
            //seek file to begin
            //rewind(inFile);
            if (!is_bmp) {
                LOGE("Not bmp file");
//                if (throwException) {
                    throw_cant_open_file_exception(env, inPath);
  //              }
                return JNI_FALSE;
            } else {
                LOGI("IS BMP");
            }


        unsigned char *buf;
            BITMAPFILEHEADER bmp;
            BITMAPINFOHEADER inf;
            int size;
            int width, height;

            fread(&bmp, sizeof(BITMAPFILEHEADER), 1, inFile);
            //read(fd, &bmp, sizeof(BITMAPFILEHEADER));
            //read(fd, &inf,sizeof(BITMAPINFOHEADER));
            fread(&inf, sizeof(BITMAPINFOHEADER), 1, inFile);

            width = inf.biWidth;
            height = inf.biHeight;
            LOGII("width = ", inf.biWidth);
            LOGII("height = ", inf.biHeight);

            LOGII("bfType",bmp.bfType);
            LOGII("bfSize",bmp.bfSize);
            LOGII("bfOffBits",bmp.bfOffBits);

            LOGII("biSize",inf.biSize);
            LOGII("biPlanes",inf.biPlanes);
            LOGII("biBitCount",inf.biBitCount);
            LOGII("biCompression",inf.biCompression);
            LOGII("biSizeImage",inf.biSizeImage);

            //check if bitnap not 24 bits - throw exception

            /*if (inf.biBitCount != 24) {
                LOGE("Not 24 bits file");
                if (throwException) {
                    throw_cant_open_file_exception(env, inPath);
                }
                return NULL;
            }*/

            LOGII("biSizeImage", inf.biSizeImage);
            if(inf.biSizeImage == 0)  {
                size = width * 3 + width % 4;
                size = size * height;
            } else {
                size = inf.biSizeImage;
            }

            buf = (unsigned char *)malloc(size);
            if (buf == NULL) {
                LOGE("Cant allocate buffer");
                return NULL;
            }

            fseek(inFile, bmp.bfOffBits, SEEK_SET);

            //fread(pixels, sizeof(unsigned char), rowPerStrip * rowSize, inFile);

            int n = fread(buf, sizeof(unsigned char), size, inFile);
            //int n = read(inFile, buf, size);
            LOGII("Read bytes", n);

             int temp, line, i, j, numImgBytes, iw, ih, ind = 0, new_ind = 0;
                uint32 *pixels;

                temp = width * 3;
                line = temp + width % 4;
                numImgBytes = (4 * (width * height));
                pixels = (uint32*)malloc(numImgBytes);

                //memcpy(pixels, buf, numImgBytes);
                numImgBytes = line * height;
                for (i = 0; i < numImgBytes; i++) {
                    unsigned int r, g, b;
                    if (i > temp && (i % line) >= temp) continue;

                    b = buf[i];
                    i++;
                    g = buf[i];
                    i++;
                    r = buf[i];

                    /*iw = ind % width;
                    ih = ind / width;
                    new_ind = iw + (height - ih - 1) * width;*/



                    unsigned int alpha = 255;
                    pixels[ind] = (r| g << 8 | b << 16 | alpha << 24) ;
                    ind++;
                }

                for (i = 0; i < height/2 ;i++) {
                    uint32 *tmp = new uint32[width];
                    memcpy(tmp, pixels + i * width, width * sizeof(uint32));
                    memcpy(pixels + i * width, pixels + (height - 1- i) * width , width * sizeof(uint32));
                    memcpy(pixels + (height - 1- i) * width , tmp, width * sizeof(uint32));
                    free (tmp);
                }

                free(buf);


            jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
                jmethodID methodid = env->GetStaticMethodID(bitmapClass, "createBitmap",
                                                            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");

            jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
            jfieldID bitmapConfigField = env->GetStaticFieldID(bitmapConfigClass, "ARGB_8888",
                                                              "Landroid/graphics/Bitmap$Config;");
                //BitmapConfig
                jobject config = env->GetStaticObjectField(bitmapConfigClass, bitmapConfigField);


               jobject java_bitmap = env->CallStaticObjectMethod(bitmapClass, methodid, width,
                                                                      height, config);

int ret;
void *bitmapPixels;
if ((ret = AndroidBitmap_lockPixels(env, java_bitmap, &bitmapPixels)) < 0) {
        //error
        LOGE("Lock pixels failed");
        return NULL;
    }
    int pixelsCount = width * height;

        memcpy(bitmapPixels, (jint *) pixels, sizeof(jint) * pixelsCount);

AndroidBitmap_unlockPixels(env, java_bitmap);

free(pixels);

                                                                      return java_bitmap;
  }

#ifdef __cplusplus
}
#endif

