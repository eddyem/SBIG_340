/*
 *                                                                                                  geany_encoding=koi8-r
 * imfunctions.h
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#pragma once
#ifndef __IMFUNCTIONS_H__
#define __IMFUNCTIONS_H__

#include <stdint.h>
#include <stdio.h>
#include <time.h>    // time, gmtime etc

// how to save files: rewrite, check existance or add number
typedef enum{
    STORE_REWRITE,
    STORE_NORMAL,
    STORE_NEXTNUM
} store_type;

// which format should be used when image stored (OR`ed)
typedef enum{
    FORMAT_NONE = 0,
    FORMAT_FITS = 1,
    FORMAT_TIFF = 2,
    FORMAT_RAW  = 4
} image_format;

// exposed image type
typedef enum{
    IMTYPE_AUTODARK,
    IMTYPE_LIGHT,
    IMTYPE_DARK
} image_type;

// subframe parameters
typedef struct{
    uint16_t Xstart;
    uint16_t Ystart;
    uint8_t size;
} imsubframe;

// all data of image stored
typedef struct{
    store_type st; // how would files be stored
    char *imname;  // image basename
    image_format imformat;
    image_type imtype;
    double exptime;
    int binning;
    imsubframe *subframe;
    size_t W, H;       // image size
    uint16_t *imdata;  // image data itself
    time_t exposetime; // time of exposition start
    int timestamp; // add timestamp to filename
    int once; // get only one image
} imstorage;

extern double exp_calculated;

// image type suffixes
#define SUFFIX_FITS         "fits.gz"
#define SUFFIX_RAW          "bin"
#define SUFFIX_TIFF         "tiff"
#define SUFFIX_JPEG         "jpg"


void set_max_exptime(double t);
char *make_filename(imstorage *img, const char *suff);
imstorage *chk_storeimg(imstorage *img, char* store, char *format);
int store_image(imstorage *filename);
void print_stat(imstorage *img);

#ifndef CLIENT
uint16_t *get_imdata(imstorage *img);
#else
time_t get_wd_period();
#endif
int save_histo(FILE *f, imstorage *img);

#endif // __IMFUNCTIONS_H__
