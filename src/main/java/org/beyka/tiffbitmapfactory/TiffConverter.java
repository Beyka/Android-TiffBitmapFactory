package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;

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
     * Convert any of supported formats from {@link ImageFormat} to tiff
     * @param in path to income tiff file
     * @param out path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     *
     * @return true if convert process have been successful
     */
    public static boolean convertToTiff(File in, File out, ConverterOptions options, IProgressListener listener) {
        switch (getImageType(in.getAbsolutePath())) {
            case JPEG:
                return convertJpgTiff(in.getAbsolutePath(), out.getAbsolutePath(), options, listener);
            case PNG:
                return convertPngTiff(in.getAbsolutePath(), out.getAbsolutePath(), options, listener);
            case TIFF:
                // TODO: 9/19/17 make convert tiff to tiff method
                break;
        }
        return false;
    }



    /**
     * Convert tiff to png file. Uses direct data read method, that decrease memory usage
     * @param tiff path to income tiff file
     * @param png path to outcome png file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static native boolean convertTiffPng(String tiff, String png, ConverterOptions options, IProgressListener listener);

    /**
     * Convert png to tiff file. Uses direct data read method, that decrease memory usage.
     * @param png path to income png file
     * @param tiff path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static native boolean convertPngTiff(String png, String tiff, ConverterOptions options, IProgressListener listener);

    /**
     * Convert tiff to jpeg file. Uses direct data read method, that decrease memory usage
     * @param tiff path to income tiff file
     * @param jpg path to outcome jpeg file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static native boolean convertTiffJpg(String tiff, String jpg, ConverterOptions options, IProgressListener listener);

    /**
     * Convert jpeg to tiff file. Uses direct data read method, that decrease memory usage.
     * @param jpg path to income jpeg file
     * @param tiff path to outcome tiff file
     * @param options converter options
     * @param listener listener which will receive converting progress
     * @return true if convert process have been successful
     */
    public static native boolean convertJpgTiff(String jpg, String tiff, ConverterOptions options, IProgressListener listener);

    public static native boolean convertBmpTiff(String bmp, String tiff, ConverterOptions options, IProgressListener listener);

    public static native Bitmap readBmp(String tiff, String bmp, ConverterOptions options, IProgressListener listener);

    /**
     * Return type of file.
     * @param path - file path
     * @return
     */
    public static native ImageFormat getImageType(String path);

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
         */
        private volatile boolean isStoped;

        /**
         * Stop native convert thread
         * If converting is started in any thread except main, calling of this method will cause force stop of converting and returning of false.
         */
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
