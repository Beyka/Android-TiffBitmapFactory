package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;

import java.io.File;

/**
 * Created by alexeyba on 7/17/15.
 */
public class TiffBitmapFactory {

    static {
        System.loadLibrary("tifffactory");
    }

    public static Bitmap decodeFile(File file) {
        return nativeDecodePath(file.getAbsolutePath(), new Options());
    }

    public static Bitmap decodeFile(File file, Options options) {
        return nativeDecodePath(file.getAbsolutePath(), options);
    }

    public static Bitmap decodePath(String path) {
        return nativeDecodePath(path, new Options());
    }

    public static Bitmap decodePath(String path, Options options) {
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        return nativeDecodePath(path, options);
    }

    public static int getDirectoryCount(String path) {
        return nativeGetDirectoryCount(path);
    }

    private static native Bitmap nativeDecodePath(String path, Options options);

    private static native int nativeGetDirectoryCount(String path);

    public static final class Options {

        /**
         * Create a default Options object, which if left unchanged will give
         * the same result from the decoder as if null were passed.
         */
        public Options() {
            inJustDecodeBounds = false;
            inSampleSize = 1;
            directoryCount = 1;
        }

        /**
         * If set to true, the decoder will return null (no bitmap), but
         * the out... fields will still be set, allowing the caller to query
         * the bitmap without having to allocate the memory for its pixels.
         */
        public boolean inJustDecodeBounds;

        /**
         * If set to a value > 1, requests the decoder to subsample the original
         * image, returning a smaller image to save memory. The sample size is
         * the number of pixels in either dimension that correspond to a single
         * pixel in the decoded bitmap. For example, inSampleSize == 4 returns
         * an image that is 1/4 the width/height of the original, and 1/16 the
         * number of pixels. Any value <= 1 is treated the same as 1. Note: the
         * decoder uses a final value based on powers of 2, any other value will
         * be rounded down to the nearest power of 2.
         */
        public int inSampleSize;

        /**
         * Set directory to extract from bitmap. Default value is 1.
         * To get number of directories in file use {@link #getDirectoriesSize(String)}
         */
        public int directoryCount;

        /**
         * Not use in current revision. Bitmap always decode with ARGB_8888
         *
         * If this is non-null, the decoder will try to decode into this
         * internal configuration. If it is null, or the request cannot be met,
         * the decoder will try to pick the best matching config based on the
         * system's screen depth, and characteristics of the original image such
         * as if it has per-pixel alpha (requiring a config that also does).
         *
         * Image are loaded with the {@link Bitmap.Config#ARGB_8888} config by
         * default.
         */
        public Bitmap.Config inPreferredConfig = Bitmap.Config.ARGB_8888;
    }
}
