# Bounded raw-coordinate decoding

`Options.inUseRawCoordinates = true` opts into the new decoder in `RawDecoder.cpp`.
Default `false` keeps the existing public decoding path. Both pathname and file
 descriptor overloads support the option. No existing method, enum, or JNI export
was removed.

Contract:

- Coordinates are stored TIFF pixels, before orientation. Metadata width/height
  use the same coordinates. The orientation tag is reported but not applied.
- DecodeArea uses `[x, x + width) × [y, y + height)`. Positive sizes are required;
  right/bottom are clipped to the image. Negative origins are rejected.
- The last row/column and 1 × 1 regions are valid.
- Sample is a positive power of two. Output is ARGB_8888, nearest-pixel sampled,
  `ceil(width/sample) × ceil(height/sample)`; output pixel `(i,j)` comes from
  `(x+i*sample,y+j*sample)`. Draw each output pixel over `sample` source pixels,
  clipping the last cell at the image edge. `inPreferredConfig` and the legacy
  averaging setting do not change this explicit mode.
- CCITT bilevel uses scanlines. Other supported TIFFs use RGBA strip/tile reads.
  The decoder changes the orientation tag only on its own in-memory TIFF handle;
  the file is opened read-only and is never rewritten.
- Memory preflight includes output pixels, a working block, a conservative codec
  allowance and 1 MiB overhead. An enormous single strip can still exceed budget.
  A smaller ROI cannot reduce the codec's strip size. This is reported as a
  memory error before allocating the bitmap. This estimate is not a process RSS
  hard limit: libtiff/codec metadata, GPU and allocator overhead also exist.
- Cancellation is checked between scanlines, RGBA blocks and output rows.
  `Options.stop()` still works as a sticky per-request signal. A codec call
  already running must return before its resources can be released.
- A caller-owned FD is duplicated; the caller must close its own descriptor.
  Decode repositions the duplicate to offset zero (the underlying open-file
  offset is shared). Do not concurrently use that same open file description.

Other focused fixes: directory count now starts from page zero; two pre-existing
NDK debug compilation errors were fixed (logging macro and variable scope).
The old SIGSEGV/longjmp-based decoder remains unchanged otherwise. The new path
uses ordinary cleanup and does not install process-wide signal handlers.

From the containing BTiffViewer checkout:

```sh
./gradlew :tiffbitmapfactory:assembleDebug
ANDROID_SERIAL=emulator-5554 ./gradlew :tiffbitmapfactory:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.class=org.beyka.tiffbitmapfactory.RawCoordinatesTest
```

The opt-in coordinate test asserts pixels against generated RGB source patterns
for strips/tiles, eight orientations, samples 1/2/4/8, offset and edge ROIs;
it also checks pages, memory rejection, cancellation and CCITT parity. Generate
small pattern assets with `python3 scripts/create_raw_fixtures.py` in this repo.
The fax fixture is copied unchanged from the existing sample corpus.

`viewer.gradle` is the app integration build (AGP 9, external ndk-build).
The original standalone/publishing `build.gradle` is preserved. For upstream
standalone packaging, rebuild JNI using the repository's existing NDK workflow;
do not package stale `.so` files after changing Java/JNI.
