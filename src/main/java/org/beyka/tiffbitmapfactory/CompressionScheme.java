package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 5.1.17.
 */

public enum CompressionScheme {
    /**
     * No compression
     */
    COMPRESSION_NONE(1),
    /**
     * CCITT Group 3 fax encoding
     */
    COMPRESSION_CCITTFAX3(3),
    /**
     * CCITT Group 4 fax encoding
     */
    COMPRESSION_CCITTFAX4(4),
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
    COMPRESSION_ADOBE_DEFLATE(8),
    /**
     * All other compression schemes
     */
    OTHER(0);

    final int ordinal;

    CompressionScheme(int ordinal) {
        this.ordinal = ordinal;
    }
}
