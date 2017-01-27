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

int rewrite_ifexists = 0, // rewrite existing files == 0 or 1
    verbose = 0; // each -v increments this value, e.g. -vvv sets it to 3
#define DEFAULT_COMDEV  "/dev/ttyUSB0"
//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .daemon = 0,
    .terminal = 0,
    .heater = HEATER_LEAVE,
    .device = DEFAULT_COMDEV,
    .rest_pars_num = 0,
    .splist = 0,
    .newspeed = 0,
    .rest_pars = NULL
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
myoption cmdlnopts[] = {
    // set 1 to param despite of its repeating number:
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"daemon",  NO_ARGS,    NULL,   'd',    arg_int,    APTR(&G.daemon),    _("run as daemon")},
    {"terminal",NO_ARGS,    NULL,   't',    arg_int,    APTR(&G.terminal),  _("run as terminal")},
    {"device",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.device),    _("serial device name (default: " DEFAULT_COMDEV ")")},
    {"heater-on",NO_ARGS,   APTR(&G.heater),HEATER_ON,  arg_none, NULL,     _("turn heater on")},
    {"heater-off",NO_ARGS,  APTR(&G.heater),HEATER_OFF, arg_none, NULL,     _("turn heater off")},
    {"spd-list",NO_ARGS,    NULL,   'l',    arg_int,    APTR(&G.splist),    _("list speeds available")},
    {"spd-set", NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.newspeed),  _("set terminal speed")},
    // simple integer parameter with obligatory arg:
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

