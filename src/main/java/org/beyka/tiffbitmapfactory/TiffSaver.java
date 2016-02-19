package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.util.Log;

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
        COMPRESSION_NONE (1),

        /**
         * CCITT modified Huffman RLE
         */
        COMPRESSION_CCITTRLE (2),

        /**
         * CCITT Group 3 fax encoding
         */
        COMPRESSION_CCITTFAX3 (3),

        /**
         * CCITT Group 4 fax encoding
         */
        COMPRESSION_CCITTFAX4 (4),

        /**
         * LZW
         */
        COMPRESSION_LZW (5),

        /**
         * JPEG ('old-style' JPEG, later overriden in Technote2)
         */
        COMPRESSION_OJPEG (6),

        /**
         * JPEG ('new-style' JPEG)
         */
        COMPRESSION_JPEG (7),

        COMPRESSION_NEXT (32766),
        COMPRESSION_CCITTRLEW (32771),

        /**
         * PackBits compression, aka Macintosh RLE
         */
        COMPRESSION_PACKBITS (32773),
        COMPRESSION_THUNDERSCAN (32809),
        COMPRESSION_IT8CTPAD (32895),
        COMPRESSION_IT8LW (32896),
        COMPRESSION_IT8MP (32897),
        COMPRESSION_IT8BL (32898),
        COMPRESSION_PIXARFILM (32908),
        COMPRESSION_PIXARLOG (32909),
        COMPRESSION_DEFLATE (32946),

        /**
         * Deflate ('Adobe-style')
         */
        COMPRESSION_ADOBE_DEFLATE (8),
        COMPRESSION_DCS (32947),
        COMPRESSION_JBIG (34661),
        COMPRESSION_SGILOG (34676),
        COMPRESSION_SGILOG24 (34677),
        COMPRESSION_JP2000 (34712);

        final int ordinal;
        CompressionMode(int ordinal) {
            this.ordinal = ordinal;
        }
    }

    public static void saveBitmap(File file, Bitmap bmp){
        saveBitmap(file, bmp, new SaveOptions());
    }

    public static void saveBitmap(File file, Bitmap bmp, SaveOptions options){
        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

//        int col = pixels[bmp.getHeight()/2 * bmp.getWidth() + bmp.getWidth()/2];
//        int r = Color.red(col);
//        int g = Color.green(col);
//        int b = Color.blue(col);
//        int a = Color.alpha(col);
//        Log.i("Native", r + " red");
//        Log.i("Native", g + " green");
//        Log.i("Native", b + " blue");
//        Log.i("Native", a + " alpha");

        save(file.getAbsolutePath(), pixels, options, bmp.getWidth(), bmp.getHeight());
    }

    private static synchronized native void save(String filePath, int[] image, SaveOptions options, int width, int height);

    public static final class SaveOptions {

        public SaveOptions() {
            compressionMode = CompressionMode.COMPRESSION_NONE;
        }

        /**
         * Compression scheme used on the image data.
         * <p>Default value is {@link CompressionMode#COMPRESSION_NONE}</p>
         */
        public CompressionMode compressionMode;
    }
}
