/*                                                                                                  geany_encoding=koi8-r
 * main.c
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
 */
#include "usefull_macros.h"
#include <signal.h>
#include "term.h"
#include "cmdlnopts.h"
#include "imfunctions.h"

void signals(int signo){
    abort_image();
    restore_console();
    restore_tty();
    exit(signo);
}

int main(int argc, char **argv){
    initial_setup();
    for(int i = 0; i < 255; ++i) signal(i, signals);
    signal(SIGTSTP, SIG_IGN); // ctrl+Z - ignore
    signal(SIGQUIT, SIG_IGN); // ctrl+\ - ignore

    glob_pars *G = parse_args(argc, argv);
    printf("sp: %d\n", G->splist);
    if(G->splist) list_speeds();
    if(G->daemon && G->terminal){
        WARNX(_("Options --daemon and --terminal can't be together!"));
        return 1;
    }
    if(!try_connect(G->device, G->speed)){
        WARNX(_("Check power and connection: device not answer!"));
        return 1;
    }
    char *fw = get_firmvare_version();
    if(fw) printf(_("Firmware version: %s\n"), fw);
    if(G->newspeed && term_setspeed(G->newspeed))
        ERRX(_("Can't change speed to %d"), G->newspeed);
    if(G->shutter_cmd && shutter_command(G->shutter_cmd))
        WARNX(_("Can't send shutter command: %s"), G->shutter_cmd);
    if(G->heater != HEATER_LEAVE)
        heater(G->heater); // turn on/off heater
    if(G->takeimg){
        imsubframe *F = NULL;
        if(G->subframe){
            if(!(F = define_subframe(G->subframe)))
                ERRX(_("Error defining subframe"));
            G->binning = 0xff; // take subframe
        }
        imstorage *img = chk_storeimg(G->outpfname, G->imstoretype);
        if(img){
            img->subframe = F;
            img->exptime = G->exptime;
            img->binning = G->binning;
            if(start_exposition(img, G->imtype))
                WARNX(_("Error starting exposition"));
            else if(store_image(img))
                WARNX(_("Error storing image %s"), img->imname);
            FREE(img->imname);
            FREE(img->imdata);
            FREE(img);
        }
        FREE(F);
    }
    if(G->daemon || G->terminal){
        red(_("All other commandline options rejected!\n"));
        if(G->terminal) run_terminal(); // non-echo terminal mode
        if(G->daemon) daemonize();
    }
}
