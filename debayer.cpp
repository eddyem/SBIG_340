/*
 *                                                                                                  geany_encoding=koi8-r
 * debayer.cpp - debayer image using libraw
 * based on openbayer_sample.cpp from LibRaw samples
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libraw/libraw.h>
#include <gd.h>

#include "debayer.h"
#include "usefull_macros.h"

static int write_jpeg(const char *fname, const uint8_t *data, imstorage *img){
    if(!img) return 1;
    size_t nx = img->W, ny = img->H;
    gdImagePtr im = gdImageCreateTrueColor(nx, ny);
    if(!im) return 4;
    //for(size_t y = 0; y < ny; ++y)for(size_t x = 0; x < nx; ++x) im->tpixels[y][x] = 0XFF0000;
    size_t x, y;
    for(y = 0; y < ny; ++y){
        for(x = 0; x < nx; ++x){
            im->tpixels[y][x] = (data[0] << 16) | (data[1] << 8) | data[2] ;
            data += 3;
        }
    }
    FILE *fp = fopen(fname, "w");
    if(!fp){
        fprintf(stderr, "Can't save jpg image %s\n", fname);
        gdImageDestroy(im);
        return 5;
    }
    char date[256];
    strftime(date, 256, "%d/%m/%y\n%H:%M:%S", localtime(&img->exposetime));
    gdFTUseFontConfig(1);
    char *ret = gdImageStringFT(im, NULL, 0xffffff, "monotype", 10, 0., 2, 12, date);
    if(ret) fprintf(stderr, "Error: %s\n", ret);
    im->tpixels[10][10] = 0XFF0000;
    im->tpixels[15][15] = 0XFF0000;
    gdImageJpeg(im, fp, 90);
    fclose(fp);
    gdImageDestroy(im);
    return 0;
}

/**
 * Debayer image `img` and store it
 * @param black - black level (minimum on image)
 * @return 0 if all OK
 */
int write_debayer(imstorage *img, uint16_t black){
    char *name = make_filename(img, SUFFIX_JPEG);
    if(!name) return 1;
    int r = 0;
    size_t fsz = img->W * img->H * sizeof(uint16_t);
    LibRaw rp;
    rp.imgdata.params.output_tiff = 1;
    int ret = rp.open_bayer((unsigned char*)img->imdata, fsz, img->W, img->H,
            0,0,0,0,0, LIBRAW_OPENBAYER_BGGR, 0,0, black);
    if(ret != LIBRAW_SUCCESS) return 2;
    if ((ret = rp.unpack()) != LIBRAW_SUCCESS){
        WARNX(_("Unpack error: %d"), ret);
        rp.recycle();
        return 3;
    }
    if((ret = rp.dcraw_process()) != LIBRAW_SUCCESS){
        WARNX(_("Processing error: %d"), ret);
        rp.recycle();
        return 4;
    }
    libraw_processed_image_t *image = rp.dcraw_make_mem_image(&ret);
    if(!image){
        WARNX(_("Can't make memory image: %d"), ret);
        rp.recycle();
        return 5;
    }
    if(image->type != LIBRAW_IMAGE_BITMAP){
        r = 6; goto retn;
    }
    if(image->colors != 3){
        r = 7; goto retn;
    }
    write_jpeg(name, image->data, img);
retn:
    LibRaw::dcraw_clear_mem(image);
    rp.recycle();
    return r;
}
