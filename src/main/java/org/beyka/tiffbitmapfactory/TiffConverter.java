package org.beyka.tiffbitmapfactory;

import org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException;
import org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException;
import org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException;

import java.io.File;

/**
 * Created by beyka on 5/9/17.
 */

public class TiffConverter {
    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tiffconverter");
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertToTiff(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert any of supported formats from {@link ImageFormat} to tiff
     * @param inFile path to income tiff file
     * @param outFile path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     *
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertToTiff(File inFile, File outFile, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return convertToTiff(
                inFile == null ? null : inFile.getAbsolutePath(),
                outFile == null ? null : outFile.getAbsolutePath(),
                options,
                listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertToTiff(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert any of supported formats from {@link ImageFormat} to tiff
     * @param inPath path to income tiff file
     * @param outPath path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     *
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertToTiff(String inPath, String outPath, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        if (inPath == null || outPath == null) {
            if (options != null && options.throwExceptions) {
                throw new CantOpenFileException((String) null);
            }
            return false;
        }
        switch (getImageType(inPath)) {
            case JPEG:
                return convertJpgTiff(inPath, outPath, options, listener);
            case PNG:
                return convertPngTiff(inPath, outPath, options, listener);
            case BMP:
                return convertBmpTiff(inPath, outPath, options, listener);
            case TIFF:
                // TODO: 9/19/17 make convert tiff to tiff method
                break;
        }
        return false;
    }

    /**
     * Convert any of supported formats from {@link ImageFormat} to tiff
     * This method don't close file descriptor
     * @param inFd file descriptor that represent income file
     * @param outFd file descriptor that represent outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     *
     * @return true if convert process have been successful
     */
    public static boolean convertToTiff(int inFd, int outFd, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        if (inFd < 0 || outFd < 0) {
            if (options != null && options.throwExceptions) {
                throw new CantOpenFileException(inFd < 0 ? inFd : outFd);
            }
            return false;
        }
        switch (getImageTypeFd(inFd)) {
            case JPEG:
                return convertJpgTiffFd(inFd, outFd, options, listener);
            case PNG:
                return convertPngTiffFd(inFd, outFd, options, listener);
            case BMP:
                return convertBmpTiffFd(inFd, outFd, options, listener);
            case TIFF:
                // TODO: 9/19/17 make convert tiff to tiff method
                break;
        }
        return false;
    }


    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertTiffPngFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert tiff to png file. Uses direct data read method, that decrease memory usage
     * @param tiff path to income tiff file
     * @param png path to outcome png file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertTiffPng(String tiff, String png, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(tiff, png, options)
                && nativeConvertTiffPng(tiff, png, options, listener);
    }

    /**
     * Convert tiff to png file. Uses direct data read method, that decrease memory usage
     * @param tiff file descriptor that represent income tiff file
     * @param png file descriptor that represent outcome png file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertTiffPngFd(int tiff, int png, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(tiff, png, options)
                && nativeConvertTiffPngFd(tiff, png, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertPngTiffFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert png to tiff file. Uses direct data read method, that decrease memory usage.
     * @param png path to income png file
     * @param tiff path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertPngTiff(String png, String tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(png, tiff, options)
                && nativeConvertPngTiff(png, tiff, options, listener);
    }

    /**
     * Convert png to tiff file. Uses direct data read method, that decrease memory usage.
     * This method don't close file descriptor
     * @param png file descriptor that represent income png file
     * @param tiff file descriptor that represent outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertPngTiffFd(int png, int tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(png, tiff, options)
                && nativeConvertPngTiffFd(png, tiff, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertTiffJpgFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert tiff to jpeg file. Uses direct data read method, that decrease memory usage
     * @param tiff path to income tiff file
     * @param jpg path to outcome jpeg file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertTiffJpg(String tiff, String jpg, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(tiff, jpg, options)
                && nativeConvertTiffJpg(tiff, jpg, options, listener);
    }

    /**
     * Convert tiff to jpg file. Uses direct data read method, that decrease memory usage
     * @param tiff file descriptor that represent income tiff file
     * @param jpg file descriptor that represent outcome jpg file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertTiffJpgFd(int tiff, int jpg, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(tiff, jpg, options)
                && nativeConvertTiffJpgFd(tiff, jpg, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertJpgTiffFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert jpeg to tiff file. Uses direct data read method, that decrease memory usage.
     * @param jpg path to income jpeg file
     * @param tiff path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertJpgTiff(String jpg, String tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(jpg, tiff, options)
                && nativeConvertJpgTiff(jpg, tiff, options, listener);
    }

    /**
     * Convert jpeg to tiff file. Uses direct data read method, that decrease memory usage.
     * This method don't close file descriptor
     * @param jpg file descriptor that represent income jpeg file
     * @param tiff file descriptor that represent outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertJpgTiffFd(int jpg, int tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(jpg, tiff, options)
                && nativeConvertJpgTiffFd(jpg, tiff, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertTiffBmpFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert tiff to bmp file. Uses direct data read method, that decrease memory usage
     * @param tiff path to income tiff file
     * @param bmp path to outcome jpeg file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertTiffBmp(String tiff, String bmp, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(tiff, bmp, options)
                && nativeConvertTiffBmp(tiff, bmp, options, listener);
    }

    /**
     * Convert tiff to bmp file. Uses direct data read method, that decrease memory usage
     * @param tiff file descriptor that represent income tiff file
     * @param bmp file descriptor that represent outcome bmp file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertTiffBmpFd(int tiff, int bmp, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(tiff, bmp, options)
                && nativeConvertTiffBmpFd(tiff, bmp, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#convertBmpTiffFd(int, int, ConverterOptions, IProgressListener)}.
     * <p></p>
     * Convert bmp to tiff file. Uses direct data read method, that decrease memory usage.
     * @param bmp path to income bmp file
     * @param tiff path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    @Deprecated
    public static boolean convertBmpTiff(String bmp, String tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidPaths(bmp, tiff, options)
                && nativeConvertBmpTiff(bmp, tiff, options, listener);
    }

    /**
     * Convert bmp to tiff file. Uses direct data read method, that decrease memory usage.
     * This method don't close file descriptor
     * @param bmp file descriptor that represent income bmp file
     * @param tiff file descriptor that represent outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static boolean convertBmpTiffFd(int bmp, int tiff, ConverterOptions options, IProgressListener listener) throws CantOpenFileException, DecodeTiffException, NotEnoughtMemoryException {
        return hasValidFileDescriptors(bmp, tiff, options)
                && nativeConvertBmpTiffFd(bmp, tiff, options, listener);
    }

    /**
     * @deprecated Since Android Q is released. You can use this method with scoped storage.
     * Otherwise use {@link TiffConverter#getImageTypeFd(int)}.
     * <p></p>
     * Return type of file.
     * @param path - file path
     * @return one of {@link ImageFormat} or {@link ImageFormat#UNKNOWN}
     */
    @Deprecated
    public static ImageFormat getImageType(String path) {
        return path == null ? ImageFormat.UNKNOWN : nativeGetImageType(path);
    }

    /**
     * Return type of file. This method don't close file descriptor
     * @param fd - file descriptor for file
     * @return one of {@link ImageFormat} or {@link ImageFormat#UNKNOWN}
     */
    public static ImageFormat getImageTypeFd(int fd) {
        return fd < 0 ? ImageFormat.UNKNOWN : nativeGetImageTypeFd(fd);
    }

    /**
     * Close a detached file descriptor.
     *
     * @deprecated File descriptors passed to this library remain owned by the
     * caller. Prefer closing the owning {@code ParcelFileDescriptor}. This
     * method remains for descriptors explicitly transferred with
     * {@code ParcelFileDescriptor.detachFd()}.
     * @param fd detached file descriptor owned by the caller
     */
    @Deprecated
    public static void closeFd(int fd) {
        nativeCloseFd(fd);
    }

    private static native void nativeCloseFd(int fd);
    private static native ImageFormat nativeGetImageType(String path);
    private static native ImageFormat nativeGetImageTypeFd(int fd);

    private static native boolean nativeConvertTiffPng(String tiff, String png, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertTiffPngFd(int tiff, int png, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertPngTiff(String png, String tiff, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertPngTiffFd(int png, int tiff, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertTiffJpg(String tiff, String jpg, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertTiffJpgFd(int tiff, int jpg, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertJpgTiff(String jpg, String tiff, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertJpgTiffFd(int jpg, int tiff, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertTiffBmp(String tiff, String bmp, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertTiffBmpFd(int tiff, int bmp, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertBmpTiff(String bmp, String tiff, ConverterOptions options, IProgressListener listener);
    private static native boolean nativeConvertBmpTiffFd(int bmp, int tiff, ConverterOptions options, IProgressListener listener);

    private static boolean hasValidPaths(String inputPath, String outputPath, ConverterOptions options) {
        if (inputPath != null && outputPath != null) {
            return true;
        }
        if (options != null && options.throwExceptions) {
            throw new CantOpenFileException((String) null);
        }
        return false;
    }

    private static boolean hasValidFileDescriptors(int inputFd, int outputFd, ConverterOptions options) {
        if (inputFd >= 0 && outputFd >= 0) {
            return true;
        }
        if (options != null && options.throwExceptions) {
            throw new CantOpenFileException(inputFd < 0 ? inputFd : outputFd);
        }
        return false;
    }

    public static final class ConverterOptions {

        public ConverterOptions() {
            availableMemory = 8000*8000*4;
            appendTiff = false;
            resUnit = ResolutionUnit.NONE;
            compressionScheme = CompressionScheme.NONE;
            //orientation = Orientation.TOP_LEFT;
            throwExceptions = false;
            isStoped = false;
        }

        /**
         * Uses for stoping of native thread
         * @deprecated As of release 0.9.8.4, replaced by {@link Thread#interrupt()}
         */
        private volatile boolean isStoped;

        /**
         * Stop native convert thread
         * If converting is started in any thread except main, calling of this method will cause force stop of converting and returning of false.
         * @deprecated As of release 0.9.8.4, replaced by {@link Thread#interrupt()}
         */
        @Deprecated
        public void stop() {
            isStoped = true;
        }

        /**
         * Number of bytes that may be allocated during the file operations.
         * <p>-1 means memory is unlimited.</p>
         * <p>Default value is 244Mb</p>
         */
        public long availableMemory;

        /**
         * If set to true, converter will throw exceptions if some errors appeared while converting.
         * Otherwise converter will just return false.
         * Default value is false
         */
        public boolean throwExceptions;

        /**
         * <b>For converting from TIFF to ANY cases</b>
         * <p>Tiff directory that should be converted</p>
         */
        public int readTiffDirectory;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * <p>If this value set to true while converting any to tiff - new tiff directory will be created
         * and added to existed tiff file.</p>
         * Otherwise file will be overwritten.
         * <p>Default value is false</p>
         */
        public boolean appendTiff;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * Compression scheme used on the image data.
         * <p>Default value is {@link CompressionScheme#NONE COMPRESSION_NONE}</p>
         * <p>This parameter is link to TIFFTAG_COMPRESSION tag</p>
         */
        public CompressionScheme compressionScheme;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * {@link org.beyka.tiffbitmapfactory.Orientation Orientation} that will used for saving image
         * <p>By default uses {@link org.beyka.tiffbitmapfactory.Orientation#TOP_LEFT ORIENTATION_TOPLEFT} </p>
         * <p>This parameter is link to TIFFTAG_ORIENTATION tag</p>
         */
        //public Orientation orientation;

        /**
         * If this field is non-null - Converter will use only area specified in {@link DecodeArea} object to convert.
         * <p><b>This field works only when convert from TIFF.</b></p>
         * <p>For decoding not full bitmap, but only part of it - create new {@link DecodeArea} object and specify:</p>
         * <p>{@link DecodeArea#x x}, {@link DecodeArea#y y} - left top corner of decoding area </p>
         * <p>{@link DecodeArea#width width} - width of decoding area </p>
         * <p>{@link DecodeArea#height height} - height of decoding area </p>
         */
        public DecodeArea inTiffDecodeArea;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffConverter.ConverterOptions#resUnit} in the ImageWidth direction.
         * <p> It is not mandatory that the image be actually displayed or printed at the size implied by this parameter.
         * It is up to the application to use this information as it wishes.</p>
         * <p>Defualt value is 0</p>
         */
        public float xResolution;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffConverter.ConverterOptions#resUnit} in the ImageHeight direction.
         * <p> It is not mandatory that the image be actually displayed or printed at the size implied by this parameter.
         * It is up to the application to use this information as it wishes.</p>
         * <p>Defualt value is 0</p>
         */
        public float yResolution;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * The unit of measurement for XResolution and YResolution.
         * <p>To be used with xResolution and yResolution. </p>
         * <p>The specification defines these values: </p>
         * <ul>
         *     <li>RESUNIT_NONE</li>
         *     <li>RESUNIT_INCH</li>
         *     <li>RESUNIT_CENTIMETER</li>
         * </ul>
         * <p>Default value is {@link org.beyka.tiffbitmapfactory.ResolutionUnit#NONE}</p>
         */
        public ResolutionUnit resUnit;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * A string that describes the subject of the image.
         * <p>This parameter is link to TIFFTAG_IMAGEDESCRIPTION tag</p>
         */
        public String imageDescription;

        /**
         * <b>For converting from ANY to TIFF cases</b>
         * Software that used for creating of image.
         * <p>This parameter is link to TIFFTAG_SOFTWARE tag</p>
         */
        public String software;

    }
}
