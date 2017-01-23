# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allows opening and saving images in *.tif format (See [Wikipedia](https://en.wikipedia.org/wiki/Tagged_Image_File_Format)) on Android devices.

This fork enables large Tiff files to be read and sampled incrementally. The caller specifies how much memory is available for processing. If there is sufficient memory the entire file is read in at once, sampled, and the bitmap is created. If there is insufficient memory, the file is read a Tile or Strip at a time, sampled, and the bitmap is created.

For decoding and encoding *.tif files it uses the native library [libtiff](https://github.com/dumganhar/libtiff)

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding and choose directory to decode.
While saving there is available few(most popular) compression mods and some additiona fields that can be writen to file, like author or copyright.

Minimum Android API level 8

Starting from version 0.8 library has support for all architectures

### Installation
Just add to your gradle dependencies :
```
compile 'com.github.beyka:androidtiffbitmapfactory:0.8.2'
```
And do not forget to add WRITE_EXTERNAL_STORAGE permission to main project manifest

### Build from sources
To build native part of library use [Android-NDK-bundle-10](https://developer.android.com/tools/sdk/ndk/index.html) or higher.
<p>For now arm64 x86_64 and mips64 abies is supporting but it has problems with decoding files with jpeg compression. To avoid broken builds for all abies make different builds for armeabi-v7a x86 armeabi mips and for arm64-v8a x86_64 mips64. To do this just use different abies in Application.mk file.</p>

<p>To start build go to tiffbitmapfactory folder and run</p>
```
ndk-build NDK_PROJECT_PATH=src/main
```

### Known problems
On devices with x64 architecture can't open and save files with JPEG compression

### Usage
#### Opening tiff file
```Java
File file = new File("/sdcard/image.tif");

//Read data about image to Options object
TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
options.inJustDecodeBounds = true;
TiffBitmapFactory.decodeFile(file, options);

int dirCount = options.outDirectoryCount;


//Read and process all images in file
for (int i = 0; i < dirCount; i++) {
    options.inDirectoryNumber = i;
    TiffBitmapFactory.decodeFile(file, options);
    int curDir = options.outCurDirectoryNumber;
    int width = options.outWidth;
    int height = options.outHeight;
    //Change sample size if width or height bigger than required width or height
    int inSampleSize = 1;
    if (height > reqHeight || width > reqWidth) {

        final int halfHeight = height / 2;
        final int halfWidth = width / 2;

        // Calculate the largest inSampleSize value that is a power of 2 and keeps both
        // height and width larger than the requested height and width.
        while ((halfHeight / inSampleSize) > reqHeight
                        && (halfWidth / inSampleSize) > reqWidth) {
            inSampleSize *= 2;
        }
    }
    options.inJustDecodeBounds = false;
    options.inSampleSize = inSampleSize;
    
    // Specify the amount of memory available for the final bitmap and temporary storage.
    options.inAvailableMemory = 20000000; // bytes
    
    Bitmap bmp = TiffBitmapFactory.decodeFile(file, options);
    processBitmap(bmp);
}
```

In case you open realy heavy images and to avoid crashes of application you can use inAvailableMemory option:
```Java
TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
options.inAvailableMemory = 1024 * 1024 * 10; //10 mb
Bitmap bmp = TiffBitmapFactory.decodeFile(file, options);
```

#### Saving tiff file
```Java
//Open some image
Bitmap bitmap = BitmapFactory.decodeFile("sdcard/image.png");
//Create options for saving
TiffSaver.SaveOptions options = new TiffSaver.SaveOptions();
//By default compression mode is none
options.compressionMode = TiffSaver.CompressionMode.COMPRESSION_LZW;
//By default orientation is top left
options.orientation = TiffSaver.Orientation.ORIENTATION_LEFTTOP;
//Add author tag to output file
options.author = "beyka";
//Add copyright tag to output file
options.copyright = "Some copyright";
//Save image as tif. If image saved succesfull true will be returned
boolean saved = TiffSaver.saveBitmap("/sdcard/out.tif", bitmap, options);
```

#### Adding page to existing tiff file
```Java
//Open some image
Bitmap bitmap = BitmapFactory.decodeFile("sdcard/image.png");
//Create options for saving
TiffSaver.SaveOptions options = new TiffSaver.SaveOptions();
//By default compression mode is none
options.compressionMode = TiffSaver.CompressionMode.COMPRESSION_LZW;
//By default orientation is top left
options.orientation = TiffSaver.Orientation.ORIENTATION_LEFTTOP;
//Add author tag to output file
options.author = "beyka";
//Add copyright tag to output file
options.copyright = "Some copyright";
//Save image as tif. If image saved succesfull true will be returned
boolean saved = TiffSaver.appendBitmap("/sdcard/out.tif", bitmap, options);
```
Every new page will be added as new directory to the end of file. If you trying to append directory to non-exisiting file - new file will be created


### 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

Special thanks to [dennis508](https://github.com/dennis508)    for providing of incremental reading of TIFF file


