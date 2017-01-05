package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 5.1.17.
 */

public enum Orientation {
    ORIENTATION_TOPLEFT(1),
    ORIENTATION_TOPRIGHT(2),
    ORIENTATION_BOTRIGHT(3),
    ORIENTATION_BOTLEFT(4),
    ORIENTATION_LEFTTOP(5),
    ORIENTATION_RIGHTTOP(6),
    ORIENTATION_RIGHTBOT(7),
    ORIENTATION_LEFTBOT(8),
    UNAVAILABLE(0);

    final int ordinal;

    Orientation(int ordinal) {
        this.ordinal = ordinal;
    }
}
