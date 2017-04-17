package org.beyka.tiffbitmapfactory;

/**
 * Created by beyka on 4/16/17.
 */

public enum Photometric {

    /**
     * WhiteIsZero. For bilevel and grayscale images: 0 is imaged as white.
     */
    MINISWHITE(0),

    /**
     * BlackIsZero. For bilevel and grayscale images: 0 is imaged as black.
     */
    MINISBLACK(1),

    /**
     * RGB value of (0,0,0) represents black, and (255,255,255) represents white, assuming 8-bit components. The components are stored in the indicated order: first Red, then Green, then Blue.
     */
    RGB(2),

    /**
     * Palette color. In this model, a color is described with a single component. The value of the component is used as an index into the red, green and blue curves in the ColorMap field to retrieve an RGB triplet that defines the color. When PhotometricInterpretation=3 is used, ColorMap must be present and SamplesPerPixel must be 1.
     */
    PALETTE(3),

    /**
     * Transparency Mask. This means that the image is used to define an irregularly shaped region of another image in the same TIFF file. SamplesPerPixel and BitsPerSample must be 1. PackBits compression is recommended. The 1-bits define the interior of the region; the 0-bits define the exterior of the region.
     */
    MASK(4),

    /**
     * Seperated, usually CMYK.
     */
    SEPARATED(5),

    /**
     * YCbCr
     */
    YCBCR(6),

    /**
     * CIE L*a*b*
     */
    CIELAB(8),

    /**
     * CIE L*a*b*, alternate encoding also known as ICC L*a*b*
     */
    ICCLAB(9),

    /**
     * CIE L*a*b*, alternate encoding also known as ITU L*a*b*, defined in ITU-T Rec. T.42, used in the TIFF-F and TIFF-FX standard (RFC 2301). The Decode tag, if present, holds information about this particular CIE L*a*b* encoding.
     */
    ITULAB(10),

    /**
     * LOGL
     */
    LOGL(32844),

    /**
     * LOGLUV
     */
    LOGLUV(32845),

    /**
     * Some unknown photometric
     */
    OTHER(-1);

    final int ordinal;

    Photometric(int ordinal) {
        this.ordinal = ordinal;
    }
}
