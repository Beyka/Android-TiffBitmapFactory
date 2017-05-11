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

    public static native boolean convertPngTiff(String[] png, String tiffs, ConverterOptions options);

    public static native boolean convertTiffJpg(String tiff, String jpg, ConverterOptions options);

    public static final class ConverterOptions {

        public int tiffDirectoryRead;
        public long availableMemory;
        public boolean throwExceptions;

    }
}
