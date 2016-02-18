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

    public static void saveBitmap(File file, Bitmap bmp){

        int pixels[] = new int[bmp.getWidth() * bmp.getHeight()];
        bmp.getPixels(pixels, 0, bmp.getWidth(), 0, 0, bmp.getWidth(), bmp.getHeight());

        int col = pixels[bmp.getHeight()/2 * bmp.getWidth() + bmp.getWidth()/2];
        int r = Color.red(col);
        int g = Color.green(col);
        int b = Color.blue(col);
        int a = Color.alpha(col);
        Log.i("Native", r + " red");
        Log.i("Native", g + " green");
        Log.i("Native", b + " blue");
        Log.i("Native", a + " alpha");

        save(file.getAbsolutePath(), pixels, bmp.getWidth(), bmp.getHeight());
    }

    private static synchronized native void save(String filePath, int[] image, int width, int height);

}
