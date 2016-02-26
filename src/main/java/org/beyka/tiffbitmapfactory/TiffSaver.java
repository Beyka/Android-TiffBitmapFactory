package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.Log;

import org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException;

import java.io.File;

/**
 * Created by beyka on 18.2.16.
 */
public class TiffSaver {

    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tiffsaver");
    }

    public enum CompressionMode {
        /**
         * No compression
         */
        COMPRESSION_NONE(1),
        /**
         * LZW
         */
        COMPRESSION_LZW(5),
        /**
         * JPEG ('new-style' JPEG)
         */
        COMPRESSION_JPEG(7),
        COMPRESSION_PACKBITS(32773),
        COMPRESSION_DEFLATE(32946),
        COMPRESSION_ADOBE_DEFLATE(8);

        final int ordinal;

        CompressionMode(int ordinal) {
            this.ordinal = ordinal;
        }
    }

    public enum Orientation {
        ORIENTATION_TOPLEFT(1),
        ORIENTATION_TOPRIGHT(2),
        ORIENTATION_BOTRIGHT(3),
        ORIENTATION_BOTLEFT(4),
        ORIENTATION_LEFTTOP(5),
        ORIENTATION_RIGHTTOP(6),
        ORIENTATION_RIGHTBOT(7),
        ORIENTATION_LEFTBOT(8);

        final int ordinal;

        Orientation(int ordinal) {
            this.ordinal = ordinal;
        }
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destination} not exist or can't be opened for writing
     */
    public static boolean saveBitmap(File destination, Bitmap bmp) throws NoSuchFileException {
        return saveBitmap(destination.getAbsolutePath(), bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     */
    public static boolean saveBitmap(File destination, Bitmap bmp, SaveOptions options) throws NoSuchFileException {
        return saveBitmap(destination.getAbsolutePath(), bmp, options);
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp) throws NoSuchFileException {
        return saveBitmap(destinationPath, bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @param options         - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp, SaveOptions options) throws NoSuchFileException {
        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        return save(destinationPath, pixels, options, bmp.getWidth(), bmp.getHeight());
    }

    private static synchronized native boolean save(String filePath, int[] image, SaveOptions options, int width, int height);

    /**
     * Options class to specify saving parameters
     */
    public static final class SaveOptions {

        public SaveOptions() {
            compressionMode = CompressionMode.COMPRESSION_NONE;
            orientation = Orientation.ORIENTATION_TOPLEFT;
        }

        /**
         * Compression scheme used on the image data.
         * <p>Default value is {@link CompressionMode#COMPRESSION_NONE COMPRESSION_NONE}</p>
         */
        public CompressionMode compressionMode;

        /**
         * {@link org.beyka.tiffbitmapfactory.TiffSaver.Orientation Orientation} that will used for saving image
         * <p>By default uses {@link org.beyka.tiffbitmapfactory.TiffSaver.Orientation#ORIENTATION_TOPLEFT ORIENTATION_TOPLEFT} </p>
         */
        public Orientation orientation;

        /**
         * Author for writing to file
         */
        public String author;

        /**
         * Copyright for writing to file
         */
        public String copyright;
    }
}
