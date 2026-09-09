#include "NativeDecoder.h"
#include <algorithm>
#include <climits>
#include <memory>

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
    const uint64_t outputBytes = static_cast<uint64_t>(ow) * oh * 4;
    const bool bilevel = canDecodeBilevelCcittStreaming();
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
    const uint64_t blockBytes = bilevel ? static_cast<uint64_t>(lineSize) : static_cast<uint64_t>(bw) * bh * 4;
    // Include decoded raster, codec buffers and output. A huge single strip is
    // rejected before allocating, even if the requested region is tiny.
    const uint64_t codecBytes = bilevel ? static_cast<uint64_t>(TIFFStripSize(image)) : blockBytes * 3;
    const uint64_t required = outputBytes + blockBytes + codecBytes + 1024 * 1024;
    if (outputBytes > INT_MAX || blockBytes > INT_MAX || required > static_cast<uint64_t>(availableMemory)) {
        if (throwException) throw_not_enought_memory_exception(env, availableMemory, required);
        return NULL;
    }
    std::unique_ptr<uint8_t, decltype(&free)> block(static_cast<uint8_t *>(malloc(blockBytes)), &free);
    if (!block) return fail("Cannot allocate decode block");
    jclass configClass = env->FindClass("android/graphics/Bitmap$Config");
    jobject config = env->GetStaticObjectField(configClass,
            env->GetStaticFieldID(configClass, "ARGB_8888", "Landroid/graphics/Bitmap$Config;"));
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jobject bitmap = env->CallStaticObjectMethod(bitmapClass,
            env->GetStaticMethodID(bitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
            ow, oh, config);
    env->DeleteLocalRef(config); env->DeleteLocalRef(configClass); env->DeleteLocalRef(bitmapClass);
    if (!bitmap || env->ExceptionCheck()) return NULL;
    AndroidBitmapInfo info;
    void *pixels = NULL;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != 0 || AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0)
        return fail("Cannot access bitmap pixels");
    // libtiff's RGBA block reader interprets orientation. Set only this handle's
    // in-memory tag to top-left so all sampling remains in stored coordinates.
    TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    bool ok = true, cancelled = false;
    uint64_t processed = 0;
    const bool swapColors = booleanOption("inSwapRedBlueColors");
    sendProgress(0, static_cast<jlong>(ow) * oh);
    auto put = [&](uint32_t dx, uint32_t dy, uint32_t rgba) {
        if (swapColors)
            rgba = (rgba & 0xff00ff00) | ((rgba & 0xff) << 16) | ((rgba >> 16) & 0xff);
        reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(pixels) + dy * info.stride)[dx] = rgba;
        ++processed;
    };
    if (bilevel) {
        uint16_t photometric = PHOTOMETRIC_MINISWHITE;
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photometric);
        const bool msb = TIFFIsMSB2LSB(image);
        for (int row = 0; row < y + height; ++row) {
            if (checkStop()) { cancelled = true; break; }
            if (TIFFReadScanline(image, block.get(), row, 0) < 0) { ok = false; break; }
            if (row < y || (row - y) % sample) continue;
            for (uint32_t dx = 0; dx < ow; ++dx) {
                const uint32_t sx = x + dx * sample;
                const bool one = (block.get()[sx / 8] & (msb ? 0x80 >> (sx % 8) : 1 << (sx % 8))) != 0;
                const bool black = photometric == PHOTOMETRIC_MINISWHITE ? one : !one;
                put(dx, (row - y) / sample, black ? 0xff000000 : 0xffffffff);
            }
            sendProgress(processed, static_cast<jlong>(ow) * oh);
        }
    } else {
        for (uint32_t by = (y / bh) * bh; by < static_cast<uint32_t>(y + height) && ok && !cancelled; by += bh) {
            for (uint32_t bx = tiled ? (x / bw) * bw : 0; bx < static_cast<uint32_t>(x + width); bx += bw) {
                if (checkStop()) { cancelled = true; break; }
                // Locate the first output sample that can belong to this block before
                // paying the (usually much larger) cost of decompressing it.  When the
                // sample step is larger than a tile, whole tile rows/columns can contain
                // no requested source pixel at all.
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
                        put(dx, dy, reinterpret_cast<uint32_t *>(block.get())[(rows - 1 - sy) * bw + sx]);
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
