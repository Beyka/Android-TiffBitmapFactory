//
// Created by beyka on 9/21/17.
//

#ifndef __BMP_H_INCLUDED__   // if x.h hasn't been included yet...
#define __BMP_H_INCLUDED__

#pragma pack(2)
typedef struct BITMAPFILEHEADER
{
    BITMAPFILEHEADER() : bfReserved1(0), bfReserved2(0) {}
    unsigned char bfType[2];
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;
} BITMAPFILEHEADER;

typedef struct                       /**** BMP file info structure ****/
{
    unsigned int   biSize;           /* Size of info header */
    int            biWidth;          /* Width of image */
    int            biHeight;         /* Height of image */
    unsigned short biPlanes;         /* Number of color planes */
    unsigned short biBitCount;       /* Number of bits per pixel */
    unsigned int   biCompression;    /* Type of compression to use 0 - none, 1 - rle 8 bit, 2 - rle 4 but, 3 - bi_bitfields*/
    unsigned int   biSizeImage;      /* Size of image data */
    int            biXPelsPerMeter;  /* X pixels per meter */
    int            biYPelsPerMeter;  /* Y pixels per meter */
    unsigned int   biClrUsed;        /* Number of colors used */
    unsigned int   biClrImportant;   /* Number of important colors */
    unsigned int biPalete[3] ;
    unsigned char reserved[56];
    /* Palete (masks for 16 bit bmps)*/
    /*a 5-5-5 16-bit image, where the blue mask is 0x001f, the green mask is 0x03e0, and the red mask is 0x7c00; and a 5-6-5 16-bit image, where the blue mask is 0x001f, the green mask is 0x07e0, and the red mask is 0xf800*/
} BITMAPINFOHEADER;


#pragma pack()

#endif