0.9.9.0
- Added support for decoding and converting files from file descriptors

0.9.8.7
- Fixed converting from JPG to TIFF with CCITTRLE, CCITTFAX3, CCITTFAX4 compression schemes

0.9.8.6
- Changed binary files (rebuild)

0.9.8.5
- Added signal handling of SIGSEGV signal before open tiff images to prevent many silent crashes from libtiff

0.9.8.4
- Added possibility to cancel background processing by interrupting of worker thread as usual in Java

0.9.8.3
-Fixed impossibility to convert full image without bounds

0.9.8.2
-Added handling of SIGSEGV crashes from libtiff

0.9.8
-Added support for dirrect converting files to TIFF and from TIFF
-Fixed bug causing crashing while saving tiff

0.9.7.6
-Added support for CCITT modified Huffman RLE (CCITTRLE) compression scheme

0.9.7.5
- Returned previous version of write bitmap to tiff algorithm with some improvements. This changes is due to an error of writing tiff data with bottom orientations

0.9.7.4
- Add converter utils
- Improved write and read algorithms

0.9.7.3
- Add possibility to decode part of image by specify decode area

0.9.7.2
- Changed saving algorithm to use android native bitmap
- Add progress listener for decoding process

0.9.7.1
- Added reading of various tiff tags
- Changed enums

0.9.7
- Added possibility to stop decoding that runs in separate thread
- Fixed crashing when decoding images with bad metadata

0.9.6.2
- Added support for TIFFTAG_XRESOLUTION, TIFFTAG_YRESOLUTION, TIFFTAG_RESOLUTIONUNIT

0.9.6
- Added support for CCITT Group 3 and CCITT Group 4 compression schemes

0.9.5.1
- Fixed decoding algorithm for stripped images

0.9.5 
- Added algorithms for efficient memory usage
- Added switchers for throwing exceptions and using of tiff tag orientation

