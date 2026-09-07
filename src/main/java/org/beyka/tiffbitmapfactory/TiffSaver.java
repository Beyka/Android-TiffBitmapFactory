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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean saveBitmap(String destinationPath, Bitmap bmp, SaveOptions options) throws CantOpenFileException {
//        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
//        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        return save(destinationPath, -1, bmp, options, false);
    }

    /**
     * Append bitmap to the end of existing file or create new file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param destination - file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
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
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean appendBitmap(String destinationPath, Bitmap bmp,SaveOptions options) throws CantOpenFileException {
//        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
//        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        return save(destinationPath, -1, bmp, options, true);
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param fileDescriptor - file descriptor that represent file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean saveBitmap(int fileDescriptor, Bitmap bmp) throws CantOpenFileException, NotEnoughtMemoryException {
        return saveBitmap(fileDescriptor, bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param fileDescriptor - file descriptor that represent file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean saveBitmap(int fileDescriptor, Bitmap bmp, SaveOptions options) throws CantOpenFileException, NotEnoughtMemoryException {
        return save(null, fileDescriptor, bmp, options, false);
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param fileDescriptor - file descriptor that represent file to write bitmap
     * @param bmp         - Bitmap for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean appendBitmap(int fileDescriptor, Bitmap bmp) throws CantOpenFileException, NotEnoughtMemoryException {
        return appendBitmap(fileDescriptor, bmp, new SaveOptions());
    }

    /**
     * Save bitmap to file with default {@link TiffSaver.SaveOptions options}.
     *
     * @param fileDescriptor - file descriptor that represent file to write bitmap
     * @param bmp         - Bitmap for saving
     * @param options     - options for saving
     * @return true if bitmap was saved successful or false otherwise
     * @throws CantOpenFileException when {@code destination} not exist or can't be opened for writing
     * @throws org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException when there is no avalable memory for processing bitmap
     * @throws org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException when error occure while saving image
     */
    public static boolean appendBitmap(int fileDescriptor, Bitmap bmp, SaveOptions options) throws CantOpenFileException, NotEnoughtMemoryException {
        return save(null, fileDescriptor, bmp, options, true);
    }

    private static synchronized native boolean save(String filePath, int fileDescriptor, Bitmap bmp, SaveOptions options, boolean append);

    /**
     * Close detached file descriptor
     * @param fd
     */
    public static native void closeFd(int fd);

    /**
     * Options class to specify saving parameters
     */
    public static final class SaveOptions {

        public SaveOptions() {
            inAvailableMemory = 8000*8000*4;
            inThrowException = false;
            compressionScheme = CompressionScheme.NONE;
            orientation = Orientation.TOP_LEFT;
            xResolution = 0;
            yResolution = 0;
            resUnit = ResolutionUnit.NONE;
        }

        /**
         * Number of bytes that may be allocated during the Tiff file operations.
         * <p>-1 means memory is unlimited.</p>
         * <p>Default value is 244Mb</p>
         */
        public long inAvailableMemory;

        /**
         * If set to true, saver will throw exceptions if some errors appeared while writing tiff.
         * Otherwise  saver will just return false.
         * Default value is false
         */
        public boolean inThrowException;

        /**
         * Compression scheme used on the image data.
         * <p>Default value is {@link CompressionScheme#NONE COMPRESSION_NONE}</p>
         * <p>This parameter is link to TIFFTAG_COMPRESSION tag</p>
         */
        public CompressionScheme compressionScheme;

        /**
         * {@link org.beyka.tiffbitmapfactory.Orientation Orientation} that will used for saving image
         * <p>By default uses {@link org.beyka.tiffbitmapfactory.Orientation#TOP_LEFT ORIENTATION_TOPLEFT} </p>
         * <p>This parameter is link to TIFFTAG_ORIENTATION tag</p>
         */
        public Orientation orientation;

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
