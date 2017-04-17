package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 5.1.17.
 */

public enum Orientation {
    TOP_LEFT(1),
    TOP_RIGHT(2),
    BOT_RIGHT(3),
    BOT_LEFT(4),
    LEFT_TOP(5),
    RIGHT_TOP(6),
    RIGHT_BOT(7),
    LEFT_BOT(8),
    UNAVAILABLE(0);

    final int ordinal;

    Orientation(int ordinal) {
        this.ordinal = ordinal;
    }
}
