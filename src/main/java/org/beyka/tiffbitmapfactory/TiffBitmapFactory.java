package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;

import org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException;
import org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException;
import org.beyka.tiffbitmapfactory.exceptions.ReadTiffException;

import java.io.File;

/**
 * Created by alexeyba on 7/17/15.
 */
public class TiffBitmapFactory {

    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tifffactory");
    }

    public enum ImageOrientation {
        ORIENTATION_TOPLEFT,
        ORIENTATION_TOPRIGHT,
        ORIENTATION_BOTRIGHT,
        ORIENTATION_BOTLEFT,
        ORIENTATION_LEFTTOP,
        ORIENTATION_RIGHTTOP,
        ORIENTATION_RIGHTBOT,
        ORIENTATION_LEFTBOT,
        UNAVAILABLE;
    }

    public enum ImageConfig {
        /**
         * Each pixel is stored on 4 bytes. Each channel (RGB and alpha
         * for translucency) is stored with 8 bits of precision (256
         * possible values.)
         *
         * This configuration is very flexible and offers the best
         * quality. It should be used whenever possible.
         */
        ARGB_8888 (2),
        /**
         * Each pixel is stored on 2 bytes and only the RGB channels are
         * encoded: red is stored with 5 bits of precision (32 possible
         * values), green is stored with 6 bits of precision (64 possible
         * values) and blue is stored with 5 bits of precision.
         *
         * This configuration can produce slight visual artifacts depending
         * on the configuration of the source. For instance, without
         * dithering, the result might show a greenish tint. To get better
         * results dithering should be applied.
         *
         * This configuration may be useful when using opaque bitmaps
         * that do not require high color fidelity.
         */
        RGB_565 (4),
        /**
         * Each pixel is stored as a single translucency (alpha) channel.
         * This is very useful to efficiently store masks for instance.
         * No color information is stored.
         * With this configuration, each pixel requires 1 byte of memory.
         */
        ALPHA_8 (8);


        final int ordinal;
        ImageConfig(int ordinal) {
            this.ordinal = ordinal;
        }
    }

    /**
     * Decode file to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.ReadTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code file} not exist
     */
    public static Bitmap decodeFile(File file) throws NoSuchFileException, ReadTiffException {
        return nativeDecodePath(file.getAbsolutePath(), new Options());
    }

    /**
     * Decode file to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     * @throws org.beyka.tiffbitmapfactory.exceptions.ReadTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code file} not exist
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when {@link Options#inAvailableMemory} not enought to decode image
     */
    public static Bitmap decodeFile(File file, Options options) throws NoSuchFileException, ReadTiffException, NotEnoughtMemoryException {
        return nativeDecodePath(file.getAbsolutePath(), options);
    }

    /**
     * Decode path to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.ReadTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code path} not exist
     */
    public static Bitmap decodePath(String path) throws NoSuchFileException, ReadTiffException {
        return nativeDecodePath(path, new Options());
    }

    /**
     * Decode path to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.ReadTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code path} not exist
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when {@link Options#inAvailableMemory} not enought to decode image
     */
    public static Bitmap decodePath(String path, Options options) throws NoSuchFileException, ReadTiffException, NotEnoughtMemoryException {
        options.inPreferredConfig = ImageConfig.ARGB_8888;
        return nativeDecodePath(path, options);
    }

    private static synchronized native Bitmap nativeDecodePath(String path, Options options);

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
            inDirectoryNumber = 0;
            inAvailableMemory = -1;

            outWidth = -1;
            outHeight = -1;
            outDirectoryCount = -1;
            outImageOrientation = ImageOrientation.UNAVAILABLE;
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
         * If set to a value &gt; 1, requests the decoder to subsample the original
         * image, returning a smaller image to save memory. The sample size is
         * the number of pixels in either dimension that correspond to a single
         * pixel in the decoded bitmap. For example, inSampleSize == 4 returns
         * an image that is 1/4 the width/height of the original, and 1/16 the
         * number of pixels. Any value &lt;= 1 is treated the same as 1. Note: the
         * decoder uses a final value based on powers of 2, any other value will
         * be rounded down to the nearest power of 2.
         */
        public int inSampleSize;

        /**
         * Set directory to extract from image. Default value is 0.
         * To get number of directories in file see {@link #outDirectoryCount}
         */
        public int inDirectoryNumber;
        
        /**
         * Number of bytes that may be allocated during the Tiff file operations.
         * -1 means memory is unlimited.
         */
        public int inAvailableMemory;

        /**
         * If this is non-null, the decoder will try to decode into this
         * internal configuration. If it is null, or the request cannot be met,
         * the decoder will use {@link ImageConfig#ARGB_8888} configuration.
         *
         * <p>Numeration starts with 0</p>
         *
         * <p>Image are loaded with the {@link ImageConfig#ARGB_8888} config by
         * default.</p>
         *
         * <p>In current version supported are {@link ImageConfig#ARGB_8888}, {@link ImageConfig#ALPHA_8} and {@link ImageConfig#RGB_565}</p>
         */
        public ImageConfig inPreferredConfig = ImageConfig.ARGB_8888;

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
         * The number of current directory.
         * <p>curDirectoryNumber will be set to -1 if there is an error trying to decode.</p>
         */
        public int outCurDirectoryNumber;

        /**
         * The count of directory in image file.
         * <p>outDirectoryCount will be set to -1 if there is an error trying to decode.</p>
         * <p>outDirectoryCount will be set to 0 if {@link #inJustDecodeBounds} is
         * set to false and image decoded successful.</p>
         */
        public int outDirectoryCount;

        /**
         * This parameter returns orientation of decoded image
         * <p>For storing orientation uses {@link org.beyka.tiffbitmapfactory.TiffBitmapFactory.ImageOrientation ImageOrientation} enum</p>
         * <p>If image wasn't decoded successful this parameter will be equal to {@link ImageOrientation#UNAVAILABLE}</p>
         */
        public ImageOrientation outImageOrientation;
    }
}
