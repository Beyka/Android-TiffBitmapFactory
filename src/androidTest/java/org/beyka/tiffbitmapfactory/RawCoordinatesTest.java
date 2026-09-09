package org.beyka.tiffbitmapfactory;

import android.graphics.Bitmap;
import android.os.ParcelFileDescriptor;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import java.io.*;
import static org.junit.Assert.*;
import org.beyka.tiffbitmapfactory.exceptions.NotEnoughtMemoryException;
import org.beyka.tiffbitmapfactory.exceptions.DecodeTiffException;

public class RawCoordinatesTest {
    private File fixture(String name) throws IOException {
        File file = new File(InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir(), name);
        try (InputStream in = InstrumentationRegistry.getInstrumentation().getContext().getAssets().open("raw/" + name);
             OutputStream out = new FileOutputStream(file)) {
            byte[] bytes = new byte[8192]; int n;
            while ((n = in.read(bytes)) >= 0) out.write(bytes, 0, n);
        }
        return file;
    }
    private TiffBitmapFactory.Options options() {
        TiffBitmapFactory.Options o = new TiffBitmapFactory.Options();
        o.inUseRawCoordinates = true; o.inThrowException = true;
        o.inAvailableMemory = 16 * 1024 * 1024;
        return o;
    }
    @Test public void rawRegionsMatchStoredPixelsForEveryOrientationAndSample() throws Exception {
        for (String kind : new String[]{"tile", "strip"}) for (int orientation = 1; orientation <= 8; orientation++) {
            try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(fixture(kind + "-" + orientation + ".tif"), ParcelFileDescriptor.MODE_READ_ONLY)) {
                for (int sample : new int[]{1, 2, 3, 4, 5, 6, 7, 8}) for (DecodeArea area : new DecodeArea[]{
                        new DecodeArea(0,0,37,29), new DecodeArea(5,3,32,26), new DecodeArea(36,28,1,1)}) {
                    TiffBitmapFactory.Options o = options(); o.inSampleSize = sample; o.inDecodeArea = area;
                    // Raw mode deliberately leaves orientation to the caller.
                    o.inUseOrientationTag = true;
                    Bitmap b = TiffBitmapFactory.decodeFileDescriptor(fd.getFd(), o);
                    assertNotNull(b);
                    assertEquals((area.width + sample - 1) / sample, b.getWidth());
                    assertEquals((area.height + sample - 1) / sample, b.getHeight());
                    for (int y=0; y<b.getHeight(); y++) for (int x=0; x<b.getWidth(); x++) {
                        int sx=area.x+x*sample, sy=area.y+y*sample;
                        int expected=0xff000000 | ((sx*7)%256)<<16 | ((sy*9)%256)<<8 | ((sx+sy*3)%256);
                        assertEquals(kind + " orientation=" + orientation + " sample=" + sample + " at="+sx+","+sy, expected,b.getPixel(x,y));
                    }
                    b.recycle();
                }
                assertTrue(fd.getFileDescriptor().valid());
            }
        }
    }
    @Test public void rawSamplingLargerThanTileSupportsTiledAndStrippedImages() throws Exception {
        for (String kind : new String[]{"tile", "strip"}) {
            File file = fixture(kind + "-1.tif"); // Tiled variant uses 16x16 tiles.
            for (DecodeArea area : new DecodeArea[]{
                    new DecodeArea(0, 0, 37, 29),
                    new DecodeArea(5, 3, 32, 26),
                    new DecodeArea(17, 1, 20, 28)}) {
                for (int sample : new int[]{17, 31, 32, 63, 64}) {
                    TiffBitmapFactory.Options o = options();
                    o.inSampleSize = sample;
                    o.inDecodeArea = area;
                    Bitmap b = TiffBitmapFactory.decodeFile(file, o);
                    assertNotNull("sample=" + sample, b);
                    assertEquals((area.width + sample - 1) / sample, b.getWidth());
                    assertEquals((area.height + sample - 1) / sample, b.getHeight());
                    for (int by = 0; by < b.getHeight(); by++) {
                        for (int bx = 0; bx < b.getWidth(); bx++) {
                            int sx = area.x + bx * sample;
                            int sy = area.y + by * sample;
                            int expected = 0xff000000 | ((sx * 7) % 256) << 16
                                    | ((sy * 9) % 256) << 8 | ((sx + sy * 3) % 256);
                            assertEquals("kind=" + kind + " area=" + area.x + "," + area.y
                                    + " sample=" + sample + " at=" + sx + "," + sy,
                                    expected, b.getPixel(bx, by));
                        }
                    }
                    b.recycle();
                }
            }
        }
    }
    @Test public void legacySamplingSupportsTiledAndStrippedImagesWithEitherFilter() throws Exception {
        for (String kind : new String[]{"tile", "strip"}) {
            File file = fixture(kind + "-1.tif");
            for (boolean singlePixel : new boolean[]{false, true}) {
                for (int sample : new int[]{1, 2, 3, 4, 5, 6, 7, 8}) {
                    TiffBitmapFactory.Options o = new TiffBitmapFactory.Options();
                    o.inThrowException = true;
                    o.inSampleSize = sample;
                    o.inUseSinglePixelSample = singlePixel;
                    Bitmap b = TiffBitmapFactory.decodeFile(file, o);
                    assertNotNull("kind=" + kind + " sample=" + sample
                            + " singlePixel=" + singlePixel, b);
                    assertEquals(37 / sample, b.getWidth());
                    assertEquals(29 / sample, b.getHeight());
                    if (singlePixel) {
                        for (int by = 0; by < b.getHeight(); by++) {
                            for (int bx = 0; bx < b.getWidth(); bx++) {
                                int sx = bx * sample;
                                int sy = by * sample;
                                int expected = 0xff000000 | ((sx * 7) % 256) << 16
                                        | ((sy * 9) % 256) << 8 | ((sx + sy * 3) % 256);
                                assertEquals("kind=" + kind + " sample=" + sample
                                                + " at=" + sx + "," + sy,
                                        expected, b.getPixel(bx, by));
                            }
                        }
                    }
                    b.recycle();
                }
            }
        }
    }
    @Test public void nonPositiveSampleSizeIsRejectedByBothDecoders() throws Exception {
        File file = fixture("strip-1.tif");
        for (boolean rawCoordinates : new boolean[]{false, true}) {
            for (int sample : new int[]{0, -1, -2}) {
                TiffBitmapFactory.Options o = new TiffBitmapFactory.Options();
                o.inUseRawCoordinates = rawCoordinates;
                o.inSampleSize = sample;
                assertNull("rawCoordinates=" + rawCoordinates + " sample=" + sample,
                        TiffBitmapFactory.decodeFile(file, o));
            }
        }
    }
    @Test public void pageCountAndMetadataRemainCorrectOnEveryPage() throws Exception {
        try (ParcelFileDescriptor fd = ParcelFileDescriptor.open(fixture("pages.tif"), ParcelFileDescriptor.MODE_READ_ONLY)) {
            for(int page=0;page<3;page++) {
                TiffBitmapFactory.Options o=options(); o.inDirectoryNumber=page; o.inJustDecodeBounds=true;
                assertNull(TiffBitmapFactory.decodeFileDescriptor(fd.getFd(),o));
                assertEquals(3,o.outDirectoryCount); assertEquals(37,o.outWidth); assertEquals(29,o.outHeight);
                o.inJustDecodeBounds=false;
                Bitmap b=TiffBitmapFactory.decodeFileDescriptor(fd.getFd(),o);
                assertEquals(0xff000000 | (page*11)<<16,b.getPixel(0,0)); b.recycle();
            }
            TiffBitmapFactory.Options invalid=options(); invalid.inDirectoryNumber=3;
            assertThrows(DecodeTiffException.class,()->TiffBitmapFactory.decodeFileDescriptor(fd.getFd(),invalid));
        }
    }
    @Test public void singlePixelAndMemoryRejection() throws Exception {
        Bitmap one=TiffBitmapFactory.decodeFile(fixture("pixel.tif"),options());
        assertEquals(1,one.getWidth()); assertEquals(1,one.getHeight()); one.recycle();
        TiffBitmapFactory.Options o=options(); o.inAvailableMemory=16;
        File file=fixture("strip-1.tif");
        assertThrows(NotEnoughtMemoryException.class,()->TiffBitmapFactory.decodeFile(file,o));
    }
    @Test public void ccittStreamingMatchesLegacyFullResolution() throws Exception {
        File file=fixture("fax2d.tif");
        TiffBitmapFactory.Options legacy=new TiffBitmapFactory.Options();
        legacy.inThrowException=true;
        Bitmap expected=TiffBitmapFactory.decodeFile(file,legacy);
        Bitmap actual=TiffBitmapFactory.decodeFile(file,options());
        assertNotNull(expected); assertNotNull(actual);
        assertEquals(expected.getWidth(),actual.getWidth());
        assertEquals(expected.getHeight(),actual.getHeight());
        assertTrue(expected.sameAs(actual));
        expected.recycle(); actual.recycle();
    }
    @Test public void cancelledDecodeReturnsNullAndNextRequestWorks() throws Exception {
        File file=fixture("strip-1.tif"); TiffBitmapFactory.Options o=options();
        assertNull(TiffBitmapFactory.decodeFile(file,o,(current,total)->o.stop()));
        Bitmap b=TiffBitmapFactory.decodeFile(file,options()); assertNotNull(b); b.recycle();
    }
    @Test public void malformedFileThrowsAndDescriptorStaysOwnedByCaller() throws Exception {
        File bad=new File(InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir(),"invalid.tif");
        try (OutputStream out=new FileOutputStream(bad)) { out.write(new byte[]{1,2,3,4}); }
        try (ParcelFileDescriptor fd=ParcelFileDescriptor.open(bad,ParcelFileDescriptor.MODE_READ_ONLY)) {
            assertThrows(org.beyka.tiffbitmapfactory.exceptions.CantOpenFileException.class,
                    ()->TiffBitmapFactory.decodeFileDescriptor(fd.getFd(),options()));
            assertTrue(fd.getFileDescriptor().valid());
        }
    }
}
