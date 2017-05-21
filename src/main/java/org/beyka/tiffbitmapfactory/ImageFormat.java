package org.beyka.tiffbitmapfactory;

/**
 * Supported image formats
 */

public enum ImageFormat {
    UNKNOWN(0),
    JPEG(1),
    PNG(2),
    TIFF(4);

    final int ordinal;

    ImageFormat(int ordinal) {
        this.ordinal = ordinal;
    }
}
