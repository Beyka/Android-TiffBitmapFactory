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

    public static native boolean convertTiffPng(String tiff, String png);

    public static native boolean convertPngTiff(String png, String tiff);

    public static native boolean convertPngTiff(String[] png, String tiffs);
}
