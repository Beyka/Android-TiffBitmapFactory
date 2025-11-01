# AAR Build Guide

This document provides step-by-step instructions for building the Android Archive (AAR) file for the Android-TiffBitmapFactory library.

## Prerequisites

### Required Tools

1. **Android NDK** (version 21.3.6528147 or compatible)
   - Download from: https://developer.android.com/ndk/downloads
   - Or install via Android Studio SDK Manager

2. **Gradle** (version 8.5 or higher recommended)
   - Check installation: `gradle --version`
   - Install via package manager or download from https://gradle.org/install/

3. **Java Development Kit (JDK)** (version 11 or higher)
   - Check installation: `java -version`
   - Download from: https://adoptium.net/ or https://www.oracle.com/java/

### Required Files

This build configuration requires the following files to be present:

- `settings.gradle` - Gradle settings file (included in repository)
- `build.gradle` - Build configuration with Android Gradle Plugin 8.7.3+ (included in repository)
- Pre-built native libraries in `src/main/libs/`

## Build Process

### Step 1: Build Native Libraries (if not already built)

The native C/C++ libraries must be built before creating the AAR. If the `.so` files already exist in `src/main/libs/`, you can skip this step.

```bash
# Navigate to the project root
cd /path/to/Android-TiffBitmapFactory

# Build native libraries using ndk-build
ndk-build NDK_PROJECT_PATH=src/main
```

This will generate the following libraries for each architecture:
- `libtiff.so` - Core TIFF library
- `libtifffactory.so` - TIFF decoding functionality
- `libtiffsaver.so` - TIFF encoding functionality
- `libtiffconverter.so` - Format conversion functionality

**Supported architectures:**
- `arm64-v8a` - 64-bit ARM devices
- `armeabi-v7a` - 32-bit ARM devices

### Step 2: Verify Native Libraries

Check that the native libraries exist:

```bash
ls -la src/main/libs/arm64-v8a/
ls -la src/main/libs/armeabi-v7a/
```

You should see four `.so` files in each architecture directory.

### Step 3: Clean Previous Builds (Optional)

```bash
gradle clean
```

This removes any previously built artifacts from the `build/` directory.

### Step 4: Build the Release AAR

```bash
gradle assembleRelease
```

This command:
1. Compiles Java source files
2. Packages resources
3. Bundles native libraries
4. Generates the release AAR file

**Build time:** Typically 10-20 seconds on modern hardware.

### Step 5: Locate the Generated AAR

The AAR file will be generated at:

```
build/outputs/aar/Android-TiffBitmapFactory-release.aar
```

**File size:** Approximately 1.3 MB (includes native libraries for both architectures)

## Verifying the AAR

### Check AAR Contents

You can inspect the AAR contents using the `unzip` command:

```bash
unzip -l build/outputs/aar/Android-TiffBitmapFactory-release.aar
```

The AAR should contain:
- `AndroidManifest.xml` - Android manifest file
- `classes.jar` - Compiled Java classes
- `jni/arm64-v8a/` - Native libraries for 64-bit ARM
- `jni/armeabi-v7a/` - Native libraries for 32-bit ARM
- `res/` - Android resources
- `R.txt` - Resource identifiers

### Verify Native Libraries

Ensure all four `.so` files are present for each architecture:

```bash
unzip -l build/outputs/aar/Android-TiffBitmapFactory-release.aar | grep "\.so$"
```

Expected output should list 8 files total (4 libraries × 2 architectures).

## Using the AAR

### Option 1: Local Maven Repository

Copy the AAR to your project's `libs/` directory and add to `build.gradle`:

```gradle
dependencies {
    implementation files('libs/Android-TiffBitmapFactory-release.aar')
}
```

### Option 2: Maven Central / JitPack

For publishing to Maven repositories, see `publish-module.gradle` configuration.

### Required Permissions

Apps using this library must request storage permissions in their `AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
```

### ProGuard Configuration

If using ProGuard/R8 in your app, add these rules to prevent JNI method obfuscation:

```proguard
-keep class org.beyka.tiffbitmapfactory.**{ *; }
```

## Troubleshooting

### Build Fails: "Android SDK Build Tools version is ignored"

**Issue:** AGP 8.7.3 requires Build Tools 34.0.0 or higher.

**Solution:** This is just a warning. The build will use Build Tools 34.0.0 automatically. You can suppress this by removing `buildToolsVersion` from `build.gradle`.

### Build Fails: "Plugin was not found"

**Issue:** Android Gradle Plugin not properly configured.

**Solution:** Ensure `settings.gradle` and `build.gradle` are configured correctly with the buildscript block.

### Build Fails: Native Libraries Not Found

**Issue:** `.so` files missing from `src/main/libs/`.

**Solution:** Run `ndk-build NDK_PROJECT_PATH=src/main` to build native libraries first.

### Java Source/Target Version Deprecation Warning

**Issue:** Java compiler warns about deprecated source/target version 8.

**Solution:** This is a warning only. The build will succeed. To suppress, add to `gradle.properties`:

```properties
android.javaCompile.suppressSourceTargetDeprecationWarning=true
```

### Gradle Version Incompatibility

**Issue:** "Could not create an instance of type com.android.build.gradle.options.ProjectOptionService"

**Solution:** Update Android Gradle Plugin version in `build.gradle`:
- Gradle 9.x requires AGP 8.7.3 or higher
- Gradle 8.x requires AGP 8.0.0 or higher
- Gradle 7.x requires AGP 7.0.0 or higher

## Build Configuration Details

### Gradle Configuration

The project uses the following key configurations:

**build.gradle:**
```gradle
buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:8.7.3'
    }
}

apply plugin: 'com.android.library'

android {
    namespace 'org.beyka.tiffbitmapfactory'
    compileSdkVersion 30

    defaultConfig {
        minSdkVersion 16
        targetSdkVersion 30
        versionCode 987
        versionName "0.9.9.0"
    }

    ndkVersion "21.3.6528147"

    sourceSets.main {
        jni.srcDirs = []  // Native libs are pre-built
        jniLibs.srcDir 'src/main/libs'  // Use pre-built .so files
    }
}
```

**settings.gradle:**
```gradle
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "Android-TiffBitmapFactory"
```

### NDK Configuration

**Application.mk:**
- STL: `c++_static`
- Platform: `android-16`
- ABIs: `arm64-v8a, armeabi-v7a`
- Modules: `tiff, png, jpeg, tifffactory, tiffsaver, tiffconverter`

**Android.mk:**
- Uses 16KB page size alignment: `-Wl,-z,max-page-size=16384`
- Links against prebuilt static libraries (libpng, libjpeg)
- Optimization flags: `-O3 -fstrict-aliasing -fprefetch-loop-arrays`

## Version Information

- **Library Version:** 0.9.9.0
- **Min SDK:** 16 (Android 4.1 Jelly Bean)
- **Target SDK:** 30 (Android 11)
- **Compile SDK:** 30
- **NDK Version:** 21.3.6528147
- **Android Gradle Plugin:** 8.7.3
- **Gradle:** 8.5+ (tested with 9.2.0)

## Additional Resources

- **README.md** - Usage examples and API documentation
- **CLAUDE.md** - Architecture overview for developers
- **CHANGELOG.txt** - Version history and changes
- **ProGuard rules:** `proguard-rules.pro`

## Quick Reference

### Complete Build from Scratch

```bash
# 1. Build native libraries
ndk-build NDK_PROJECT_PATH=src/main

# 2. Build AAR
gradle clean assembleRelease

# 3. Locate AAR
ls -lh build/outputs/aar/Android-TiffBitmapFactory-release.aar
```

### Verify Build

```bash
# Check AAR size (should be ~1.3 MB)
ls -lh build/outputs/aar/*.aar

# List AAR contents
unzip -l build/outputs/aar/Android-TiffBitmapFactory-release.aar

# Verify all 8 native libraries are included
unzip -l build/outputs/aar/Android-TiffBitmapFactory-release.aar | grep "\.so$" | wc -l
```

## Distribution

The generated AAR file can be:
1. Distributed directly to app developers
2. Published to Maven Central (requires additional configuration)
3. Published to JitPack for easy dependency management
4. Included in a local Maven repository

For Maven publication setup, refer to the commented-out `publish-module.gradle` configuration in `build.gradle`.
