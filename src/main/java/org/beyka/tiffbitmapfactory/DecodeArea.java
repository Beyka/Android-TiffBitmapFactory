package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 10/24/17.
 */

/**
 * Holder for points of decode area
 */
public class DecodeArea {
    public int x;
    public int y;
    public int width;
    public int height;

    public DecodeArea(){}

    public DecodeArea(int x, int y, int width, int height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    }
}
