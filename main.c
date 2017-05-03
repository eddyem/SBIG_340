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
#include <time.h>
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
    putlog("Exit with status %d", signo);
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
    if(G->rest_pars_num)
        openlogfile(G->rest_pars[0]);
    imstorage *img = NULL;
    imsubframe *F = NULL;
    #ifndef CLIENT
    if(G->htrperiod) set_heater_period(G->htrperiod);
    if(G->max_exptime > 0) set_max_exptime(G->max_exptime);
    if(G->splist){
        list_speeds();
        return 0;
    }
    #endif // !CLIENT
// daemonize @ start
#if defined DAEMON || defined CLIENT
    if(!G->once){
        #ifndef EBUG
        putlog("Daemonize");
        fflush(stdout);
        if(daemon(1, 0)){
            putlog("Can't daemonize");
            ERR("daemon()");
        }
        #endif // EBUG
        while(1){ // guard for dead processes
            pid_t childpid = fork();
            if(childpid){
                putlog("Create child with PID %d\n", childpid);
                DBG("Create child with PID %d\n", childpid);
                wait(NULL);
                putlog("Child %d died\n", childpid);
                WARNX("Child %d died\n", childpid);
                sleep(1);
            }else{
                prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
                break; // go out to normal functional
            }
        }
    }
#endif // DAEMON || CLIENT
#ifndef CLIENT
    if(!try_connect(G->device, G->speed)){
        putlog("device not answer");
        WARNX(_("Check power and connection: device not answer!"));
        return 1;
    }
    char *fw = get_firmvare_version();
    if(fw) printf(_("Firmware version: %s\n"), fw);
    if(G->newspeed && term_setspeed(G->newspeed)){
        putlog("Can't change speed to %d", G->newspeed);
        ERRX(_("Can't change speed to %d"), G->newspeed);
    }
    if(G->shutter_cmd && shutter_command(G->shutter_cmd)){
        putlog("Can't send shutter command: %s", G->shutter_cmd);
        WARNX(_("Can't send shutter command: %s"), G->shutter_cmd);
    }
    if(G->heater != HEATER_LEAVE){
        heater(G->heater); // turn on/off heater
    }
#ifndef DAEMON
    if(G->takeimg){
#endif // DAEMON
        if(G->subframe){
            if(!(F = define_subframe(G->subframe))){
                putlog("Error defining subframe");
                ERRX(_("Error defining subframe"));
            }
            G->binning = 0xff; // take subframe
        }
#endif // !CLIENT
        img = MALLOC(imstorage, 1);
#ifdef CLIENT
        img->once = G->once;
        img->timestamp = G->timestamp;
#endif
#ifndef DAEMON
        img->imname = strdup(G->outpfname);
        img->exposetime = time(NULL);
        if(!chk_storeimg(img, G->imstoretype, G->imformat)) return 1;
#endif
#ifndef CLIENT
        if(img){
            DBG("OK");
            img->subframe = F;
            img->exptime = G->exptime;
            img->binning = G->binning;

            if(start_exposition(img, G->imtype)){ // start test exposition even in daemon
                putlog("Error starting exposition");
                ERRX(_("Error starting exposition"));
            }else{
                if(!get_imdata(img)){
                    putlog("Error image transfer");
                    WARNX(_("Error image transfer"));
                }else{
#ifndef DAEMON
                    if(store_image(img)){
                        putlog("Error storing image");
                        WARNX(_("Error storing image"));
                    }
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
#if defined CLIENT || defined DAEMON
    #ifdef DAEMON
        set_darks(G->min_dark_exp, G->dark_interval);
    #endif
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
