#include "NativeDecoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {

int destBytesPerPixel(int config) {
    if (config == 4) {
        return 2;
    }
    if (config == 8) {
        return 1;
    }
    return 4;
}

const char *bitmapConfigName(int config) {
    if (config == 4) {
        return "RGB_565";
    }
    if (config == 8) {
        return "ALPHA_8";
    }
    return "ARGB_8888";
}

uint16_t packRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint8_t luminance(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
}

} // namespace

jobject NativeDecoder::createConfiguredBitmap(int width, int height, int configInt) {
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jfieldID configField = env->GetStaticFieldID(bitmapConfigClass,
            bitmapConfigName(configInt), "Landroid/graphics/Bitmap$Config;");
    jobject config = env->GetStaticObjectField(bitmapConfigClass, configField);
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createMethod = env->GetStaticMethodID(bitmapClass, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject bitmap = env->CallStaticObjectMethod(bitmapClass, createMethod, width, height, config);
    env->DeleteLocalRef(config);
    env->DeleteLocalRef(bitmapConfigClass);
    env->DeleteLocalRef(bitmapClass);
    if (bitmap == NULL || env->ExceptionCheck()) {
        return NULL;
    }
    return bitmap;
}

bool NativeDecoder::canDecodeNativeGray8() {
    uint16_t bits = 0;
    uint16_t spp = 0;
    uint16_t photo = 0;
    uint16_t planar = PLANARCONFIG_CONTIG;
    uint16_t format = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(image, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photo);
    TIFFGetFieldDefaulted(image, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLEFORMAT, &format);
    if (bits != 8 || spp != 1) {
        return false;
    }
    if (photo != PHOTOMETRIC_MINISBLACK && photo != PHOTOMETRIC_MINISWHITE) {
        return false;
    }
    if (planar != PLANARCONFIG_CONTIG) {
        return false;
    }
    if (format != SAMPLEFORMAT_UINT && format != SAMPLEFORMAT_VOID) {
        return false;
    }
    uint16_t extraCount = 0;
    uint16_t *extras = NULL;
    if (TIFFGetField(image, TIFFTAG_EXTRASAMPLES, &extraCount, &extras) && extraCount > 0) {
        return false;
    }
    return true;
}

bool NativeDecoder::canDecodeNativeRgb8() {
    uint16_t bits = 0;
    uint16_t spp = 0;
    uint16_t photo = 0;
    uint16_t planar = PLANARCONFIG_CONTIG;
    uint16_t format = SAMPLEFORMAT_UINT;
    TIFFGetFieldDefaulted(image, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photo);
    TIFFGetFieldDefaulted(image, TIFFTAG_PLANARCONFIG, &planar);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLEFORMAT, &format);
    if (bits != 8 || spp != 3 || photo != PHOTOMETRIC_RGB) {
        return false;
    }
    if (planar != PLANARCONFIG_CONTIG) {
        return false;
    }
    if (format != SAMPLEFORMAT_UINT && format != SAMPLEFORMAT_VOID) {
        return false;
    }
    uint16_t extraCount = 0;
    uint16_t *extras = NULL;
    if (TIFFGetField(image, TIFFTAG_EXTRASAMPLES, &extraCount, &extras) && extraCount > 0) {
        return false;
    }
    return true;
}

bool NativeDecoder::canDecodeNativeBilevel() {
    uint16_t bits = 0;
    uint16_t spp = 0;
    uint16_t photo = 0;
    TIFFGetFieldDefaulted(image, TIFFTAG_BITSPERSAMPLE, &bits);
    TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLESPERPIXEL, &spp);
    TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photo);
    return bits == 1 && spp == 1 &&
           (photo == PHOTOMETRIC_MINISWHITE || photo == PHOTOMETRIC_MINISBLACK);
}

bool NativeDecoder::canStreamToBitmap(int inSampleSize) {
    if (inSampleSize <= 0) {
        return false;
    }
    if (canDecodeNativeBilevel() || canDecodeNativeGray8() || canDecodeNativeRgb8()) {
        return true;
    }
    return inSampleSize == 1 || useSinglePixelSample;
}

bool NativeDecoder::packRasterToBitmap(jint *raster, int rasterWidth, int rasterHeight,
                                       jobject bitmap, int configInt) {
    AndroidBitmapInfo info;
    void *pixels = NULL;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0 || pixels == NULL) {
        return false;
    }

    const int bpp = destBytesPerPixel(configInt);
    for (int y = 0; y < rasterHeight; ++y) {
        uint8_t *row = static_cast<uint8_t *>(pixels) + static_cast<size_t>(y) * info.stride;
        const jint *src = raster + y * rasterWidth;
        if (configInt == ARGB_8888 && info.stride == static_cast<uint32_t>(rasterWidth * 4)) {
            memcpy(row, src, static_cast<size_t>(rasterWidth) * 4);
            continue;
        }
        for (int x = 0; x < rasterWidth; ++x) {
            const uint32_t abgr = static_cast<uint32_t>(src[x]);
            const uint8_t r = static_cast<uint8_t>(abgr);
            const uint8_t g = static_cast<uint8_t>(abgr >> 8);
            const uint8_t b = static_cast<uint8_t>(abgr >> 16);
            const uint8_t a = static_cast<uint8_t>(abgr >> 24);
            uint8_t *pixel = row + static_cast<size_t>(x) * bpp;
            if (configInt == ALPHA_8) {
                *pixel = a;
            } else if (configInt == RGB_565) {
                const uint16_t packed = packRgb565(r, g, b);
                memcpy(pixel, &packed, sizeof(packed));
            } else {
                memcpy(pixel, &abgr, sizeof(abgr));
            }
        }
    }

    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

jobject NativeDecoder::createStreamingBitmap(int inSampleSize, int configInt) {
    const int sourceX = hasBounds ? boundX : 0;
    const int sourceY = hasBounds ? boundY : 0;
    const int sourceWidth = hasBounds ? boundWidth : origwidth;
    const int sourceHeight = hasBounds ? boundHeight : origheight;
    const int logicalW = sourceWidth / inSampleSize;
    const int logicalH = sourceHeight / inSampleSize;
    if (logicalW <= 0 || logicalH <= 0) {
        return NULL;
    }

    int bitmapW = logicalW;
    int bitmapH = logicalH;
    if (useOrientationTag && origorientation > 4) {
        bitmapW = logicalH;
        bitmapH = logicalW;
    }

    const bool tiled = TIFFIsTiled(image) != 0;
    uint32_t blockW = static_cast<uint32_t>(origwidth);
    uint32_t blockH = static_cast<uint32_t>(origheight);
    if (tiled) {
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &blockW);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &blockH);
    } else {
        TIFFGetFieldDefaulted(image, TIFFTAG_ROWSPERSTRIP, &blockH);
        if (blockH == 0) {
            blockH = static_cast<uint32_t>(origheight);
        }
        blockH = std::min(blockH, static_cast<uint32_t>(origheight));
    }
    if (blockW == 0 || blockH == 0) {
        if (throwException) {
            throwDecodeFileException("Invalid block dimensions");
        }
        return NULL;
    }

    const bool nativeBilevel = canDecodeNativeBilevel();
    const bool nativeGray = !nativeBilevel && canDecodeNativeGray8();
    const bool nativeRgb = !nativeBilevel && !nativeGray && canDecodeNativeRgb8();
    uint64_t workBytes = 0;
    if (nativeBilevel || nativeGray || nativeRgb) {
        const tmsize_t nativeSize = tiled ? TIFFTileSize(image) : TIFFScanlineSize(image);
        if (nativeSize <= 0) {
            if (throwException) {
                throwDecodeFileException("Invalid native TIFF block size");
            }
            return NULL;
        }
        workBytes = static_cast<uint64_t>(nativeSize);
    } else {
        workBytes = static_cast<uint64_t>(blockW) * blockH * sizeof(uint32_t);
    }
    if (workBytes > availableMemory || workBytes > SIZE_MAX) {
        if (throwException) {
            throw_not_enought_memory_exception(env, availableMemory, workBytes);
        }
        return NULL;
    }

    if (checkStop()) {
        return NULL;
    }

    jobject bitmap = createConfiguredBitmap(bitmapW, bitmapH, configInt);
    if (bitmap == NULL) {
        return NULL;
    }

    AndroidBitmapInfo info;
    void *pixels = NULL;
    const int expectedFormat = configInt == RGB_565
            ? ANDROID_BITMAP_FORMAT_RGB_565
            : configInt == ALPHA_8
                    ? ANDROID_BITMAP_FORMAT_A_8
                    : ANDROID_BITMAP_FORMAT_RGBA_8888;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        info.format != expectedFormat ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0 || pixels == NULL) {
        env->DeleteLocalRef(bitmap);
        if (throwException) {
            throwDecodeFileException("Cannot access bitmap pixels");
        }
        return NULL;
    }

    auto mapCoord = [&](int lx, int ly, int *ox, int *oy) {
        if (useOrientationTag) {
            switch (origorientation) {
                case ORIENTATION_TOPRIGHT:
                    *ox = logicalW - 1 - lx;
                    *oy = ly;
                    break;
                case ORIENTATION_BOTRIGHT:
                    *ox = logicalW - 1 - lx;
                    *oy = logicalH - 1 - ly;
                    break;
                case ORIENTATION_BOTLEFT:
                    *ox = lx;
                    *oy = logicalH - 1 - ly;
                    break;
                case ORIENTATION_LEFTTOP:
                    *ox = ly;
                    *oy = lx;
                    break;
                case ORIENTATION_RIGHTTOP:
                    *ox = logicalH - 1 - ly;
                    *oy = lx;
                    break;
                case ORIENTATION_RIGHTBOT:
                    *ox = logicalH - 1 - ly;
                    *oy = logicalW - 1 - lx;
                    break;
                case ORIENTATION_LEFTBOT:
                    *ox = ly;
                    *oy = logicalW - 1 - lx;
                    break;
                case ORIENTATION_TOPLEFT:
                default:
                    *ox = lx;
                    *oy = ly;
                    break;
            }
        } else {
            switch (origorientation) {
                case ORIENTATION_TOPRIGHT:
                case ORIENTATION_RIGHTTOP:
                    *ox = logicalW - 1 - lx;
                    *oy = ly;
                    break;
                case ORIENTATION_BOTRIGHT:
                case ORIENTATION_RIGHTBOT:
                    *ox = logicalW - 1 - lx;
                    *oy = logicalH - 1 - ly;
                    break;
                case ORIENTATION_BOTLEFT:
                case ORIENTATION_LEFTBOT:
                    *ox = lx;
                    *oy = logicalH - 1 - ly;
                    break;
                case ORIENTATION_TOPLEFT:
                case ORIENTATION_LEFTTOP:
                default:
                    *ox = lx;
                    *oy = ly;
                    break;
            }
        }
    };

    const int bpp = destBytesPerPixel(configInt);
    auto putRgb = [&](int lx, int ly, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (invertRedAndBlue) {
            const uint8_t tmp = r;
            r = b;
            b = tmp;
        }
        int ox = 0;
        int oy = 0;
        mapCoord(lx, ly, &ox, &oy);
        if (ox < 0 || oy < 0 || ox >= bitmapW || oy >= bitmapH) {
            return;
        }
        uint8_t *pixel = static_cast<uint8_t *>(pixels) +
                         static_cast<size_t>(oy) * info.stride +
                         static_cast<size_t>(ox) * bpp;
        if (configInt == ALPHA_8) {
            *pixel = a;
        } else if (configInt == RGB_565) {
            const uint16_t packed = packRgb565(r, g, b);
            memcpy(pixel, &packed, sizeof(packed));
        } else {
            const uint32_t abgr = static_cast<uint32_t>(r) |
                                  (static_cast<uint32_t>(g) << 8) |
                                  (static_cast<uint32_t>(b) << 16) |
                                  (static_cast<uint32_t>(a) << 24);
            memcpy(pixel, &abgr, sizeof(abgr));
        }
    };

    auto putGray = [&](int lx, int ly, uint8_t gray) {
        putRgb(lx, ly, gray, gray, gray, configInt == ALPHA_8 ? gray : 0xFF);
    };

    auto putBilevel = [&](int lx, int ly, bool black) {
        if (configInt == ALPHA_8) {
            putRgb(lx, ly, 0, 0, 0, black ? 0xFF : 0x00);
        } else if (black) {
            putRgb(lx, ly, 0, 0, 0, 0xFF);
        } else {
            putRgb(lx, ly, 0xFF, 0xFF, 0xFF, 0xFF);
        }
    };

    auto putRgba = [&](int lx, int ly, uint32_t abgr) {
        const uint8_t r = static_cast<uint8_t>(abgr);
        const uint8_t g = static_cast<uint8_t>(abgr >> 8);
        const uint8_t b = static_cast<uint8_t>(abgr >> 16);
        const uint8_t a = static_cast<uint8_t>(abgr >> 24);
        putRgb(lx, ly, r, g, b, a);
    };

    std::unique_ptr<uint8_t, decltype(&free)> block(
            static_cast<uint8_t *>(malloc(static_cast<size_t>(workBytes))), &free);
    if (!block) {
        AndroidBitmap_unlockPixels(env, bitmap);
        env->DeleteLocalRef(bitmap);
        if (throwException) {
            throwDecodeFileException("Cannot allocate decode block");
        }
        return NULL;
    }

    bool ok = true;
    bool cancelled = false;
    const int lastSourceX = sourceX + (logicalW - 1) * inSampleSize;
    const int lastSourceY = sourceY + (logicalH - 1) * inSampleSize;

    auto inSampleY = [&](int y) {
        return y >= sourceY && y <= lastSourceY && (y - sourceY) % inSampleSize == 0;
    };

    sendProgress(0, progressTotal);

    if (nativeBilevel) {
        uint16_t photometric = PHOTOMETRIC_MINISWHITE;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        const bool msbFirst = TIFFIsMSB2LSB(image) != 0;
        auto unpackBit = [&](const uint8_t *rowBytes, int x) {
            const uint8_t packed = rowBytes[x >> 3];
            const uint8_t mask = msbFirst
                    ? static_cast<uint8_t>(0x80U >> (x & 7))
                    : static_cast<uint8_t>(1U << (x & 7));
            const bool one = (packed & mask) != 0;
            return photometric == PHOTOMETRIC_MINISWHITE ? one : !one;
        };

        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = 0; ty < static_cast<uint32_t>(origheight) && ok && !cancelled; ty += blockH) {
                for (uint32_t tx = 0; tx < static_cast<uint32_t>(origwidth); tx += blockW) {
                    if (checkStop()) {
                        cancelled = true;
                        break;
                    }
                    const uint32_t validW = std::min(blockW, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(blockH, static_cast<uint32_t>(origheight) - ty);
                    if (tx + validW <= static_cast<uint32_t>(sourceX) ||
                        ty + validH <= static_cast<uint32_t>(sourceY) ||
                        tx > static_cast<uint32_t>(lastSourceX) ||
                        ty > static_cast<uint32_t>(lastSourceY)) {
                        continue;
                    }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) {
                        ok = false;
                        break;
                    }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) {
                            continue;
                        }
                        const int ly = (srcY - sourceY) / inSampleSize;
                        const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * rowStride;
                        for (int lx = 0; lx < logicalW; ++lx) {
                            const int srcX = sourceX + lx * inSampleSize;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) {
                                continue;
                            }
                            putBilevel(lx, ly, unpackBit(rowBytes, srcX - static_cast<int>(tx)));
                        }
                    }
                    sendProgress(static_cast<jlong>(ty) * origwidth + tx, progressTotal);
                }
            }
        } else {
            for (int row = 0; row <= lastSourceY; ++row) {
                if (checkStop()) {
                    cancelled = true;
                    break;
                }
                if (TIFFReadScanline(image, block.get(), static_cast<uint32_t>(row), 0) < 0) {
                    ok = false;
                    break;
                }
                sendProgress(static_cast<jlong>(row) * origwidth, progressTotal);
                if (!inSampleY(row)) {
                    continue;
                }
                const int ly = (row - sourceY) / inSampleSize;
                for (int lx = 0; lx < logicalW; ++lx) {
                    const int srcX = sourceX + lx * inSampleSize;
                    putBilevel(lx, ly, unpackBit(block.get(), srcX));
                }
            }
        }
    } else if (nativeGray) {
        uint16_t photometric = PHOTOMETRIC_MINISBLACK;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        auto decodeGray = [&](uint8_t sample) {
            return photometric == PHOTOMETRIC_MINISWHITE
                    ? static_cast<uint8_t>(255 - sample)
                    : sample;
        };

        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = 0; ty < static_cast<uint32_t>(origheight) && ok && !cancelled; ty += blockH) {
                for (uint32_t tx = 0; tx < static_cast<uint32_t>(origwidth); tx += blockW) {
                    if (checkStop()) {
                        cancelled = true;
                        break;
                    }
                    const uint32_t validW = std::min(blockW, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(blockH, static_cast<uint32_t>(origheight) - ty);
                    if (tx + validW <= static_cast<uint32_t>(sourceX) ||
                        ty + validH <= static_cast<uint32_t>(sourceY) ||
                        tx > static_cast<uint32_t>(lastSourceX) ||
                        ty > static_cast<uint32_t>(lastSourceY)) {
                        continue;
                    }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) {
                        ok = false;
                        break;
                    }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) {
                            continue;
                        }
                        const int ly = (srcY - sourceY) / inSampleSize;
                        const uint8_t *rowBytes = block.get() +
                                static_cast<size_t>(row) * rowStride;
                        for (int lx = 0; lx < logicalW; ++lx) {
                            const int srcX = sourceX + lx * inSampleSize;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) {
                                continue;
                            }
                            putGray(lx, ly, decodeGray(rowBytes[srcX - static_cast<int>(tx)]));
                        }
                    }
                    sendProgress(static_cast<jlong>(ty) * origwidth + tx, progressTotal);
                }
            }
        } else {
            for (int row = 0; row <= lastSourceY; ++row) {
                if (checkStop()) {
                    cancelled = true;
                    break;
                }
                if (TIFFReadScanline(image, block.get(), static_cast<uint32_t>(row), 0) < 0) {
                    ok = false;
                    break;
                }
                sendProgress(static_cast<jlong>(row) * origwidth, progressTotal);
                if (!inSampleY(row)) {
                    continue;
                }
                const int ly = (row - sourceY) / inSampleSize;
                for (int lx = 0; lx < logicalW; ++lx) {
                    putGray(lx, ly, decodeGray(block.get()[sourceX + lx * inSampleSize]));
                }
            }
        }
    } else if (nativeRgb) {
        auto emitRgb = [&](int lx, int ly, const uint8_t *rgb) {
            uint8_t r = rgb[0];
            uint8_t g = rgb[1];
            uint8_t b = rgb[2];
            const uint8_t a = configInt == ALPHA_8 ? luminance(r, g, b) : 0xFF;
            putRgb(lx, ly, r, g, b, a);
        };

        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = 0; ty < static_cast<uint32_t>(origheight) && ok && !cancelled; ty += blockH) {
                for (uint32_t tx = 0; tx < static_cast<uint32_t>(origwidth); tx += blockW) {
                    if (checkStop()) {
                        cancelled = true;
                        break;
                    }
                    const uint32_t validW = std::min(blockW, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(blockH, static_cast<uint32_t>(origheight) - ty);
                    if (tx + validW <= static_cast<uint32_t>(sourceX) ||
                        ty + validH <= static_cast<uint32_t>(sourceY) ||
                        tx > static_cast<uint32_t>(lastSourceX) ||
                        ty > static_cast<uint32_t>(lastSourceY)) {
                        continue;
                    }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) {
                        ok = false;
                        break;
                    }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) {
                            continue;
                        }
                        const int ly = (srcY - sourceY) / inSampleSize;
                        const uint8_t *rowBytes = block.get() +
                                static_cast<size_t>(row) * rowStride;
                        for (int lx = 0; lx < logicalW; ++lx) {
                            const int srcX = sourceX + lx * inSampleSize;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) {
                                continue;
                            }
                            emitRgb(lx, ly, rowBytes + static_cast<size_t>(srcX - static_cast<int>(tx)) * 3);
                        }
                    }
                    sendProgress(static_cast<jlong>(ty) * origwidth + tx, progressTotal);
                }
            }
        } else {
            for (int row = 0; row <= lastSourceY; ++row) {
                if (checkStop()) {
                    cancelled = true;
                    break;
                }
                if (TIFFReadScanline(image, block.get(), static_cast<uint32_t>(row), 0) < 0) {
                    ok = false;
                    break;
                }
                sendProgress(static_cast<jlong>(row) * origwidth, progressTotal);
                if (!inSampleY(row)) {
                    continue;
                }
                const int ly = (row - sourceY) / inSampleSize;
                for (int lx = 0; lx < logicalW; ++lx) {
                    emitRgb(lx, ly, block.get() +
                            static_cast<size_t>(sourceX + lx * inSampleSize) * 3);
                }
            }
        }
    } else {
        TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        for (uint32_t by = tiled ? (static_cast<uint32_t>(sourceY) / blockH) * blockH : 0;
             by < static_cast<uint32_t>(lastSourceY + 1) && ok && !cancelled; by += blockH) {
            for (uint32_t bx = tiled ? (static_cast<uint32_t>(sourceX) / blockW) * blockW : 0;
                 bx < static_cast<uint32_t>(origwidth); bx += blockW) {
                if (checkStop()) {
                    cancelled = true;
                    break;
                }
                const uint32_t rows = tiled
                        ? blockH
                        : std::min(blockH, static_cast<uint32_t>(origheight) - by);
                if (by + rows <= static_cast<uint32_t>(sourceY) ||
                    by > static_cast<uint32_t>(lastSourceY)) {
                    if (!tiled) {
                        break;
                    }
                    continue;
                }
                ok = tiled
                        ? TIFFReadRGBATileExt(image, bx, by,
                                reinterpret_cast<uint32_t *>(block.get()), 1)
                        : TIFFReadRGBAStripExt(image, by,
                                reinterpret_cast<uint32_t *>(block.get()), 1);
                if (!ok) {
                    break;
                }
                const uint32_t *raster = reinterpret_cast<uint32_t *>(block.get());
                for (int ly = 0; ly < logicalH; ++ly) {
                    const int srcY = sourceY + ly * inSampleSize;
                    if (srcY < static_cast<int>(by) || srcY >= static_cast<int>(by + rows)) {
                        continue;
                    }
                    if (checkStop()) {
                        cancelled = true;
                        break;
                    }
                    const uint32_t sy = static_cast<uint32_t>(srcY) - by;
                    for (int lx = 0; lx < logicalW; ++lx) {
                        const int srcX = sourceX + lx * inSampleSize;
                        if (srcX < static_cast<int>(bx) ||
                            srcX >= static_cast<int>(bx + blockW) ||
                            srcX >= origwidth) {
                            continue;
                        }
                        const uint32_t sx = static_cast<uint32_t>(srcX) - bx;
                        putRgba(lx, ly, raster[(rows - 1 - sy) * blockW + sx]);
                    }
                }
                sendProgress(static_cast<jlong>(by) * origwidth + bx, progressTotal);
                if (!tiled) {
                    break;
                }
            }
        }
    }

    AndroidBitmap_unlockPixels(env, bitmap);
    if (cancelled || checkStop() || env->ExceptionCheck()) {
        env->DeleteLocalRef(bitmap);
        return NULL;
    }
    if (!ok) {
        env->DeleteLocalRef(bitmap);
        if (throwException) {
            throwDecodeFileException("Error reading image");
        }
        return NULL;
    }
    sendProgress(progressTotal, progressTotal);
    return bitmap;
}
