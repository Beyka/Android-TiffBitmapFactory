package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 4/16/17.
 */

public enum PlanarConfig {
    CONTIG(1),
    SEPARATE(2);

    final int ordinal;

    PlanarConfig(int ordinal) {
        this.ordinal = ordinal;
    }
}
