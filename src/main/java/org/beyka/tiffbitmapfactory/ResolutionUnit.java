package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 13.3.17.
 */

public enum ResolutionUnit {
    NONE(1),
    INCH(2),
    CENTIMETER(3);

    final int ordinal;

    ResolutionUnit(int ordinal) {
        this.ordinal = ordinal;
    }
}
