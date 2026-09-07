package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;
import android.util.Log;

import org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException;
import org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException;
import org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException;

import java.io.File;

/**
 * Created by alexeyba on 7/17/15.
 */
public class TiffBitmapFactory {

    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tifffactory");
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
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int)}.
     * <p></p>
     * Decode file to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodeFile(File file) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodeFile(file, new Options(), null);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int, Options)}.
     * <p></p>
     * Decode file to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when {@link Options#inAvailableMemory} not enought to decode image
     */
    public static Bitmap decodeFile(File file, Options options) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodeFile(file, options, null);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int, Options, IProgressListener)}.
     * <p></p>
     * Decode file to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param file - file to decode
     * @param options - options for decoding
     * @param listener - listener which will receive decoding progress
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when {@link Options#inAvailableMemory} not enought to decode image
     */
    public static Bitmap decodeFile(File file, Options options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        long time = System.currentTimeMillis();
        Log.i("THREAD", "Starting decode " + file.getAbsolutePath());
        Bitmap mbp = nativeDecodePath(file.getAbsolutePath(), options, listener);
        Log.w("THREAD", "elapsed ms: " + (System.currentTimeMillis() - time) + " for " + file.getAbsolutePath());
        return mbp;
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int)}.
     * <p></p>
     * Decode path to bitmap with default options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodePath(String path) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodePath(path, new Options(), null);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int, Options)}.
     * <p></p>
     * Decode path to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodePath(String path, Options options) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodePath(path, options);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffBitmapFactory#decodeFileDescriptor(int, Options, IProgressListener)}.
     * <p></p>
     * Decode path to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param path - file to decode
     * @param options - options for decoding
     * @param listener - listener which will receive decoding progress
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodePath(String path, Options options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        long time = System.currentTimeMillis();
        Log.i("THREAD", "Starting decode " + path);
        Bitmap mbp = nativeDecodePath(path, options, listener);
        Log.w("THREAD", "elapsed ms: " + (System.currentTimeMillis() - time) + " for " + path);
        return mbp;
    }

    /**
     * Decode file descriptor to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param fileDescriptor - file descriptor that represent file to decode
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodeFileDescriptor(int fileDescriptor) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodeFileDescriptor(fileDescriptor, new Options(), null);
    }

    /**
     * Decode file descriptor to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param fileDescriptor - file descriptor that represent file to decode
     * @param options - options for decoding
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodeFileDescriptor(int fileDescriptor, Options options) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return decodeFileDescriptor(fileDescriptor, options, null);
    }

    /**
     * Decode file descriptor to bitmap with specified options. If the specified file name is null,
     * or cannot be decoded into a bitmap, the function returns null.
     * @param fileDescriptor - file descriptor that represent file to decode
     * @param options - options for decoding
     * @param listener - listener which will receive decoding progress
     * @return The decoded bitmap, or null if the image data could not be
     *         decoded, or, if options is non-null, if options requested only the
     *         size be returned (in {@link Options#outWidth}, {@link Options#outHeight}, {@link Options#outDirectoryCount})
     *
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while decoding image
     * @throws org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException when {@code file} not exist or {@code file} is not tiff image
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when for decoding of image system need more memory than {@link Options#inAvailableMemory} or default value
     */
    public static Bitmap decodeFileDescriptor(int fileDescriptor, Options options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        long time = System.currentTimeMillis();
        Log.i("THREAD", "Starting decode descriptor " + fileDescriptor);
        Bitmap mbp = nativeDecodeFD(fileDescriptor, options, listener);
        Log.w("THREAD", "elapsed ms: " + (System.currentTimeMillis() - time) + " for descriptor " + fileDescriptor);
        return mbp;
    }

    private static native Bitmap nativeDecodePath(String path, Options options, IProgressListener listener);

    private static native Bitmap nativeDecodeFD(int fd, Options options, IProgressListener listener);

    /**
     * Close detached file descriptor
     * @param fd
     */
    public static native void closeFd(int fd);

    /**
     * Options class to specify decoding parameterMs
     */
    public static final class Options {

        /**
         * Create a default Options object, which if left unchanged will give
         * the same result from the decoder as if null were passed.
         */
        public Options() {
            isStoped = false;

            inThrowException = false;
            inUseOrientationTag = false;
            inSwapRedBlueColors = false;
            inJustDecodeBounds = false;
            inSampleSize = 1;
            inDirectoryNumber = 0;
            inAvailableMemory = 8000*8000*4;

            outWidth = -1;
            outHeight = -1;
            outDirectoryCount = -1;
            outImageOrientation = Orientation.UNAVAILABLE;
        }

        /**
         * Uses for stoping of native thread
         * @deprecated As of release 0.9.8.4, replaced by {@link Thread#interrupt()}
         */
        private volatile boolean isStoped;

        /**
         * Stop native decoding thread
         * If decoding is started in any thread except main, calling of this method will cause force stop of decoding and returning of null object.
         * @deprecated As of release 0.9.8.4, replaced by {@link Thread#interrupt()}
         */
        public void stop() {
            isStoped = true;
        }

        /**
         * If set to true decoder will rotate and flip image according to TIFFTAG_ORIENTATION.
         * Otherwise image will be returned as it decoded.
         * <p>The specification considers this a baseline tag, but does add that support for orientations other than 1 is not a Baseline TIFF requirement.</p>
         * So for almost all cases this variable may be false.
         * <p>Default value is false</p>
         */
        public boolean inUseOrientationTag;

        /**
         * If set to true, decoder will throw exceptions if some errors appeared while decoding.
         * Otherwise  decoder will just return null.
         * Default value is false
         */
        public boolean inThrowException;

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
         * <p>-1 means memory is unlimited.</p>
         * <p>Default value is 244Mb</p>
         */
        public long inAvailableMemory;

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
         * If this field is non-null - Decoder will decode and return only area specified in {@link DecodeArea} object
         * For decoding not full bitmap, but only part of it - create new {@link DecodeArea} object and specify:
         * <p>{@link DecodeArea#x x}, {@link DecodeArea#y y} - left top corner of decoding area </p>
         * <p>{@link DecodeArea#width width} - width of decoding area </p>
         * <p>{@link DecodeArea#height height} - height of decoding area </p>
         */
        public DecodeArea inDecodeArea;

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
         */
        public int outDirectoryCount;

        /**
         * This parameter returns orientation of decoded image
         * <p>For storing orientation uses {@link org.beyka.tiffbitmapfactory.Orientation ImageOrientation} enum</p>
         * <p>If image wasn't decoded successful this parameter will be equal to {@link Orientation#UNAVAILABLE}</p>
         * <p>This parameter is link to TIFFTAG_ORIENTATION tag</p>
         */
        public Orientation outImageOrientation;

        /**
         * This parameter returns compression scheme of decoded image
         * <p>For storing compression mode uses {@link CompressionScheme CompressionScheme} enum</p>
         * <p>This parameter is link to TIFFTAG_COMPRESSION tag</p>
         */
        public CompressionScheme outCompressionScheme;

        /**
         * How the components of each pixel are stored.
         * <p>The specification defines these values:</p>
         * <ul>
         *  <li>1 = Chunky format. The component values for each pixel are stored contiguously. For example, for RGB data, the data is stored as RGBRGBRGB</li>
         *  <li>2 = Planar format. The components are stored in separate component planes. For example, RGB data is stored with the Red components in one component plane, the Green in another, and the Blue in another.</li>
         * </ul>
         * <p>This parameter is link to TIFFTAG_PLANARCONFIG tag</p>
         */
        public PlanarConfig outPlanarConfig;

        /**
         * The number of components per pixel.
         * <p>SamplesPerPixel is usually 1 for bilevel, grayscale, and palette-color images. SamplesPerPixel is usually 3 for RGB images. If this value is higher, ExtraSamples should give an indication of the meaning of the additional channels.</p>
         */
        public int outSamplePerPixel;

        /**
         * Number of bits per component.
         */
        public int outBitsPerSample;

        /**
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffBitmapFactory.Options#outResolutionUnit} in the ImageWidth direction.
         */
        public float outXResolution;

        /**
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffBitmapFactory.Options#outResolutionUnit} in the ImageHeight direction.
         */
        public float outYResolution;

        /**
         * The unit of measurement for XResolution and YResolution.
         * <p>The specification defines these values: </p>
         * <ul>
         *     <li>RESUNIT_NONE</li>
         *     <li>RESUNIT_INCH</li>
         *     <li>RESUNIT_CENTIMETER</li>
         * </ul>
         * <p>This parameter is link to TIFFTAG_RESOLUTIONUNIT tag</p>
         */
        public ResolutionUnit outResolutionUnit;

        /**
         * The tile width in pixels. This is the number of columns in each tile.
         */
        public int outTileWidth;

        /**
         * The tile height in pixels. This is the number of rows in each tile.
         */
        public int outTileHeight;

        /**
         * The number of rows per strip.
         * <p>RowsPerStrip and ImageLength together tell us the number of strips in the entire image.</p>
         */
        public int outRowPerStrip;

        /**
         * Size for a strip of data in bytes.
         */
        public int outStripSize;

        /**
         * Number of strips in the image.
         */
        public int outNumberOfStrips;

        /**
         * The color space of the image data.
         * <p>This parameter is link to TIFFTAG_PHOTOMETRIC tag</p>
         */
        public Photometric outPhotometric;

        /**
         *  The logical order of bits within a byte.
         * <p>The specification defines these values:</p>
         * <ul>
         *  <li>1 = Pixels with lower column values are stored in the higher-order bits of the byte.</li>
         *  <li>2 = Pixels with lower column values are stored in the lower-order bits of the byte.</li>
         * </ul>
         * <p>This parameter is link to TIFFTAG_PLANARCONFIG tag</p>
         */
        public FillOrder outFillOrder;

        /**
         * Author of file.
         * <p>This parameter is link to TIFFTAG_ARTIST tag</p>
         */
        public String outAuthor = "";

        /**
         * Copyright of file
         * <p>This parameter is link to TIFFTAG_COPYRIGHT tag</p>
         */
        public String outCopyright = "";

        /**
         * A string that describes the subject of the image.
         * <p>This parameter is link to TIFFTAG_IMAGEDESCRIPTION tag</p>
         */
        public String outImageDescription = "";

        /**
         * Name and version number of the software package(s) used to create the image.
         * <p>This parameter is link to TIFFTAG_SOFTWARE tag</p>
         */
        public String outSoftware = "";

        /**
         * Date and time of image creation. The format is: "YYYY:MM:DD HH:MM:SS", with hours like those on a 24-hour clock.
         * <p>This parameter is link to TIFFTAG_DATETIME tag</p>
         */
        public String outDatetime = "";

        /**
         * The computer and/or operating system in use at the time of image creation.
         * <p>This parameter is link to TIFFTAG_HOSTCOMPUTER tag</p>
         */
        public String outHostComputer = "";

    }
}
