package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 13.3.17.
 */

public enum ResolutionUnit {
    RESUNIT_NONE(1),
    RESUNIT_INCH(2),
    RESUNIT_CENTIMETER(3);

    final int ordinal;

    ResolutionUnit(int ordinal) {
        this.ordinal = ordinal;
    }
}
