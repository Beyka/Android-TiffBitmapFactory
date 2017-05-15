package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 5/9/17.
 */

public class TiffConverter {
    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tiffconverter");
    }

//    public static boolean convertTiffPng(String in, String out) {
//
//    }

    public static native boolean convertTiffPng(String tiff, String png, ConverterOptions options);

    public static native boolean convertPngTiff(String png, String tiff, ConverterOptions options);

    public static native boolean convertTiffJpg(String tiff, String jpg, ConverterOptions options);

    public static native boolean convertJpgTiff(String jpg, String tiff, ConverterOptions options);

    public static final class ConverterOptions {

        public ConverterOptions() {
            availableMemory = 8000*8000*4;
            appendTiff = false;
            resUnit = ResolutionUnit.NONE;
            compressionScheme = CompressionScheme.LZW;
            throwExceptions = false;
        }

        public int tiffDirectoryRead;
        public long availableMemory;
        public boolean throwExceptions;
        public boolean appendTiff;
        public CompressionScheme compressionScheme;

        /**
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffSaver.SaveOptions#resUnit} in the ImageWidth direction.
         * <p> It is not mandatory that the image be actually displayed or printed at the size implied by this parameter.
         * It is up to the application to use this information as it wishes.</p>
         * <p>Defualt value is 0</p>
         */
        public float xResolution;

        /**
         * The number of pixels per {@link org.beyka.tiffbitmapfactory.TiffSaver.SaveOptions#resUnit} in the ImageHeight direction.
         * <p> It is not mandatory that the image be actually displayed or printed at the size implied by this parameter.
         * It is up to the application to use this information as it wishes.</p>
         * <p>Defualt value is 0</p>
         */
        public float yResolution;

        /**
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

    }
}
