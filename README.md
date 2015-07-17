# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allow open *.tif image files on Android devices.

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding.

## Usage
###Read directory count of image:
```Java
TiffBitmapFactory.getDirectoryCount(String path_to_file)
```

###Open file: 
```Java
TiffBitmapFactory.decodePath(String path_to_file)
```
```Java
TiffBitmapFactory.decodeFile(File file)
```

###Additional usage:
<p>TiffBitmapFactory class contains inner class Options that allow to tune some parameters</p>
```Java
Options.inJustDecodeBounds
```
if set to true will return blank bitmap with width, height and decode config

```Java
Options.inSampleSize
```
set sample size for decoding. if sample size > 1 than image will be reduced

```Java
Options.directoryCount
```
set directory to read from image
