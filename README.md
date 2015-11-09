# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allows opening of *.tif (See [Wikipedia](https://en.wikipedia.org/wiki/Tagged_Image_File_Format)) image files on Android devices.

This fork enables large Tiff files to be read and sampled incrementally. The caller specifies how much memory is available for processing. If there is sufficient memory the entire file is read in at once, sampled, and the bitmap is created. If there is insufficient memory, the file is read a Tile or Strip at a time, sampled, and the bitmap is created.

For decoding *.tif files it uses the native library [libtiff](https://github.com/dumganhar/libtiff)

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding and choose directory to decode.

Minimum Android API level 8

### Usage
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

### Build
To build native part of library use [Android-NDK-bundle-10](https://developer.android.com/tools/sdk/ndk/index.html) or higher.
<p>Go to tiffbitmapfactory folder and run</p>
```
ndk-build NDK_PROJECT_PATH=src/main
```

Special thanks to dennis508@yahoo.com for providing of incremental reading of TIFF file


