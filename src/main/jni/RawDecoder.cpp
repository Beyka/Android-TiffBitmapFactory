#include "NativeDecoder.h"

#include <algorithm>
#include <climits>
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

uint16_t packRgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint8_t luminance(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint8_t>((r * 77 + g * 150 + b * 29) >> 8);
}

} // namespace

// Kept separate from legacy orientation/cropping paths. No process-wide signal
// handlers or longjmp buffers: cleanup follows ordinary return paths.
jobject NativeDecoder::getRawBitmap() {
    auto booleanOption = [&](const char *name) {
        return env->GetBooleanField(optionsObject, env->GetFieldID(jBitmapOptionsClass, name, "Z"));
    };
    auto intOption = [&](const char *name) {
        return env->GetIntField(optionsObject, env->GetFieldID(jBitmapOptionsClass, name, "I"));
    };
    throwException = booleanOption("inThrowException");
    auto fail = [&](const char *message) -> jobject {
        if (throwException && !env->ExceptionCheck()) throwDecodeFileException(message);
        return NULL;
    };
    const int sample = intOption("inSampleSize");
    LOGII("inSampleSize ", sample);
    const int page = intOption("inDirectoryNumber");
    if (sample <= 0 || page < 0)
        return fail("Invalid sample size or page");
    jlong budget = env->GetLongField(optionsObject,
            env->GetFieldID(jBitmapOptionsClass, "inAvailableMemory", "J"));
    if (budget > 0) availableMemory = budget;
    int configInt = ARGB_8888;
    jobject preferred = env->GetObjectField(optionsObject,
            env->GetFieldID(jBitmapOptionsClass, "inPreferredConfig",
                    "Lorg/beyka/tiffbitmapfactory/TiffBitmapFactory$ImageConfig;"));
    if (preferred) {
        jclass cls = env->GetObjectClass(preferred);
        configInt = env->GetIntField(preferred, env->GetFieldID(cls, "ordinal", "I"));
        env->DeleteLocalRef(cls);
        env->DeleteLocalRef(preferred);
        if (configInt != ARGB_8888 && configInt != RGB_565 && configInt != ALPHA_8) {
            configInt = ARGB_8888;
        }
    }
    if (checkStop()) return NULL;
    if (decodingMode == DECODE_MODE_FILE_DESCRIPTOR) {
        if (jFd < 0 || lseek(jFd, 0, SEEK_SET) < 0) return fail("Descriptor is not seekable");
        image = TIFFFdOpen(jFd, "", "r");
    } else {
        const char *path = env->GetStringUTFChars(jPath, NULL);
        if (!path) return NULL;
        image = TIFFOpen(path, "r");
        env->ReleaseStringUTFChars(jPath, path);
    }
    if (!image) {
        if (throwException) throwCantOpenFileException();
        return NULL;
    }
    if (!TIFFSetDirectory(image, page)) return fail("Page does not exist");
    writeDataToOptions(page);
    if (checkStop()) return NULL;
    if (origwidth <= 0 || origheight <= 0) return fail("Invalid TIFF dimensions");
    env->SetIntField(optionsObject, env->GetFieldID(jBitmapOptionsClass, "outWidth", "I"), origwidth);
    env->SetIntField(optionsObject, env->GetFieldID(jBitmapOptionsClass, "outHeight", "I"), origheight);
    if (booleanOption("inJustDecodeBounds")) return NULL;

    int x = 0, y = 0, width = origwidth, height = origheight;
    jobject area = env->GetObjectField(optionsObject, env->GetFieldID(jBitmapOptionsClass,
            "inDecodeArea", "Lorg/beyka/tiffbitmapfactory/DecodeArea;"));
    if (area) {
        jclass cls = env->GetObjectClass(area);
        auto value = [&](const char *name) { return env->GetIntField(area, env->GetFieldID(cls, name, "I")); };
        x = value("x"); y = value("y"); width = value("width"); height = value("height");
        env->DeleteLocalRef(cls); env->DeleteLocalRef(area);
        if (x < 0 || y < 0 || x >= origwidth || y >= origheight || width <= 0 || height <= 0)
            return fail("Invalid raw decode area");
        width = std::min(width, origwidth - x);
        height = std::min(height, origheight - y);
    }
    const uint32_t ow = (static_cast<uint64_t>(width) + sample - 1) / sample;
    const uint32_t oh = (static_cast<uint64_t>(height) + sample - 1) / sample;
    const int bpp = destBytesPerPixel(configInt);
    const uint64_t outputBytes = static_cast<uint64_t>(ow) * oh * bpp;
    const bool nativeBilevel = canDecodeNativeBilevel();
    const bool nativeGray = !nativeBilevel && canDecodeNativeGray8();
    const bool nativeRgb = !nativeBilevel && !nativeGray && canDecodeNativeRgb8();
    const bool tiled = TIFFIsTiled(image);
    uint32_t bw = origwidth, bh = 0;
    if (tiled) {
        TIFFGetField(image, TIFFTAG_TILEWIDTH, &bw);
        TIFFGetField(image, TIFFTAG_TILELENGTH, &bh);
    } else {
        TIFFGetFieldDefaulted(image, TIFFTAG_ROWSPERSTRIP, &bh);
        bh = std::min(bh, static_cast<uint32_t>(origheight));
    }
    const tmsize_t lineSize = TIFFScanlineSize(image);
    if (!bw || !bh || lineSize <= 0) return fail("Invalid block dimensions");
    uint64_t blockBytes = 0;
    bool useEncodedStrip = false;
    if (nativeBilevel || nativeGray || nativeRgb) {
        tmsize_t nativeSize = tiled ? TIFFTileSize(image) : lineSize;
        if (!tiled && (nativeGray || nativeRgb)) {
            const tmsize_t stripSize = TIFFStripSize(image);
            if (stripSize > 0 &&
                static_cast<uint64_t>(stripSize) <= 16ull * 1024 * 1024 &&
                static_cast<uint64_t>(stripSize) <= static_cast<uint64_t>(lineSize) * bh * 2) {
                nativeSize = stripSize;
                useEncodedStrip = true;
            }
        }
        if (nativeSize <= 0) return fail("Invalid native TIFF block size");
        blockBytes = static_cast<uint64_t>(nativeSize);
    } else {
        blockBytes = static_cast<uint64_t>(bw) * bh * 4;
    }
    const uint64_t codecBytes = (nativeBilevel || nativeGray || nativeRgb)
            ? (nativeBilevel && !tiled
                    ? static_cast<uint64_t>(TIFFStripSize(image))
                    : blockBytes)
            : blockBytes * 3;
    const uint64_t required = outputBytes + blockBytes + codecBytes + 1024 * 1024;
    if (outputBytes > INT_MAX || blockBytes > INT_MAX || required > static_cast<uint64_t>(availableMemory)) {
        if (throwException) throw_not_enought_memory_exception(env, availableMemory, required);
        return NULL;
    }
    std::unique_ptr<uint8_t, decltype(&free)> block(static_cast<uint8_t *>(malloc(blockBytes)), &free);
    if (!block) return fail("Cannot allocate decode block");
    jobject bitmap = createConfiguredBitmap(static_cast<int>(ow), static_cast<int>(oh), configInt);
    if (!bitmap || env->ExceptionCheck()) return NULL;
    AndroidBitmapInfo info;
    void *pixels = NULL;
    const int expectedFormat = configInt == RGB_565
            ? ANDROID_BITMAP_FORMAT_RGB_565
            : configInt == ALPHA_8
                    ? ANDROID_BITMAP_FORMAT_A_8
                    : ANDROID_BITMAP_FORMAT_RGBA_8888;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != 0 ||
        info.format != expectedFormat ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0) {
        env->DeleteLocalRef(bitmap);
        return fail("Cannot access bitmap pixels");
    }
    bool ok = true, cancelled = false;
    uint64_t processed = 0;
    const bool swapColors = booleanOption("inSwapRedBlueColors");
    sendProgress(0, static_cast<jlong>(ow) * oh);
    auto putRgb = [&](uint32_t dx, uint32_t dy, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (swapColors) {
            const uint8_t tmp = r;
            r = b;
            b = tmp;
        }
        uint8_t *pixel = static_cast<uint8_t *>(pixels) +
                         static_cast<size_t>(dy) * info.stride +
                         static_cast<size_t>(dx) * bpp;
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
        ++processed;
    };
    auto putRgba = [&](uint32_t dx, uint32_t dy, uint32_t rgba) {
        putRgb(dx, dy, static_cast<uint8_t>(rgba), static_cast<uint8_t>(rgba >> 8),
               static_cast<uint8_t>(rgba >> 16), static_cast<uint8_t>(rgba >> 24));
    };
    auto putGray = [&](uint32_t dx, uint32_t dy, uint8_t gray) {
        putRgb(dx, dy, gray, gray, gray, configInt == ALPHA_8 ? gray : 0xFF);
    };
    auto putBilevel = [&](uint32_t dx, uint32_t dy, bool black) {
        if (configInt == ALPHA_8) {
            putRgb(dx, dy, 0, 0, 0, black ? 0xFF : 0x00);
        } else if (black) {
            putRgb(dx, dy, 0, 0, 0, 0xFF);
        } else {
            putRgb(dx, dy, 0xFF, 0xFF, 0xFF, 0xFF);
        }
    };

    const int lastSourceX = x + static_cast<int>((ow - 1) * sample);
    const int lastSourceY = y + static_cast<int>((oh - 1) * sample);
    auto inSampleY = [&](int row) {
        return row >= y && row <= lastSourceY && (row - y) % sample == 0;
    };
    const uint32_t regionTileY = (static_cast<uint32_t>(y) / bh) * bh;
    const uint32_t regionTileX = (static_cast<uint32_t>(x) / bw) * bw;
    const uint32_t regionEndY = std::min(static_cast<uint32_t>(lastSourceY) + 1,
                                         static_cast<uint32_t>(origheight));
    const uint32_t regionEndX = std::min(static_cast<uint32_t>(lastSourceX) + 1,
                                         static_cast<uint32_t>(origwidth));
    const int stripStartRow = static_cast<int>(regionTileY);

    if (nativeBilevel) {
        uint16_t photometric = PHOTOMETRIC_MINISWHITE;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        const bool msb = TIFFIsMSB2LSB(image) != 0;
        auto unpackBit = [&](const uint8_t *rowBytes, int sx) {
            const bool one = (rowBytes[sx >> 3] &
                    (msb ? static_cast<uint8_t>(0x80U >> (sx & 7))
                         : static_cast<uint8_t>(1U << (sx & 7)))) != 0;
            return photometric == PHOTOMETRIC_MINISWHITE ? one : !one;
        };
        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = regionTileY; ty < regionEndY && ok && !cancelled; ty += bh) {
                for (uint32_t tx = regionTileX; tx < regionEndX; tx += bw) {
                    const uint32_t validW = std::min(bw, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(bh, static_cast<uint32_t>(origheight) - ty);
                    const uint32_t dy0 = ty <= static_cast<uint32_t>(y) ? 0 : (ty - y + sample - 1) / sample;
                    const uint32_t dx0 = tx <= static_cast<uint32_t>(x) ? 0 : (tx - x + sample - 1) / sample;
                    if (dy0 >= oh || static_cast<uint64_t>(y) + static_cast<uint64_t>(dy0) * sample >=
                            static_cast<uint64_t>(ty) + validH ||
                        dx0 >= ow || static_cast<uint64_t>(x) + static_cast<uint64_t>(dx0) * sample >=
                            static_cast<uint64_t>(tx) + validW) {
                        continue;
                    }
                    if (checkStop()) { cancelled = true; break; }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) { ok = false; break; }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) continue;
                        const uint32_t dy = static_cast<uint32_t>((srcY - y) / sample);
                        const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * rowStride;
                        for (uint32_t dx = 0; dx < ow; ++dx) {
                            const int srcX = x + static_cast<int>(dx) * sample;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) continue;
                            putBilevel(dx, dy, unpackBit(rowBytes, srcX - static_cast<int>(tx)));
                        }
                    }
                    sendProgress(processed, static_cast<jlong>(ow) * oh);
                }
            }
        } else {
            for (int row = stripStartRow; row <= lastSourceY; ++row) {
                if (((row - stripStartRow) & 31) == 0 && checkStop()) { cancelled = true; break; }
                if (TIFFReadScanline(image, block.get(), row, 0) < 0) { ok = false; break; }
                if (!inSampleY(row)) continue;
                const uint32_t dy = static_cast<uint32_t>((row - y) / sample);
                for (uint32_t dx = 0; dx < ow; ++dx) {
                    putBilevel(dx, dy, unpackBit(block.get(), x + static_cast<int>(dx) * sample));
                }
                sendProgress(processed, static_cast<jlong>(ow) * oh);
            }
        }
    } else if (nativeGray) {
        uint16_t photometric = PHOTOMETRIC_MINISBLACK;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        auto decodeGray = [&](uint8_t value) {
            return photometric == PHOTOMETRIC_MINISWHITE
                    ? static_cast<uint8_t>(255 - value) : value;
        };
        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = regionTileY; ty < regionEndY && ok && !cancelled; ty += bh) {
                for (uint32_t tx = regionTileX; tx < regionEndX; tx += bw) {
                    const uint32_t validW = std::min(bw, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(bh, static_cast<uint32_t>(origheight) - ty);
                    const uint32_t dy0 = ty <= static_cast<uint32_t>(y) ? 0 : (ty - y + sample - 1) / sample;
                    const uint32_t dx0 = tx <= static_cast<uint32_t>(x) ? 0 : (tx - x + sample - 1) / sample;
                    if (dy0 >= oh || static_cast<uint64_t>(y) + static_cast<uint64_t>(dy0) * sample >=
                            static_cast<uint64_t>(ty) + validH ||
                        dx0 >= ow || static_cast<uint64_t>(x) + static_cast<uint64_t>(dx0) * sample >=
                            static_cast<uint64_t>(tx) + validW) {
                        continue;
                    }
                    if (checkStop()) { cancelled = true; break; }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) { ok = false; break; }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) continue;
                        const uint32_t dy = static_cast<uint32_t>((srcY - y) / sample);
                        const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * rowStride;
                        for (uint32_t dx = 0; dx < ow; ++dx) {
                            const int srcX = x + static_cast<int>(dx) * sample;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) continue;
                            putGray(dx, dy, decodeGray(rowBytes[srcX - static_cast<int>(tx)]));
                        }
                    }
                    sendProgress(processed, static_cast<jlong>(ow) * oh);
                }
            }
        } else if (useEncodedStrip) {
            for (uint32_t by = regionTileY; by < regionEndY && ok && !cancelled; by += bh) {
                if (checkStop()) { cancelled = true; break; }
                const uint32_t rows = std::min(bh, static_cast<uint32_t>(origheight) - by);
                if (TIFFReadEncodedStrip(image, TIFFComputeStrip(image, by, 0),
                        block.get(), static_cast<tmsize_t>(blockBytes)) < 0) {
                    ok = false;
                    break;
                }
                for (uint32_t row = 0; row < rows; ++row) {
                    const int srcY = static_cast<int>(by + row);
                    if (!inSampleY(srcY)) continue;
                    const uint32_t dy = static_cast<uint32_t>((srcY - y) / sample);
                    const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * lineSize;
                    for (uint32_t dx = 0; dx < ow; ++dx) {
                        putGray(dx, dy, decodeGray(rowBytes[x + static_cast<int>(dx) * sample]));
                    }
                }
                sendProgress(processed, static_cast<jlong>(ow) * oh);
            }
        } else {
            for (int row = stripStartRow; row <= lastSourceY; ++row) {
                if (((row - stripStartRow) & 31) == 0 && checkStop()) { cancelled = true; break; }
                if (TIFFReadScanline(image, block.get(), row, 0) < 0) { ok = false; break; }
                if (!inSampleY(row)) continue;
                const uint32_t dy = static_cast<uint32_t>((row - y) / sample);
                for (uint32_t dx = 0; dx < ow; ++dx) {
                    putGray(dx, dy, decodeGray(block.get()[x + static_cast<int>(dx) * sample]));
                }
                if ((row & 31) == 0) sendProgress(processed, static_cast<jlong>(ow) * oh);
            }
        }
    } else if (nativeRgb) {
        auto emitRgb = [&](uint32_t dx, uint32_t dy, const uint8_t *rgb) {
            const uint8_t a = configInt == ALPHA_8 ? luminance(rgb[0], rgb[1], rgb[2]) : 0xFF;
            putRgb(dx, dy, rgb[0], rgb[1], rgb[2], a);
        };
        if (tiled) {
            const tmsize_t rowStride = TIFFTileRowSize(image);
            for (uint32_t ty = regionTileY; ty < regionEndY && ok && !cancelled; ty += bh) {
                for (uint32_t tx = regionTileX; tx < regionEndX; tx += bw) {
                    const uint32_t validW = std::min(bw, static_cast<uint32_t>(origwidth) - tx);
                    const uint32_t validH = std::min(bh, static_cast<uint32_t>(origheight) - ty);
                    const uint32_t dy0 = ty <= static_cast<uint32_t>(y) ? 0 : (ty - y + sample - 1) / sample;
                    const uint32_t dx0 = tx <= static_cast<uint32_t>(x) ? 0 : (tx - x + sample - 1) / sample;
                    if (dy0 >= oh || static_cast<uint64_t>(y) + static_cast<uint64_t>(dy0) * sample >=
                            static_cast<uint64_t>(ty) + validH ||
                        dx0 >= ow || static_cast<uint64_t>(x) + static_cast<uint64_t>(dx0) * sample >=
                            static_cast<uint64_t>(tx) + validW) {
                        continue;
                    }
                    if (checkStop()) { cancelled = true; break; }
                    if (TIFFReadTile(image, block.get(), tx, ty, 0, 0) < 0) { ok = false; break; }
                    for (uint32_t row = 0; row < validH; ++row) {
                        const int srcY = static_cast<int>(ty + row);
                        if (!inSampleY(srcY)) continue;
                        const uint32_t dy = static_cast<uint32_t>((srcY - y) / sample);
                        const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * rowStride;
                        for (uint32_t dx = 0; dx < ow; ++dx) {
                            const int srcX = x + static_cast<int>(dx) * sample;
                            if (srcX < static_cast<int>(tx) || srcX >= static_cast<int>(tx + validW)) continue;
                            emitRgb(dx, dy, rowBytes + static_cast<size_t>(srcX - static_cast<int>(tx)) * 3);
                        }
                    }
                    sendProgress(processed, static_cast<jlong>(ow) * oh);
                }
            }
        } else if (useEncodedStrip) {
            for (uint32_t by = regionTileY; by < regionEndY && ok && !cancelled; by += bh) {
                if (checkStop()) { cancelled = true; break; }
                const uint32_t rows = std::min(bh, static_cast<uint32_t>(origheight) - by);
                if (TIFFReadEncodedStrip(image, TIFFComputeStrip(image, by, 0),
                        block.get(), static_cast<tmsize_t>(blockBytes)) < 0) {
                    ok = false;
                    break;
                }
                for (uint32_t row = 0; row < rows; ++row) {
                    const int srcY = static_cast<int>(by + row);
                    if (!inSampleY(srcY)) continue;
                    const uint32_t dy = static_cast<uint32_t>((srcY - y) / sample);
                    const uint8_t *rowBytes = block.get() + static_cast<size_t>(row) * lineSize;
                    for (uint32_t dx = 0; dx < ow; ++dx) {
                        emitRgb(dx, dy, rowBytes + static_cast<size_t>(x + static_cast<int>(dx) * sample) * 3);
                    }
                }
                sendProgress(processed, static_cast<jlong>(ow) * oh);
            }
        } else {
            for (int row = stripStartRow; row <= lastSourceY; ++row) {
                if (((row - stripStartRow) & 31) == 0 && checkStop()) { cancelled = true; break; }
                if (TIFFReadScanline(image, block.get(), row, 0) < 0) { ok = false; break; }
                if (!inSampleY(row)) continue;
                const uint32_t dy = static_cast<uint32_t>((row - y) / sample);
                for (uint32_t dx = 0; dx < ow; ++dx) {
                    emitRgb(dx, dy, block.get() + static_cast<size_t>(x + static_cast<int>(dx) * sample) * 3);
                }
                if ((row & 31) == 0) sendProgress(processed, static_cast<jlong>(ow) * oh);
            }
        }
    } else {
        TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        for (uint32_t by = (y / bh) * bh; by < static_cast<uint32_t>(y + height) && ok && !cancelled; by += bh) {
            for (uint32_t bx = tiled ? (x / bw) * bw : 0; bx < static_cast<uint32_t>(x + width); bx += bw) {
                if (checkStop()) { cancelled = true; break; }
                const uint32_t rows = tiled ? bh : std::min(bh, static_cast<uint32_t>(origheight) - by);
                const uint32_t dy0 = by <= static_cast<uint32_t>(y) ? 0 : (by - y + sample - 1) / sample;
                const uint32_t dx0 = bx <= static_cast<uint32_t>(x) ? 0 : (bx - x + sample - 1) / sample;
                if (dy0 >= oh || static_cast<uint64_t>(y) + static_cast<uint64_t>(dy0) * sample >= static_cast<uint64_t>(by) + rows ||
                    dx0 >= ow || static_cast<uint64_t>(x) + static_cast<uint64_t>(dx0) * sample >= static_cast<uint64_t>(bx) + bw) {
                    continue;
                }
                ok = tiled ? TIFFReadRGBATileExt(image, bx, by, reinterpret_cast<uint32_t *>(block.get()), 1)
                           : TIFFReadRGBAStripExt(image, by, reinterpret_cast<uint32_t *>(block.get()), 1);
                if (!ok) break;
                for (uint32_t dy = dy0; dy < oh && y + dy * sample < by + rows; ++dy) {
                    if (checkStop()) { cancelled = true; break; }
                    for (uint32_t dx = dx0; dx < ow && x + dx * sample < bx + bw; ++dx) {
                        const uint32_t sy = y + dy * sample - by, sx = x + dx * sample - bx;
                        putRgba(dx, dy, reinterpret_cast<uint32_t *>(block.get())[(rows - 1 - sy) * bw + sx]);
                    }
                }
                sendProgress(processed, static_cast<jlong>(ow) * oh);
                if (!tiled) break;
            }
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);
    if (cancelled || checkStop() || !ok || env->ExceptionCheck()) {
        env->DeleteLocalRef(bitmap);
        return (!ok && !cancelled) ? fail("Cannot decode TIFF block") : NULL;
    }
    sendProgress(static_cast<jlong>(ow) * oh, static_cast<jlong>(ow) * oh);
    return bitmap;
}
