TIFFGetMaxCompressionRatio
==========================

.. versionadded:: 4.7.2

Synopsis
--------

.. highlight:: c

::

    #include <tiffio.h>

.. c:function:: uint64_t TIFFGetMaxCompressionRatio(TIFF *tif);

Description
-----------

:c:func:`TIFFGetMaxCompressionRatio` returns the maximum compression ratio
for the current codec, which is typically achieved for a uncompressed buffer
with all bytes at zero.

This function can be used to determine if the compressed size of a strip or tile
is realistic compared to the expected uncompressed size, to prevent some
denial-of-service scenarios.

Depending on the codec, it may take into account the strip or tile size,
number of samples per pixel, etc.

Some codecs don't implement that method, or only for a subset of configurations,
and may return 0 when the maximum compression ratio is unknown.

Return values
-------------

0 is returned if no maximum compression ratio is known.
1 is returned when there is no compression.
Values strictly bigger than 1 are returned when a maximum compression ratio is
known.
