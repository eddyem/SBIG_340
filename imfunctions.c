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
#include <math.h>    // sqrt
#include <time.h>    // time, gmtime etc
#include <fitsio.h>  // save fits
#include <libgen.h>  // basename
#include <sys/stat.h> // utimensat
#include <fcntl.h>   // AT_...
/**
 * All image-storing functions modify ctime of saved files to be the time of
 * exposition start!
 */

void modifytimestamp(char *filename, imstorage *img){
    if(!filename) return;
    struct timespec times[2];
    memset(times, 0, 2*sizeof(struct timespec));
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = img->exposetime; // change mtime
    if(utimensat(AT_FDCWD, filename, times, 0)) WARN(_("Can't change timestamp for %s"), filename);
}

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
            if(st == STORE_REWRITE){
                if(0 == strcmp(suff, SUFFIX_FITS)) // add '!' before image name
                snprintf(buff, FILENAME_MAX, "!%s.%s", outfile, suff);
                return buff;
            }
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
    image_format fbysuff = FORMAT_NONE;
    // check if name's suffix is filetype
    if(pt){
        char *suff = pt + 1;
        DBG("got suffix: %s", suff);
        if(0 == strcasecmp(suff, "tiff") || 0 == strcasecmp(suff, "tif")) fbysuff = FORMAT_TIFF;
        else if(0 == strcasecmp(suff, "fits") || 0 == strcasecmp(suff, "fit")) fbysuff = FORMAT_FITS;
        else if(0 == strcasecmp(suff, "raw") || 0 == strcasecmp(suff, "bin") || 0 == strcasecmp(suff, "dump"))
            fbysuff = FORMAT_RAW;
        if(fbysuff != FORMAT_NONE) *pt = 0; // truncate filename if suffix found
    }
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
 * @return 0 if all OK
 */
int writetiff(imstorage *img){
    int H = img->H, W = img->W, y;
    uint16_t *data = img->imdata;
    char *name = make_filename(img->imname, SUFFIX_TIFF, img->st);
    TIFF *image = NULL;
    if(!name || !(image = TIFFOpen(name, "w"))){
        WARN("Can't save tiff file");
        return 1;
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
            return 2;
        }
        //TIFFWriteScanline(image, data, y, 0);
    }
    TIFFClose(image);
    green(_("Image %s saved\n"), name);
    modifytimestamp(name, img);
    return 0;
}

/**
 * Calculate image statistics: print it on screen and save for `writefits`
 */
static uint16_t glob_min, glob_max, glob_avr, glob_std;
void print_stat(imstorage *img){
    size_t size = img->W*img->H, i, Noverld = 0L, N = 0L;
    double pv, sum=0., sum2=0., sz = (double)size, tres;
    uint16_t *ptr = img->imdata, val, valoverld;
    uint16_t max = 0, min = 65535;
    valoverld = 65530;
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
    glob_avr = avr, glob_std = std, glob_max = max, glob_min = min;
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

#define TRYFITS(f, ...)                     \
do{ int status = 0;                         \
    f(__VA_ARGS__, &status);                \
    if (status){                            \
        fits_report_error(stderr, status);  \
        return -1;}                         \
}while(0)
#define WRITEKEY(...)                           \
do{ int status = 0;                             \
    fits_write_key(fp, __VA_ARGS__, &status);       \
    if(status) fits_report_error(stderr, status);\
}while(0)

int writefits(imstorage *img){
    long naxes[2] = {img->W, img->H};
    char buf[80];
    fitsfile *fp;
    char *filename = make_filename(img->imname, SUFFIX_FITS, img->st);
    TRYFITS(fits_create_file, &fp, filename);
    TRYFITS(fits_create_img, fp, USHORT_IMG, 2, naxes);
    // FILE / Input file original name
    char *fn = basename(filename);
    if(*fn == '!') ++fn; // remove '!' from filename
    WRITEKEY(TSTRING, "FILE", fn, "Input file original name");
    // ORIGIN / organization responsible for the data
    WRITEKEY(TSTRING, "ORIGIN", "SAO RAS", "organization responsible for the data");
    // OBSERVAT / Observatory name
    WRITEKEY(TSTRING, "OBSERVAT", "Special Astrophysical Observatory, Russia", "Observatory name");
    // DETECTOR / detector
    WRITEKEY(TSTRING, "DETECTOR", "Kodak KAI-340", "Detector model");
    // INSTRUME / Instrument
    WRITEKEY(TSTRING, "INSTRUME", "SBIG All-sky 340C", "Instrument");
    // PXSIZE / pixel size
    WRITEKEY(TSTRING, "PXSIZE", "7.4 x 7.4", "Pixel size in um");
    WRITEKEY(TSTRING, "FIELD", "180 degrees", "Camera field of view");
    switch(img->imtype){
        case IMTYPE_AUTODARK:
            sprintf(buf, "object without dark");
        break;
        case IMTYPE_DARK:
            sprintf(buf, "dark");
        break;
        case IMTYPE_LIGHT:
        default:
            sprintf(buf, "object");
    }
    // IMAGETYP / object, flat, dark, bias, scan, eta, neon, push
    WRITEKEY(TSTRING, "IMAGETYP", buf, "Image type");
    // DATAMAX, DATAMIN / Max,min pixel value
    uint16_t val = UINT16_MAX;
    WRITEKEY(TUSHORT, "DATAMAX", &val, "Max pixel value");
    val = 0;
    WRITEKEY(TUSHORT, "DATAMIN", &val, "Min pixel value");
    // statistical values
    WRITEKEY(TUSHORT, "STATMAX", &glob_max, "Max pixel value");
    WRITEKEY(TUSHORT, "STATMIN", &glob_min, "Min pixel value");
    WRITEKEY(TUSHORT, "STATAVR", &glob_avr, "Average pixel value");
    WRITEKEY(TUSHORT, "STATSTD", &glob_std, "Standart deviation of pixel value");
    // EXPTIME / actual exposition time (sec)
    WRITEKEY(TDOUBLE, "EXPTIME", &img->exptime, "actual exposition time (sec)");
    // DATE / Creation date (YYYY-MM-DDThh:mm:ss, UTC)
    time_t savetime = time(NULL);
    strftime(buf, 79, "%Y-%m-%dT%H:%M:%S", gmtime(&savetime));
    WRITEKEY(TSTRING, "DATE", buf, "Creation date (YYYY-MM-DDThh:mm:ss, UTC)");
    struct tm *tm_starttime = gmtime(&img->exposetime);
    strftime(buf, 79, "exposition starts at %d/%m/%Y, %H:%M:%S (UTC)", tm_starttime);
    //long tstart = (long)img->exposetime;
    WRITEKEY(TLONG, "UNIXTIME", &img->exposetime, buf);
    tm_starttime = localtime(&img->exposetime);
    strftime(buf, 79, "%Y/%m/%d", tm_starttime);
    // DATE-OBS / DATE (YYYY/MM/DD) OF OBS.
    WRITEKEY(TSTRING, "DATE-OBS", buf, "DATE OF OBS. (YYYY/MM/DD, local)");
    strftime(buf, 79, "%H:%M:%S", tm_starttime);
    // START / Measurement start time (local) (hh:mm:ss)
    WRITEKEY(TSTRING, "START", buf, "Measurement start time (hh:mm:ss, local)");
    // OBJECT  / Object name
    WRITEKEY(TSTRING, "OBJECT", "sky", "Object name");
    // BINNING / Binning
    if(img->binning == 2){
        WRITEKEY(TSTRING, "BINNING", "2 x 2", "Binning (hbin x vbin)");
    }
    if(img->subframe){ // subframe
        snprintf(buf, 80, "(%d, %d)", img->subframe->Xstart, img->subframe->Ystart);
        WRITEKEY(TSTRING, "SUBFRAME", buf, "Subframe start coordinates (Xstart, Ystart)");
    }
    TRYFITS(fits_write_img, fp, TUSHORT, 1, img->W * img->H, img->imdata);
    TRYFITS(fits_close_file, fp);
    if(*filename == '!') ++filename; // remove '!' from filename
    modifytimestamp(filename, img);
    green(_("Image %s saved\n"), filename);
    return 0;
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
    if(!img->imdata) return 1000;
    size_t histogram[256];
    size_t l, S = img->W*img->H;
    uint16_t *ptr = img->imdata;
    memset(histogram, 0, 256 * sizeof(size_t));
    for(l = 0; l < S; ++l, ++ptr){
        ++histogram[((*ptr)>>8)&0xff];
    }
    for(l = 0; l < 256; ++l){
        int status = fprintf(f, "%zd\t%zd\n", l, histogram[l]);
        if(status < 0){
            return status;
        }
    }
    size_t low2 = S/50, med = S/2, up2 = (S*49)/50, acc = 0;
    int lval = -1, mval = -1, tval = -1;
    for(l = 0; l < 256; ++l){ // get stat parameters
        acc += histogram[l];
        if(lval < 0 && acc >= low2) lval = l;
        else if(mval < 0 && acc >= med) mval = l;
        else if(tval < 0 && acc >= up2) tval = l;
    }
    printf("low 2%% (%zd pixels) = %d, median (%zd pixels) = %d, up 2%% (%zd pixels) = %d\n",
        low2, lval, med, mval, up2, tval);
    double mul = 1., mulmax = 255. / tval;
    if(mval < 120 || mval > 134){
        if(lval > 32) mul = 96. / mval;
        else if(mval < 127) mul = 120. / mval;
        else if(mval < 196) mul = 120. / (mval - lval);
        else if(mval < 245) mul = 96. / (tval - lval);
        else mval = 0.03;
    }
    if(mul > mulmax) mul = mulmax;
    green("Recommended exposition time: %.2f seconds\n", img->exptime * mul);
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
    modifytimestamp(name, img);
    name = make_filename(img->imname, "histogram.txt", img->st);
    if(!name) return 4;
    FILE *h = fopen(name, "w");
    if(!h) return 5;
    int i = -1;
    if(f){
        i = save_histo(h, img);
        fclose(h);
        modifytimestamp(name, img);
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
    print_stat(img);
    if(img->imformat & FORMAT_TIFF){ // save tiff file
        if(writetiff(img)) status |= 1;
    }
    if(img->imformat & FORMAT_RAW){
        if(writedump(img)) status |= 2;
    }
    if(img->imformat & FORMAT_FITS){ // not supported yet
        if(writefits(img)) status |= 4;
    }
    return status;
}
