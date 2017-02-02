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

typedef enum{
    STORE_REWRITE,
    STORE_NORMAL,
    STORE_NEXTNUM
} store_type;

typedef enum{
    IMTYPE_AUTODARK,
    IMTYPE_LIGHT,
    IMTYPE_DARK
} image_type;

typedef struct{
    uint16_t Xstart;
    uint16_t Ystart;
    uint8_t size;
} imsubframe;

typedef struct{
    store_type st; // how would files be stored
    char *imname;
    int binning;
    image_type imtype;
    double exptime;
    int dump;
    imsubframe *subframe;
    size_t W, H;      // image size
    uint16_t *imdata; // image data itself
} imstorage;

imstorage *chk_storeimg(char *filename, char* store);
int store_image(imstorage *filename);
void print_stat(imstorage *img);
uint16_t *get_imdata(imstorage *img);

#endif // __IMFUNCTIONS_H__
