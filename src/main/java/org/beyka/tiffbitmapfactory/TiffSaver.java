package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;

import org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException;
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(File destination, Bitmap bmp) throws NoSuchFileException, NotEnoughtMemoryException {
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp, SaveOptions options) throws NoSuchFileException {
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(File destination, Bitmap bmp) throws NoSuchFileException, NotEnoughtMemoryException {
        return appendBitmap(destination.getAbsolutePath(), bmp, new SaveOptions());
    }

    /**
     * append bitmap to the end of existing file or create new file.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(File destination, Bitmap bmp, SaveOptions options) throws NoSuchFileException {
        return appendBitmap(destination.getAbsolutePath(), bmp, options);
    }

    /**
     * append bitmap to the end of existing file or create new file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(String destinationPath, int page, Bitmap bmp) throws NoSuchFileException {
        return appendBitmap(destinationPath, bmp, new SaveOptions());
    }

    /**
     * append bitmap to the end of existing file or create new file.
     *
     * @param destinationPath - file path to write bitmap
     * @param bmp             - Bitmap for saving
     * @param options         - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws org.beyka.tiffbitmapfactory.exceptions.NoSuchFileException when {@code destinationPath} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     */
    public static boolean appendBitmap(String destinationPath, Bitmap bmp,SaveOptions options) throws NoSuchFileException {
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
         */
        public CompressionScheme compressionScheme;

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

        /**
         * A string that describes the subject of the image.
         */
        public String imageDescription;
    }
}
