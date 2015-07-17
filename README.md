# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allow open *.tif image files on Android devices.

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
//Set last directory of image to decode
options.inDirectoryCount = dirCount;

//Decode bitmap
Bitmap bmp = TiffBitmapFactory.decodeFile(file, options);
```

