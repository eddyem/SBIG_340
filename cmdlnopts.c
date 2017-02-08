/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

/*
 * here are global parameters initialisation
 */
int help;
glob_pars  G;

#define DEFAULT_COMDEV  "/dev/ttyUSB0"
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .terminal = 0,
    .heater = HEATER_LEAVE,
    .device = DEFAULT_COMDEV,
    .rest_pars_num = 0,
    .splist = 0,
    .newspeed = 0,
    .rest_pars = NULL,
    .shutter_cmd = NULL,
    .subframe = NULL,
    .speed = 0,
    .exptime = 1.,
    .binning = 0,
    .takeimg = 0,
    .imtype = "l",
    .imformat = NULL,
    .imstoretype = NULL,
    .outpfname = "output.tiff",
    .hostname = "localhost",
    .port = "4444"
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
// only standalone options
#if !defined DAEMON && !defined CLIENT
    {"imtype",  NEED_ARG,   NULL,   'T',    arg_string, APTR(&G.imtype),    _("image type: light (l, L), autodark (a, A), dark (d, D); default: light")},
    {"terminal",NO_ARGS,    NULL,   't',    arg_int,    APTR(&G.terminal),  _("run as terminal")},
    {"start-exp",NO_ARGS,   NULL,   'X',    arg_int,    APTR(&G.takeimg),   _("start exposition")},
#endif
// not daemon options
#ifndef DAEMON
    {"storetype",NEED_ARG,  NULL,   'S',    arg_string, APTR(&G.imstoretype),_("'overwrite'/'rewrite' to rewrite existing image, 'enumerate'/'numerate' to use given filename as base for series")},
    {"output",  NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outpfname), _("output file name (default: output.tiff)")},
    {"imformat",NEED_ARG,   NULL,   'f',    arg_string, APTR(&G.imformat),  _("image format: FITS (f), TIFF (t), raw dump with histogram storage (r,d), may be OR'ed; default: FITS or based on output image name")},
#endif
// not client options
#ifndef CLIENT
    {"device",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.device),    _("serial device name (default: " DEFAULT_COMDEV ")")},
    {"heater-on",NO_ARGS,   APTR(&G.heater),HEATER_ON,  arg_none, NULL,     _("turn heater on")},
    {"heater-off",NO_ARGS,  APTR(&G.heater),HEATER_OFF, arg_none, NULL,     _("turn heater off")},
    {"spd-list",NO_ARGS,    NULL,   'l',    arg_int,    APTR(&G.splist),    _("list speeds available")},
    {"baudrate",NEED_ARG,   NULL,   'b',    arg_int,    APTR(&G.speed),     _("connect at given baudrate without autocheck")},
    {"spd-set", NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.newspeed),  _("set terminal speed")},
    {"shutter", NEED_ARG,   NULL,   0,      arg_string, APTR(&G.shutter_cmd),_("shutter command: 'o' for open, 'c' for close, 'k' for de-energize")},
    {"subframe",NEED_ARG,   NULL,   0,      arg_string, APTR(&G.subframe),  _("select subframe: x,y,size")},
    {"exptime", NEED_ARG,   NULL,   'x',    arg_double, APTR(&G.exptime),   _("exposition time in seconds (default: 1s)")},
    {"binning", NEED_ARG,   NULL,   'B',    arg_int,    APTR(&G.binning),   _("binning (default 0: full size)")},
#endif
// not standalone options
#if defined DAEMON || defined CLIENT
#ifndef DAEMON
    {"hostname",NEED_ARG,   NULL,   'H',    arg_string, APTR(&G.hostname),  _("hostname to connect (default: localhost)")},
#endif
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      _("port to connect (default: 4444)")},
#endif
   end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    // format of help: "Usage: progname [args]\n"
    change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = calloc(argc, sizeof(char*));
        for (i = 0; i < argc; i++)
            G.rest_pars[i] = strdup(argv[i]);
    }
    return &G;
}

