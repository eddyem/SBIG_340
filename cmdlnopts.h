/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.h - comand line options for parceargs
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
 */

#pragma once
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include "parseargs.h"
#include "term.h"

/*
 * here are some typedef's for global data
 */
typedef struct{
    int terminal;           // run as terminal (send/receive)
    char *device;           // serial device name
    int rest_pars_num;      // number of rest parameters
    heater_cmd heater;      // turn heater on/off/leave unchanged
    int splist;             // list speeds available
    int newspeed;           // change speed
    int speed;              // connect @ this speed
    char *shutter_cmd;      // shutter command: 'o' for open, 'c' for close, 'k' for de-energize
    char *subframe;         // select subframe (x,y,size)
    double exptime;         // exsposition time (1s by default)
    int binning;            // binning (1 by default)
    int takeimg;            // take image
    char *imtype;           // image type (light, autodark, dark)
    char *imstoretype;      // "overwrite" (or "rewrite"), "normal" (or NULL), "enumerate" (or "numerate")
    char *outpfname;        // output filename for image storing
    char *imformat;         // output file format
    char *hostname;         // hostname to connect
    char *port;             // port to connect
    char** rest_pars;       // the rest parameters: array of char*
} glob_pars;


glob_pars *parse_args(int argc, char **argv);
#endif // __CMDLNOPTS_H__
