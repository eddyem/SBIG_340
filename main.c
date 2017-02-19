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
#include <sys/wait.h>
#include <sys/prctl.h>
#include <signal.h>
#ifndef EBUG
#include <sys/prctl.h>
#endif
#ifndef CLIENT
    #include "term.h"
#endif
#include "cmdlnopts.h"
#include "imfunctions.h"
#if defined CLIENT || defined DAEMON
    #include "socket.h"
#endif

void signals(int signo){
#ifndef CLIENT
    abort_image();
    restore_console();
    restore_tty();
#endif
    exit(signo);
}

int main(int argc, char **argv){
    initial_setup();
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    glob_pars *G = parse_args(argc, argv);

    imstorage *img = NULL;
    imsubframe *F = NULL;

#ifndef CLIENT
    if(G->splist) list_speeds();
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
#if !defined DAEMON && !defined CLIENT
    if(G->takeimg){
#endif
        if(G->subframe){
            if(!(F = define_subframe(G->subframe)))
                ERRX(_("Error defining subframe"));
            G->binning = 0xff; // take subframe
        }
#endif // !CLIENT
#ifndef DAEMON
        img = chk_storeimg(G->outpfname, G->imstoretype, G->imformat);
        if(!img) return 1;
#else
        img = MALLOC(imstorage, 1); // just allocate empty: all we need in daemon is exposition & binning
#endif
#ifndef CLIENT
        if(img){
            DBG("OK");
            img->subframe = F;
            img->exptime = G->exptime;
            img->binning = G->binning;

            if(start_exposition(img, G->imtype)){ // start test exposition even in daemon
                WARNX(_("Error starting exposition"));
            }else{
                if(!get_imdata(img)){
                    WARNX(_("Error image transfer"));
                }else{
#ifndef DAEMON
                    if(store_image(img))
                        WARNX(_("Error storing image"));
#endif // !DAEMON
                }
            }
        }
#endif // !CLIENT
#if !defined DAEMON && !defined CLIENT
    }
    if(G->terminal){
        red(_("All other commandline options rejected!\n"));
        run_terminal(); // non-echo terminal mode
    }
#endif // !defined DAEMON && !defined CLIENT
#ifdef CLIENT
    img->once = G->once;
    img->timestamp = G->timestamp;
#endif
#if defined CLIENT || defined DAEMON
    daemonize(img, G->hostname, G->port);
#endif
    if(img){
        FREE(img->imname);
        FREE(img->imdata);
        FREE(img);
    }
    FREE(F);
#if !defined DAEMON && !defined CLIENT
    if(!G->shutter_cmd){ // close shutter if there wasn't direct command to do something else
        shutter_command("ck");
    }
#endif // !defined DAEMON && !defined CLIENT
}
