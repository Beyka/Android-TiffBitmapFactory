# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allow open *.tif(See [Wikipedia](https://en.wikipedia.org/wiki/Tagged_Image_File_Format)) image files on Android devices.

For decoding *.tif files it uses native library [libtiff](https://github.com/dumganhar/libtiff)

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding and choose directory to decode.

### Usage
```Java
File file = new File("sdcard/image.tif");

//Read data about image to Options object
TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
options.inJustDecodeBounds = true;
TiffBitmapFactory.decodeFile(file, options);

int dirCount = options.outDirectoryCount;
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

//Set calculated parameters to Options  object
options.inJustDecodeBounds = false;
options.inSampleSize = inSampleSize;

//Decode bitmap from first directory
Bitmap bmp = TiffBitmapFactory.decodeFile(file, options);

//Decode bitmap from last directory
options.inDirectoryCount = dirCount;
options.inJustDecodeBounds = true;
TiffBitmapFactory.decodeFile(file, options);
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

//Set calculated parameters to Options  object
options.inJustDecodeBounds = false;
options.inSampleSize = inSampleSize;

//Decode bitmap from first directory
Bitmap secondBmp = TiffBitmapFactory.decodeFile(file, options);

```

### Build
To build native part of library use [Android-NDK-bundle-10](https://developer.android.com/tools/sdk/ndk/index.html) or higher.
<p>Go to tiffbitmapfactory folder and run</p>
```
ndk-build NDK_PROJECT_PATH=src/main
```

<p>To contact me use email leshabey 'at' gmail.com</p>
