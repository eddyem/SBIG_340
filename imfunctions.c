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
#include <math.h>

// image type suffixes
#define SUFFIX_FITS         "fits"
#define SUFFIX_RAW          "bin"
#define SUFFIX_TIFF         "tiff"

/**
 * NON THREAD-SAFE!
 * make filename for given name, suffix and storage type
 * @return filename or NULL if can't create it
 */
static char *make_filename(const char *outfile, const char *suff, store_type st){
    struct stat filestat;
    static char buff[FILENAME_MAX];
    if(st == STORE_NORMAL || st == STORE_REWRITE){
        snprintf(buff, FILENAME_MAX, "%s.%s", outfile, suff);
        if(stat(buff, &filestat)){
            if(ENOENT != errno){
                WARN(_("Error access file %s"), buff);
                return NULL; // some error
            }
            else return buff;
        }else{ // file exists
            if(st == STORE_REWRITE) return buff;
            else return NULL; // file exists on option STORE_NORMAL
        }
    }
    // STORE_NEXTNUM
    int num;
    for(num = 1; num < 10000; num++){
        if(snprintf(buff, FILENAME_MAX, "%s_%04d.%s", outfile, num, suff) < 1)
            return NULL;
        if(stat(buff, &filestat) && ENOENT == errno){ // OK, file not exists
            return buff;
        }
    }
    return NULL;
}

/**
 * Check image to store
 * @param filename (i) - output file name (or prefix with suffix)
 * @param store    (i) - "overwrite" (or "rewrite"), "normal" (or NULL), "enumerate" (or "numerate")
 */
imstorage *chk_storeimg(char *filename, char* store, char *format){
    FNAME();
    store_type st = STORE_NORMAL;
    image_format fmt = FORMAT_FITS;
    if(store){ // rewrite or enumerate
        int L = strlen(store);
        if(0 == strncasecmp(store, "overwrite", L) || 0 == strncasecmp(store, "rewrite", L)) st = STORE_REWRITE;
        else if(0 == strncasecmp(store, "enumerate", L) || 0 == strncasecmp(store, "numerate", L)) st = STORE_NEXTNUM;
        else{
            WARNX(_("Wrong storing type: %s"), store);
            return NULL;
        }
    }
    char *nm = strdup(filename);
    if(!nm) ERRX("strdup");
    char *pt = strrchr(nm, '.');
    if(pt){
        char *suff = pt + 1;
        DBG("got suffix: %s", suff);
        // check if name's suffix is filetype
        image_format fbysuff = FORMAT_NONE;
        if(0 == strcasecmp(suff, "tiff") || 0 == strcasecmp(suff, "tif")) fbysuff = FORMAT_TIFF;
        else if(0 == strcasecmp(suff, "fits") || 0 == strcasecmp(suff, "fit")) fbysuff = FORMAT_FITS;
        else if(0 == strcasecmp(suff, "raw") || 0 == strcasecmp(suff, "bin") || 0 == strcasecmp(suff, "dump"))
            fbysuff = FORMAT_RAW;
        if(fbysuff != FORMAT_NONE) *pt = 0; // truncate filename if suffix found
        if(format){ // check if user gave format
            fbysuff = FORMAT_NONE;
            if(strchr(format, 'f') || strchr(format, 'F')) fbysuff |= FORMAT_FITS;
            if(strchr(format, 't') || strchr(format, 'T')) fbysuff |= FORMAT_TIFF;
            if(strchr(format, 'r') || strchr(format, 'R') ||
               strchr(format, 'd') || strchr(format, 'D')) fbysuff |= FORMAT_RAW;
            if(fbysuff == FORMAT_NONE){
                WARNX(_("Wrong format string: %s"), format);
                free(nm);
                return NULL;
            }
            fmt = fbysuff;
        }else{// if user gave image with suffix, change format; else leave default: FITS
            if(fbysuff != FORMAT_NONE) fmt = fbysuff;
            DBG("fmt: %d", fmt);
        }
    }
    // now check all names
    #define FMTSZ (3)
    image_format formats[FMTSZ] = {FORMAT_FITS, FORMAT_TIFF, FORMAT_RAW};
    const char *suffixes[FMTSZ] = {SUFFIX_FITS, SUFFIX_TIFF, SUFFIX_RAW};
    for(size_t i = 0; i < FMTSZ; ++i){
        if(!(formats[i] & fmt)) continue;
        if(!make_filename(nm, suffixes[i], st)){
            WARNX(_("Can't create output file"));
            free(nm);
            return NULL;
        }
    }
    imstorage *ist = MALLOC(imstorage, 1);
    ist->st = st;
    ist->imformat = fmt;
    ist->imname = strdup(nm);
    return ist;
}

/**
 * Try to write tiff file
 * @return 1 if all OK
 */
int writetiff(imstorage *img){
    int H = img->H, W = img->W, y;
    uint16_t *data = img->imdata;
    char *name = make_filename(img->imname, SUFFIX_TIFF, img->st);
    TIFF *image = NULL;
    if(!name || !(image = TIFFOpen(name, "w"))){
        WARN("Can't save tiff file");
        return 0;
    }
    TIFFSetField(image, TIFFTAG_IMAGEWIDTH, W);
    TIFFSetField(image, TIFFTAG_IMAGELENGTH, H);
    TIFFSetField(image, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(image, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(image, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField(image, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
    TIFFSetField(image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);
    size_t S = W*sizeof(uint16_t);
    for (y = 0; y < H;  ++y, data += W){
        if(S != (size_t)TIFFWriteEncodedStrip(image, y, data, S)){
            WARNX(_("Error writing %s"), name);
            TIFFClose(image);
            return 0;
        }
        //TIFFWriteScanline(image, data, y, 0);
    }
    TIFFClose(image);
    green(_("Image %s saved\n"), name);
    return 1;
}

void print_stat(imstorage *img){
    size_t size = img->W*img->H, i, Noverld = 0L, N = 0L;
    double pv, sum=0., sum2=0., sz = (double)size, tres;
    uint16_t *ptr = img->imdata, val, valoverld;
    uint16_t max = 0, min = 65535;
    valoverld = min - 5;
    for(i = 0; i < size; i++, ptr++){
        val = *ptr;
        pv = (double) val;
        sum += pv;
        sum2 += (pv * pv);
        if(max < val) max = val;
        if(min > val) min = val;
        if(val >= valoverld) Noverld++;
    }
    printf(_("Image stat:\n"));
    double avr = sum/sz, std = sqrt(fabs(sum2/sz - avr*avr));
    printf("avr = %.1f, std = %.1f, Noverload = %ld\n", avr, std, Noverld);
    printf("max = %u, min = %u, W*H = %ld\n", max, min, size);
    Noverld = 0L;
    ptr = img->imdata; sum = 0.; sum2 = 0.;
    tres = avr + 3. * std; // max treshold == 3sigma
    for(i = 0; i < size; i++, ptr++){
        val = *ptr;
        pv = (double) val;
        if(pv > tres){
            Noverld++; // now this is an amount of overload pixels
            continue;
        }
        sum += pv;
        sum2 += (pv * pv);
        N++;
    }
    if(!N){
        printf("All pixels are over 3sigma threshold!\n");
        return;
    }
    sz = (double)N;
    avr = sum/sz; std = sqrt(fabs(sum2/sz - avr*avr));
    printf("At 3sigma: Noverload = %ld, avr = %.3f, std = %.3f\n", Noverld, avr, std);
}


/**
 * Receive image data & fill img->imdata
 * @return imdata or NULL if failed
 */
uint16_t *get_imdata(imstorage *img){
    if(wait4image()) return NULL;
    DBG("OK, get image");
    uint16_t *imdata = get_image(img);
    if(!imdata){
        WARNX(_("Error readout"));
        return NULL;
    }
    img->imdata = imdata;
    return imdata;
}

/**
 * save truncated to 256 levels histogram of `img` into file `f`
 * @return 0 if all OK
 */
int save_histo(FILE *f, imstorage *img){
    uint8_t histogram[256];
    if(!img->imdata) return 1000;
    size_t l, S = img->W*img->H;
    uint16_t *ptr = img->imdata;
    for(l = 0; l < S; ++l, ++ptr){
        ++histogram[(*ptr>>8)&0xff];
    }
    for(l = 0; l < 256; ++l){
        int status = fprintf(f, "%zd\t%u\n", l, histogram[l]);
        if(status < 0)
            return status;
    }
    return 0;
}

int writedump(imstorage *img){
    char *name = make_filename(img->imname, SUFFIX_RAW, img->st);
    if(!name) return 1;
    int f = open(name, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(f){
        size_t S = img->W*img->H*2;
        if(S != (size_t)write(f, img->imdata, S)){
            WARN(_("Error writting `%s`"), name);
            return 2;
        }
        green(_("Image dump stored in `%s`\n"), name);
        close(f);
    }else{
        WARN(_("Can't make dump"));
        return 3;
    }
    name = make_filename(img->imname, "_histogram.txt", img->st);
    if(!name) return 4;
    FILE *h = fopen(name, "w");
    if(!h) return 5;
    int i = -1;
    if(f){
        i = save_histo(h, img);
        fclose(h);
    }
    if(i < 0){
        WARN(_("Can't save histogram"));
        return 6;
    }else{
        green(_("Truncated to 256 levels histogram stored in file `%s`\n"), name);
    }
    return 0;
}

/**
 * Save image
 * @param filename (i) - output file name
 * @return 0 if all OK
 */
int store_image(imstorage *img){
    int status = 0;
    if(!img->imdata && !get_imdata(img)) return 1;
    if(img->imformat & FORMAT_TIFF){ // save tiff file
        if(!writetiff(img)) status |= 1;
    }
    if(img->imformat & FORMAT_RAW){
        if(!writedump(img)) status |= 2;
    }
    if(img->imformat & FORMAT_FITS){ // not supported yet
        status |= 4;
    }
    return status;
}
