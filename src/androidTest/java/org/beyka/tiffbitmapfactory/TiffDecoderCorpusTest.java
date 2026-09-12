package org.beyka.tiffbitmapfactory;

import android.app.Instrumentation;
import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.os.Debug;
import android.os.SystemClock;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.CRC32;

@RunWith(AndroidJUnit4.class)
public class

TiffDecoderCorpusTest {
    private static final int METADATA_WARM_RUNS = 7;

    private final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
    private final Context targetContext = instrumentation.getTargetContext();

    @Test
    public void decodesCcittWithScanlineSizedWorkingMemory() throws IOException {
        File file = copyToCache("raw/fax2d.tif");
        int[] bounds = readBounds(file);

        TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
        options.inThrowException = true;
        options.inUseOrientationTag = true;
        options.inAvailableMemory = (bounds[0] + 7) / 8 + 4096;

        Bitmap bitmap = TiffBitmapFactory.decodeFile(file, options);
        assertNotNull(bitmap);
        assertEquals(bounds[0], bitmap.getWidth());
        assertEquals(bounds[1], bitmap.getHeight());
        bitmap.recycle();
    }

    @Test
    public void decodesPreferredConfigsFromBilevel() throws IOException {
        File file = copyToCache("raw/fax2d.tif");
        int[] bounds = readBounds(file);

        TiffBitmapFactory.Options alphaOptions = new TiffBitmapFactory.Options();
        alphaOptions.inThrowException = true;
        alphaOptions.inPreferredConfig = TiffBitmapFactory.ImageConfig.ALPHA_8;
        alphaOptions.inAvailableMemory = (bounds[0] + 7) / 8 + 4096;
        Bitmap alpha = TiffBitmapFactory.decodeFile(file, alphaOptions);
        assertNotNull(alpha);
        assertEquals(Bitmap.Config.ALPHA_8, alpha.getConfig());
        assertEquals(bounds[0], alpha.getWidth());
        assertEquals(bounds[1], alpha.getHeight());
        assertBilevelAlphaUsesInkMask(alpha);
        alpha.recycle();

        TiffBitmapFactory.Options rgbOptions = new TiffBitmapFactory.Options();
        rgbOptions.inThrowException = true;
        rgbOptions.inPreferredConfig = TiffBitmapFactory.ImageConfig.RGB_565;
        rgbOptions.inAvailableMemory = (bounds[0] + 7) / 8 + 4096;
        Bitmap rgb = TiffBitmapFactory.decodeFile(file, rgbOptions);
        assertNotNull(rgb);
        assertEquals(Bitmap.Config.RGB_565, rgb.getConfig());
        assertEquals(bounds[0], rgb.getWidth());
        assertEquals(bounds[1], rgb.getHeight());
        rgb.recycle();
    }

    @Test
    public void benchmarkCorpus() throws IOException {
        List<String> assets = collectTiffs("");
        if (assets.isEmpty()) {
            throw new AssertionError("No TIFF assets found");
        }

        for (String name : assets) {
            File file = copyToCache(name);
            BoundsMeasurement boundsMeasurement;
            try {
                boundsMeasurement = measureBounds(file);
                printMetadataBenchmark(name, file, boundsMeasurement);
            } catch (Throwable error) {
                printError(name, "phase=bounds", error);
                continue;
            }
            int[] bounds = boundsMeasurement.bounds;

            int[] samples = name.equalsIgnoreCase("HUGE.tiff")
                    ? new int[]{4, 5, 6, 7, 8}
                    : new int[]{1, 2, 3, 4, 5, 6, 7, 8};
            for (int sample : samples) {
                runCase(name, file, bounds, sample, null);
            }

            if (bounds[0] >= 8 && bounds[1] >= 8) {
                int width = Math.min(bounds[0] / 2, 1024);
                int height = Math.min(bounds[1] / 2, 1024);
                DecodeArea crop = new DecodeArea(
                        (bounds[0] - width) / 2,
                        (bounds[1] - height) / 2,
                        width,
                        height
                );
                runCase(name, file, bounds, 1, crop);
                runCase(name, file, bounds, 4, crop);
            }
        }
    }

    private void runCase(String name, File file, int[] source, int sample, DecodeArea crop) {
        String cropLabel = crop == null
                ? "full"
                : crop.x + "," + crop.y + "," + crop.width + "," + crop.height;
        try {
            TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
            options.inThrowException = true;
            options.inUseOrientationTag = true;
            options.inSampleSize = sample;
            options.inDecodeArea = crop;
            options.inAvailableMemory = -1;

            final int[] progressCalls = {0};
            long heapBefore = Debug.getNativeHeapAllocatedSize();
            long started = SystemClock.elapsedRealtimeNanos();
            Bitmap bitmap = TiffBitmapFactory.decodeFile(file, options,
                    (processed, total) -> progressCalls[0]++);
            if (bitmap == null) {
                throw new AssertionError("decoder returned null");
            }
            long elapsed = SystemClock.elapsedRealtimeNanos() - started;
            long heapDelta = Debug.getNativeHeapAllocatedSize() - heapBefore;

            System.out.println("TIFF_BENCH status=ok phase=decode file=" + name
                    + " source=" + source[0] + "x" + source[1]
                    + " sample=" + sample + " crop=" + cropLabel
                    + " output=" + bitmap.getWidth() + "x" + bitmap.getHeight()
                    + " elapsedNs=" + elapsed + " nativeHeapDelta=" + heapDelta
                    + " progressCalls=" + progressCalls[0]
                    + " crc32=" + checksum(bitmap));
            bitmap.recycle();
        } catch (Throwable error) {
            printError(name, "source=" + source[0] + "x" + source[1]
                    + " sample=" + sample + " crop=" + cropLabel, error);
        }
    }

    private BoundsMeasurement measureBounds(File file) {
        long started = SystemClock.elapsedRealtimeNanos();
        int[] bounds = readBounds(file);
        return new BoundsMeasurement(bounds,
                SystemClock.elapsedRealtimeNanos() - started);
    }

    private void printMetadataBenchmark(String name, File file,
                                        BoundsMeasurement firstMeasurement) {
        long[] warmElapsed = new long[METADATA_WARM_RUNS];
        for (int index = 0; index < warmElapsed.length; index++) {
            BoundsMeasurement measurement = measureBounds(file);
            assertEquals(firstMeasurement.bounds[0], measurement.bounds[0]);
            assertEquals(firstMeasurement.bounds[1], measurement.bounds[1]);
            warmElapsed[index] = measurement.elapsedNs;
        }
        Arrays.sort(warmElapsed);

        long total = 0;
        for (long elapsed : warmElapsed) {
            total += elapsed;
        }
        System.out.println("TIFF_BENCH status=ok phase=open_metadata_close file=" + name
                + " source=" + firstMeasurement.bounds[0] + "x"
                + firstMeasurement.bounds[1]
                + " fileBytes=" + file.length()
                + " firstNs=" + firstMeasurement.elapsedNs
                + " warmMinNs=" + warmElapsed[0]
                + " warmMedianNs=" + warmElapsed[warmElapsed.length / 2]
                + " warmMeanNs=" + total / warmElapsed.length
                + " warmMaxNs=" + warmElapsed[warmElapsed.length - 1]
                + " warmRuns=" + warmElapsed.length);
    }

    private void assertBilevelAlphaUsesInkMask(Bitmap bitmap) {
        boolean sawInk = false;
        boolean sawPaper = false;
        int[] row = new int[bitmap.getWidth()];
        for (int y = 0; y < bitmap.getHeight() && !(sawInk && sawPaper); y++) {
            bitmap.getPixels(row, 0, bitmap.getWidth(), 0, y, bitmap.getWidth(), 1);
            for (int pixel : row) {
                int alpha = (pixel >>> 24) & 0xFF;
                if (alpha == 0xFF) {
                    sawInk = true;
                } else if (alpha == 0) {
                    sawPaper = true;
                }
            }
        }
        assertTrue("expected both ink and paper in bilevel ALPHA_8", sawInk && sawPaper);
    }

    private int[] readBounds(File file) {
        TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
        options.inJustDecodeBounds = true;
        options.inThrowException = true;
        TiffBitmapFactory.decodeFile(file, options);
        if (options.outWidth <= 0 || options.outHeight <= 0) {
            throw new AssertionError("Invalid bounds for " + file.getName() + ": "
                    + options.outWidth + "x" + options.outHeight);
        }
        return new int[]{options.outWidth, options.outHeight};
    }

    private static final class BoundsMeasurement {
        private final int[] bounds;
        private final long elapsedNs;

        private BoundsMeasurement(int[] bounds, long elapsedNs) {
            this.bounds = bounds;
            this.elapsedNs = elapsedNs;
        }
    }

    private String checksum(Bitmap bitmap) {
        CRC32 crc = new CRC32();
        int[] row = new int[bitmap.getWidth()];
        byte[] bytes = new byte[bitmap.getWidth() * 4];
        for (int y = 0; y < bitmap.getHeight(); y++) {
            bitmap.getPixels(row, 0, bitmap.getWidth(), 0, y, bitmap.getWidth(), 1);
            int offset = 0;
            for (int pixel : row) {
                bytes[offset++] = (byte) (pixel >>> 24);
                bytes[offset++] = (byte) (pixel >>> 16);
                bytes[offset++] = (byte) (pixel >>> 8);
                bytes[offset++] = (byte) pixel;
            }
            crc.update(bytes);
        }
        return Long.toHexString(crc.getValue());
    }

    private File copyToCache(String name) throws IOException {
        File output = new File(targetContext.getCacheDir(), "tiff-corpus/" + name);
        File parent = output.getParentFile();
        if (parent != null && !parent.mkdirs() && !parent.isDirectory()) {
            throw new IOException("Cannot create " + parent);
        }
        try (InputStream input = instrumentation.getContext().getAssets().open(name);
             FileOutputStream outputStream = new FileOutputStream(output)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                outputStream.write(buffer, 0, count);
            }
        }
        return output;
    }

    private List<String> collectTiffs(String path) throws IOException {
        AssetManager assets = instrumentation.getContext().getAssets();
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            String lower = path.toLowerCase(Locale.US);
            if (lower.endsWith(".tif") || lower.endsWith(".tiff")) {
                return Collections.singletonList(path);
            }
            return Collections.emptyList();
        }

        Arrays.sort(children);
        List<String> result = new ArrayList<>();
        for (String child : children) {
            result.addAll(collectTiffs(path.isEmpty() ? child : path + "/" + child));
        }
        return result;
    }

    private void printError(String name, String context, Throwable error) {
        String message = error.getMessage() == null
                ? "null"
                : error.getMessage().replace(' ', '_');
        System.out.println("TIFF_BENCH status=error file=" + name + " " + context
                + " error=" + error.getClass().getSimpleName() + " message=" + message);
    }
}
