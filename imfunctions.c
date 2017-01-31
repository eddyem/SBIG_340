/*
 *                                                                                                  geany_encoding=koi8-r
 * imfunctions.c - functions to work with image
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

#include "usefull_macros.h"
#include "imfunctions.h"
#include "term.h"
#include <strings.h> // strncasecmp
#include <tiffio.h>  // save tiff


// find the first non-exists filename
char *make_filename(char *outfile, char *ext){
    struct stat filestat;
    static char buff[FILENAME_MAX];
    int num;
    for(num = 1; num < 10000; num++){
        if(snprintf(buff, FILENAME_MAX, "%s_%04d.%s", outfile, num, ext) < 1)
            return NULL;
        if(stat(buff, &filestat) && ENOENT == errno) // OK, file not exists
            return buff;
    }
    return NULL;
}

/**
 * Check image to store
 * @param filename (i) - output file name (or prefix with suffix)
 * @param store    (i) - "overwrite" (or "rewrite"), "normal" (or NULL), "enumerate" (or "numerate")
 */
imstorage *chk_storeimg(char *filename, char* store){
    store_type st = STORE_NORMAL;
    if(store){ // rewrite or enumerate
        int L = strlen(store);
        if(0 == strncasecmp(store, "overwrite", L) || 0 == strncasecmp(store, "rewrite", L)) st = STORE_REWRITE;
        else if(0 == strncasecmp(store, "enumerate", L) || 0 == strncasecmp(store, "numerate", L)) st = STORE_NEXTNUM;
        else{
            WARNX(_("Wrong storing type: %s"), store);
            return NULL;
        }
    }
    char *f2store = filename;
    char *nm = strdup(filename);
    if(!nm) ERRX("strdup");
    char *pt = strrchr(nm, '.');
    if(!pt){
        WARNX(_("Wrong image name pattern: this should be a filename with suffix .tiff or .jpg"));
        FREE(nm);
        return NULL;
    }
    *pt++ = 0;
    if(strcmp(pt, "tiff") && strcmp(pt, "jpg")){
        WARNX("Can save only into jpg or tiff files!");
        return NULL;
    }
    if(st == STORE_NORMAL){
        struct stat filestat;
        if(stat(filename, &filestat)){
            if(ENOENT != errno){
                WARN(_("Error access file %s"), filename);
                return NULL;
            }
        }else{
            WARNX(_("The file %s exists, use '-Srew' option to rewrite"));
            return NULL;
        }
    }else if(st == STORE_NEXTNUM){
        f2store = make_filename(nm, pt);
    }
    FREE(nm);
    imstorage *ist = MALLOC(imstorage, 1);
    ist->st = st;
    ist->imname = strdup(f2store);
    if(!ist->imname)ERR("strdup");
    return ist;
}

/**
 * Try to write tiff file
 */
int writetiff(imstorage *img){
    int ret = 1, H = img->H, W = img->W, y;
    uint16_t *data = img->imdata;
    TIFF *image = TIFFOpen(img->imname, "w");
    if(!image){
        WARN("Can't open tiff file");
        ret = 0;
        goto done;
    }
    TIFFSetField(image, TIFFTAG_IMAGEWIDTH, W);
    TIFFSetField(image, TIFFTAG_IMAGELENGTH, H);
    TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(image, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
    TIFFSetField(image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
    //tstrip_t strip = 0;
    for (y = 0; y < H;  ++y, data += W){
TIFFWriteScanline(image, data, y, 0);
      /*  if(TIFFWriteEncodedStrip(image, strip, data, W) < 0){
            ret = 0;
            goto done;
        }*/
    }

done:
    if(image) TIFFClose(image);
    return ret;
}


/**
 * Save image
 * @param filename (i) - output file name
 * @return 0 if all OK
 */
int store_image(imstorage *img){
    if(wait4image()) return 1;
    DBG("OK, get image");
    uint16_t *imdata = get_image(img);
    if(!imdata){
        WARNX(_("Error readout"));
        return 2;
    }
    img->imdata = imdata;
    green("Save image into %s\n", img->imname);
    /*int f = open(img->imname, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(f){
        DBG("%zd", write(f, img->imdata, img->W*img->H*2));
        close(f);
    }*/
    if(!writetiff(img)) return 3;
    return 0;
}
