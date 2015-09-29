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

    /**
     * Decode file to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     */
    public static Bitmap decodeFile(File file) {
        return nativeDecodePath(file.getAbsolutePath(), new Options());
    }

    /**
     * Decode file to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in options.outWidth, options.outHeight, options.outDirectoryCount)
     */
    public static Bitmap decodeFile(File file, Options options) {
        return nativeDecodePath(file.getAbsolutePath(), options);
    }

    /**
     * Decode path to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     */
    public static Bitmap decodePath(String path) {
        return nativeDecodePath(path, new Options());
    }

    /**
     * Decode path to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in options.outWidth, options.outHeight, options.outDirectoryCount)
     */
    public static Bitmap decodePath(String path, Options options) {
        options.inPreferredConfig = Bitmap.Config.ARGB_8888;
        return nativeDecodePath(path, options);
    }

    private static native Bitmap nativeDecodePath(String path, Options options);

    /**
     * Options class to specify decoding parameters
     */
    public static final class Options {

        /**
         * Create a default Options object, which if left unchanged will give
         * the same result from the decoder as if null were passed.
         */
        public Options() {
            inSwapRedBlueColors = false;
            inJustDecodeBounds = false;
            inSampleSize = 1;
            inDirectoryCount = 1;

            outWidth = -1;
            outHeight = -1;
            outDirectoryCount = -1;
        }

        /**
         * If set to true, the decoder will swap red and blue colors.
         * <p>Note: If you use this option then your image has wrong encoding</p>
         * <p>Default value is false</p>
         */
        public boolean inSwapRedBlueColors;

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
         * Set directory to extract from image. Default value is 1.
         * To get number of directories in file see {@link #outDirectoryCount}
         */
        public int inDirectoryCount;

        /**
         * If this is non-null, the decoder will try to decode into this
         * internal configuration. If it is null, or the request cannot be met,
         * the decoder will try to pick the best matching config based on the
         * system's screen depth, and characteristics of the original image such
         * as if it has per-pixel alpha (requiring a config that also does).
         *
         * <p>Image are loaded with the {@link Bitmap.Config#ARGB_8888} config by
         * default.</p>
         *
         * <p>In current version supported are {@link Bitmap.Config#ARGB_8888}, {@link Bitmap.Config#ALPHA_8} and {@link Bitmap.Config#RGB_565}</p>
         */
        public Bitmap.Config inPreferredConfig = Bitmap.Config.ARGB_8888;

        /**
         * The resulting width of the bitmap. If {@link #inJustDecodeBounds} is
         * set to false, this will be width of the output bitmap after any
         * scaling is applied. If true, it will be the width of the input image
         * without any accounting for scaling.
         *
         * <p>outWidth will be set to -1 if there is an error trying to decode.</p>
         */
        public int outWidth;

        /**
         * The resulting height of the bitmap. If {@link #inJustDecodeBounds} is
         * set to false, this will be height of the output bitmap after any
         * scaling is applied. If true, it will be the height of the input image
         * without any accounting for scaling.
         *
         * <p>outHeight will be set to -1 if there is an error trying to decode.</p>
         */
        public int outHeight;

        /**
         * The count of directory in image file.
         * <p>outDirectoryCount will be set to -1 if there is an error trying to decode.</p>
         * <p>outDirectoryCount will be set to 0 if {@link #inJustDecodeBounds} is
         * set to false and image decoded successful.</p>
         */
        public int outDirectoryCount;
    }
}
