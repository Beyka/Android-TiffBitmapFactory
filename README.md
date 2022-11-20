# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allows opening and saving images in *.tif format (See [Wikipedia](https://en.wikipedia.org/wiki/Tagged_Image_File_Format)) on Android devices.

For decoding and encoding *.tif files it uses the native library [libtiff](https://github.com/dumganhar/libtiff). Also for images that compressed with jpeg compression scheme used [libjpeg9 for android](https://github.com/Suvitruf/libjpeg-version-9-android) (the IJG code). For converting from PNG to TIFF and from TIFF to PNG used library [libpng-android](https://github.com/julienr/libpng-android).

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding and choose directory to decode.
While saving there is available few(most popular) compression mods and some additiona fields that can be writen to file, like author or copyright.

Minimum Android API level 16

Supported architectures: all

### Installation
Just add to your gradle dependencies :
```
implementation 'io.github.beyka:Android-TiffBitmapFactory:0.9.9.1'
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
Starting Android-Q we can't open any file from sdcar, just files from scoped storage of application
If you need open file somewhere in sdcardm you should use [Storage Access Framework](https://android-doc.github.io/guide/topics/providers/document-provider.html)

Request document chooser(Android system don't know image/tiff type so using */*):
```Java
Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
intent.addCategory(Intent.CATEGORY_OPENABLE);
intent.setType("*/*");
startActivityForResult(intent, requestCode);
```
Getting answer from chooser:
```Java
@Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE) {
            try {
                ParcelFileDescriptor parcelFileDescriptor = getContentResolver().openFileDescriptor(data.getData(), "r");
                Bitmap bmp = TiffBitmapFactory.decodeFileDescriptor(parcelFileDescriptor.getFd());
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }
    }
```
Same method used also for TiffSaver and TiffConverter classes

For pre Q devices and for scoped storage you can use old api:
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
For avoiding of memory errors, library now has option called inAvailableMemory. Default value for this variable is 8000x8000x4 that equal to 244Mb. -1 means that decoder could use all available memory, but also it could be root of application crashes. Each separate thread that decoding tiff image will estimate how many memory it will use in decoding process. If estimate memory is less than available memory, decoder will decode image. Otherwise decoder will throw error or just return NULL(see inThrowException option).


#### Stop decoding that runs in separate thread
```Java
//Running decoding of big image in separate thread
Thread thread = new Thread(new Runnable() {
        @Override
        public void run() {
            Bitmap bitmap = TiffBitmapFactory.decodePath("/sdcard/big_tiff_image.tif");
        }
    });
thread.start();
//To stop thread just interrupt thread as usual
thread.interrupt();
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


#### Converting to tiff
There is possibility for dirrect convert from some formats to TIFF. This method uses as less memory as possible. Use this method if you want create TIFF form realy big image file.
```Java
TiffConverter.ConverterOptions options = new TiffConverter.ConverterOptions();
options.throwExceptions = false; //Set to true if you want use java exception mechanism;
options.availableMemory = 128 * 1024 * 1024; //Available 128Mb for work;
options.compressionScheme = CompressionScheme.LZW; //compression scheme for tiff
options.appendTiff = false;//If set to true - will be created one more tiff directory, otherwise file will be overwritten
TiffConverter.convertToTiff("/sdcard/some_image.jpg", "/sdcard/out.tif", options, progressListener);
```

If you need convert tiff to some other format:
```Java
TiffConverter.ConverterOptions options = new TiffConverter.ConverterOptions();
options.throwExceptions = false; //Set to true if you want use java exception mechanism;
options.availableMemory = 128 * 1024 * 1024; //Available 128Mb for work;
options.readTiffDirectory = 1; //Number of tiff directory to convert;
        
//Convert to JPEG
TiffConverter.convertTiffJpg("/sdcard/in.tif", "/sdcard/out.jpg", options, progressListener);
//Convert to PNG
TiffConverter.convertTiffPng("/sdcard/in.tif", "/sdcard/out.png", options, progressListener);
//Convert to BMP
TiffConverter.convertTiffBmp("/sdcard/in.tif", "/sdcard/out.bmp", options, progressListener);
```
For now library support JPEG, PNG and BMP formats for converting.


#### Progress listener
All operations(read, create, convert) have support for progress reporting.
```Java
IProgressListener progressListener = new IProgressListener() {
    @Override
    public void reportProgress(long processedPixels, long totalPixels) {
        Log.v("Progress reporter", String.format("Processed %d pixels from %d", processedPixels, totalPixels);
    }
};
```

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

