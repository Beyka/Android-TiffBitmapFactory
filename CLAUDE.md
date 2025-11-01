# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Android-TiffBitmapFactory is an Android library for decoding and encoding TIFF image files. The library wraps native C/C++ libraries (libtiff, libjpeg, libpng) through JNI to provide high-performance TIFF operations on Android devices.

**Key capabilities:**
- Decode TIFF files to Android Bitmaps
- Save Android Bitmaps to TIFF format
- Convert between TIFF and other formats (JPEG, PNG, BMP)
- Support for multi-page TIFF files (directories)
- Memory-efficient processing with configurable limits
- Progress tracking for long operations
- File descriptor support for Android Q+ scoped storage

**Minimum API Level:** 16
**Supported Architectures:** arm64-v8a, armeabi-v7a

## Build Commands

### Build Native Libraries

The native C/C++ libraries must be built with NDK before the Android library can be used:

```bash
ndk-build NDK_PROJECT_PATH=src/main
```

This builds:
- `libtiff.so` - Core TIFF library
- `libtifffactory.so` - TIFF decoding functionality
- `libtiffsaver.so` - TIFF encoding functionality
- `libtiffconverter.so` - Format conversion functionality

**Note:** Prebuilt static libraries for libpng and libjpeg are located in `src/main/jni/libs/$(TARGET_ARCH_ABI)/`

### Android Library Build

```bash
./gradlew build
```

This builds the Android library (AAR) with the native libraries.

## Architecture

### Java Layer (`src/main/java/org/beyka/tiffbitmapfactory/`)

The Java layer provides Android-friendly APIs:

- **TiffBitmapFactory** - Main entry point for decoding TIFF files to Bitmaps
  - Supports path-based (deprecated for Android Q+) and file descriptor-based decoding
  - Options class controls decoding behavior (sample size, directory selection, memory limits)
  - Returns metadata in Options output fields

- **TiffSaver** - Save Bitmaps as TIFF files
  - Supports creating new files or appending pages to existing TIFFs
  - SaveOptions configures compression, orientation, and metadata tags

- **TiffConverter** - Direct format conversion without Bitmap intermediary
  - Converts JPEG/PNG/BMP to TIFF and vice versa
  - More memory-efficient than decode-then-encode for large files

- **Enums** - Type-safe representation of TIFF metadata (CompressionScheme, Orientation, Photometric, etc.)

### Native Layer (`src/main/jni/`)

The JNI layer bridges Java to native C/C++ code:

- **NativeTiffBitmapFactory.cpp** - JNI entry points for decoding
  - Creates NativeDecoder instances to handle actual decoding

- **NativeDecoder.cpp** - Core decoding logic using libtiff

- **NativeTiffSaver.cpp** - JNI entry points for encoding

- **NativeTiffConverter.cpp** - JNI entry points for conversion
  - Delegates to format-specific converters (TiffToJpgConverter, PngToTiffConverter, etc.)

- **BaseTiffConverter.cpp** - Base class for converters
  - Provides common functionality and progress reporting

- **Format-specific converters** - Handle conversions between TIFF and other formats
  - TiffToJpgConverter, TiffToPngConverter, TiffToBmpConverter
  - JpgToTiffConverter, PngToTiffConverter, BmpToTiffConverter

### Build Configuration

- **Android.mk** - Defines native library build configuration
  - Builds 4 shared libraries: libtiff, libtifffactory, libtiffsaver, libtiffconverter
  - Links against prebuilt static libpng and libjpeg
  - **Important:** Uses `-Wl,-z,max-page-size=16384` linker flag for 16KB page size alignment (Android compliance requirement)

- **Application.mk** - NDK application configuration
  - Target platform: android-16
  - ABIs: arm64-v8a, armeabi-v7a
  - STL: c++_static

- **build.gradle** - Android library Gradle configuration
  - NDK version: 21.3.6528147
  - Native libraries are pre-built (jni.srcDirs = [])
  - Pre-built .so files expected in src/main/libs/

## Key Implementation Details

### Memory Management

The library manages native memory carefully to avoid OOM crashes:

- **inAvailableMemory** option (default: 244MB) limits memory usage per decoding operation
- Decoder estimates required memory before starting
- Throws NotEnoughtMemoryException if estimate exceeds limit
- Set to -1 for unlimited memory (risky on multi-threaded usage)

### Android Q+ File Access

Since Android Q enforces scoped storage:

- Path-based methods (decodeFile, decodePath) are deprecated
- **Use file descriptor methods** (decodeFileDescriptor) with Storage Access Framework
- Converter and Saver classes also have FD-based variants

### Multi-page TIFF Support

TIFF files can contain multiple images (directories):

1. Set `inJustDecodeBounds = true` to read metadata without decoding
2. Check `outDirectoryCount` to get number of pages
3. Set `inDirectoryNumber` to select which page to decode
4. Iterate through all directories if needed

### Thread Safety

- Decoding can be interrupted via `Thread.interrupt()` (legacy `Options.stop()` is deprecated)
- Each thread should manage its own memory limit via `inAvailableMemory`
- Progress callbacks are invoked on the calling thread

### 16KB Page Size Alignment

Recent commits added 16KB page size support for Android compliance:

- All native libraries build with `-Wl,-z,max-page-size=16384` linker flag
- This ensures compatibility with devices using 16KB memory pages
- **Critical:** Maintain this flag when modifying Android.mk

## Common Development Patterns

### Adding New TIFF Metadata Support

1. Add enum/constant in appropriate Java enum class
2. Update Options class with new input/output fields
3. Modify NativeDecoder to read TIFF tag and populate Java field
4. For encoding, update NativeTiffSaver to write tag from SaveOptions

### Adding New Compression Scheme

1. Add enum value to CompressionScheme.java
2. Ensure libtiff supports the codec (may need to modify libtiff build)
3. Test decoding and encoding with new scheme
4. Update SaveOptions validation if needed

### Modifying Native Build

- Changes to Android.mk require full rebuild: `ndk-build NDK_PROJECT_PATH=src/main clean && ndk-build NDK_PROJECT_PATH=src/main`
- Verify all ABIs build successfully (arm64-v8a, armeabi-v7a)
- Maintain 16KB page alignment flags
- Test on multiple Android versions (especially Q+ for scoped storage)

## Testing

The project includes basic instrumentation tests:

```bash
./gradlew connectedAndroidTest
```

Tests are in `src/androidTest/java/org/beyka/tiffbitmapfactory/`

## ProGuard Rules

When library users enable ProGuard, they must add:

```
-keep class org.beyka.tiffbitmapfactory.**{ *; }
```

This prevents JNI method names from being obfuscated.
