# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allows opening and saving images in *.tif format (See [Wikipedia](https://en.wikipedia.org/wiki/Tagged_Image_File_Format)) on Android devices.

For decoding and encoding *.tif files it uses the native library [libtiff](https://github.com/dumganhar/libtiff). Also for images that compressed with jpeg compression scheme used [libjpeg9 for android](https://github.com/Suvitruf/libjpeg-version-9-android) (the IJG code)

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding and choose directory to decode.
While saving there is available few(most popular) compression mods and some additiona fields that can be writen to file, like author or copyright.

Minimum Android API level 8

Supported architectures: all

### Installation
Just add to your gradle dependencies :
```
compile 'com.github.beyka:androidtiffbitmapfactory:0.9.7.5'
```
And do not forget to add WRITE_EXTERNAL_STORAGE permission to main project manifest

### Build from sources
To build native part of library use [Android-NDK-bundle](https://developer.android.com/tools/sdk/ndk/index.html).
<p>To start build go to tiffbitmapfactory folder and run</p>

``` Gradle
ndk-build NDK_PROJECT_PATH=src/main
```

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

##### Memory processing
While decoding images library use native memory mechanism, so it could use all memory available for operating system. This could produce crash of your app and close other applications which are running on the device. Also in some cases it could crash operating system. 
Also in case of using more than one thread for decoding images every thread could try to use all device memory.
For avoiding of memory errors, library now has option called inAvailableMemory. Default value for this variable is 8000*8000*4 that equal to 244Mb. -1 means that decoder could use all available memory, but also it could be root of application crashes. Each separate thread that decoding tiff image will estimate how many memory it will use in decoding process. If estimate memory is less than available memory, decoder will decode image. Otherwise decoder will throw error or just return NULL(see inThrowException option).


#### Stop decoding that runs in separate thread
```Java
final TiffBitmapFactory.Options options = new TiffBitmapFactory.Options();
//Running decoding of big image in separate thread
    new Thread(new Runnable() {
        @Override
        public void run() {
            Bitmap bitmap = TiffBitmapFactory.decodePath("/sdcard/big_tiff_image.tif", options);
        }
    }).start();
//To stop thread just call stop() method in options object
options.stop();
```

#### Saving tiff file
```Java
//Open some image
Bitmap bitmap = BitmapFactory.decodeFile("sdcard/image.png");
//Create options for saving
TiffSaver.SaveOptions options = new TiffSaver.SaveOptions();
//By default compression mode is none
options.compressionScheme = CompressionScheme.COMPRESSION_LZW;
//By default orientation is top left
options.orientation = Orientation.ORIENTATION_LEFTTOP;
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
options.compressionScheme = CompressionScheme.COMPRESSION_LZW;
//By default orientation is top left
options.orientation = Orientation.ORIENTATION_LEFTTOP;
//Add author tag to output file
options.author = "beyka";
//Add copyright tag to output file
options.copyright = "Some copyright";
//Add new directory to existing file or create new file. If image saved succesfull true will be returned
boolean saved = TiffSaver.appendBitmap("/sdcard/out.tif", bitmap, options);
```
Every new page will be added as new directory to the end of file. If you trying to append directory to non-exisiting file - new file will be created

### Proguard
If you use proguard add this to you config file:
```Gradle
-keep class org.beyka.tiffbitmapfactory.**{ *; }
```

### 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

Special thanks to [dennis508](https://github.com/dennis508)    for providing of incremental reading of TIFF file


### Applications that uses library:
[B Tiff Viewer](https://play.google.com/store/apps/details?id=com.beyka.btiffviewer)

