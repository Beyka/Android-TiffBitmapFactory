package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 5.1.17.
 */

public enum CompressionScheme {
    /**
     * No compression
     */
    NONE(1),
    /**
     * CCITT modified Huffman RLE
     */
    CCITTRLE(2),
    /**
     * CCITT Group 3 fax encoding
     */
    CCITTFAX3(3),
    /**
     * CCITT Group 4 fax encoding
     */
    CCITTFAX4(4),
    /**
     * LZW
     */
    LZW(5),
    /**
     * JPEG ('new-style' JPEG)
     */
    JPEG(7),
    PACKBITS(32773),
    DEFLATE(32946),
    ADOBE_DEFLATE(8),
    /**
     * All other compression schemes
     */
    OTHER(0);

    final int ordinal;

    CompressionScheme(int ordinal) {
        this.ordinal = ordinal;
    }
}
