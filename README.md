# Android-TiffBitmapFactory
TiffBitmapFactory is an Android library that allow open *.tif image files on Android devices.

Just now it has possibility to open tif image as mutable bitmap, read count of directory in file, apply sample rate for bitmap decoding.

# Usage
Read directory count of image:
<p>TiffBitmapFactory.getDirectoryCount(String path_to_file)</p>

Open file: 
<p>TiffBitmapFactory.decodePath(String path_to_file)</p>
<p>TiffBitmapFactory.decodeFile(File file)</p>

Additional usage:
TiffBitmapFactory class contains inner class Options that allow to tune some parameters
*<p>Options.inJustDecodeBounds</p> - if set to true will return blank bitmap with width, height and decode config
*<p>Options.inSampleSize</p> - set sample size for decoding. if sample size > 1 than image will be reduced
*<p>Options.directoryCount</p> - set directory to read from image
