package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 4/16/17.
 */

public enum FillOrder {

    /**
     * Pixels with lower column values are stored in the higher-order bits of the byte.
     */
    MSB2LSB(1),

    /**
     * Pixels with lower column values are stored in the lower-order bits of the byte.
     */
    LSB2MSB(2);

    final int ordinal;

    FillOrder(int ordinal) {
        this.ordinal = ordinal;
    }
}
