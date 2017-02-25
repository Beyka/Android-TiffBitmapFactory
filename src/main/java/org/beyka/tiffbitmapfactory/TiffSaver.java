package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;

import org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException;
import org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException;

import java.io.File;

/**
 * Created by beyka on 18.2.16.
 */
public class TiffSaver {

    static {
        System.loadLibrary("tiff");
        System.loadLibrary("tiffsaver");
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(File destination, Bitmap bmp) throws CantOpenFileException, NotEnoughtMemoryException {
        return saveBitmap(destination.getAbsolutePath(), bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(File destination, Bitmap bmp, SaveOptions options) throws CantOpenFileException {
        return saveBitmap(destination.getAbsolutePath(), bmp, options);
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp) throws CantOpenFileException {
        return saveBitmap(destinationPath, bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @param options         - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp, SaveOptions options) throws CantOpenFileException {
        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        return save(destinationPath, pixels, options, bmp.getWidth(), bmp.getHeight(), false);
    }

    /**
     * Append bitmap to the end of existing file or create new file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(File destination, Bitmap bmp) throws CantOpenFileException, NotEnoughtMemoryException {
        return appendBitmap(destination.getAbsolutePath(), bmp, new SaveOptions());
    }

    /**
     * append bitmap to the end of existing file or create new file.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(File destination, Bitmap bmp, SaveOptions options) throws CantOpenFileException {
        return appendBitmap(destination.getAbsolutePath(), bmp, options);
    }

    /**
     * append bitmap to the end of existing file or create new file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(String destinationPath, int page, Bitmap bmp) throws CantOpenFileException {
        return appendBitmap(destinationPath, bmp, new SaveOptions());
    }

    /**
     * append bitmap to the end of existing file or create new file.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @param options         - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(String destinationPath, Bitmap bmp,SaveOptions options) throws CantOpenFileException {
        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        return save(destinationPath, pixels, options, bmp.getWidth(), bmp.getHeight(), true);
    }

    private static synchronized native boolean save(String filePath, int[] image, SaveOptions options, int width, int height, boolean append);


    /**
     * Options class to specify saving parameters
     */
    public static final class SaveOptions {

        public SaveOptions() {
            compressionScheme = CompressionScheme.COMPRESSION_NONE;
            orientation = Orientation.ORIENTATION_TOPLEFT;
        }

        /**
         * Compression scheme used on the image data.
         * <p>Default value is {@link CompressionScheme#COMPRESSION_NONE COMPRESSION_NONE}</p>
         * <p>This parameter is link to TIFFTAG_COMPRESSION tag</p>
         */
        public CompressionScheme compressionScheme;

        /**
         * {@link org.beyka.tiffbitmapfactory.Orientation Orientation} that will used for saving image
         * <p>By default uses {@link org.beyka.tiffbitmapfactory.Orientation#ORIENTATION_TOPLEFT ORIENTATION_TOPLEFT} </p>
         * <p>This parameter is link to TIFFTAG_ORIENTATION tag</p>
         */
        public Orientation orientation;

        /**
         * Author for writing to file.
         * <p>This parameter is link to TIFFTAG_ARTIST tag</p>
         */
        public String author;

        /**
         * Copyright for writing to file
         * <p>This parameter is link to TIFFTAG_COPYRIGHT tag</p>
         */
        public String copyright;

        /**
         * A string that describes the subject of the image.
         * <p>This parameter is link to TIFFTAG_IMAGEDESCRIPTION tag</p>
         */
        public String imageDescription;
    }
}
