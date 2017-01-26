/*
 * client.c - simple terminal client
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
#include "usefull_macros.h"
#include "term.h"
/*
#include <termios.h>        // tcsetattr
#include <unistd.h>         // tcsetattr, close, read, write
#include <sys/ioctl.h>      // ioctl
#include <stdio.h>          // printf, getchar, fopen, perror
#include <stdlib.h>         // exit
#include <sys/stat.h>       // read
#include <fcntl.h>          // read
#include <signal.h>         // signal
#include <time.h>           // time
#include <string.h>         // memcpy
#include <stdint.h>         // int types
#include <sys/time.h>       // gettimeofday
*/
#define BUFLEN 1024

tcflag_t Bspeeds[] = {
    B9600,
    B19200,
    B38400,
    B57600,
    B115200,
    B230400,
    B460800
};
static const int speedssize = (int)sizeof(Bspeeds)/sizeof(Bspeeds[0]);
static int curspd = -1;
int speeds[] = {
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800
};

/**
 * Return -1 if not connected or value of current speed in bps
 */
int get_curspeed(){
    if(curspd < 0) return -1;
    return speeds[curspd];
}

/**
 * Send command by serial port, return 0 if all OK
 */
static uint8_t last_chksum = 0;
int send_data(uint8_t *buf){
    if(!*buf) return 1;
    uint8_t chksum = 0, *ptr = buf;
    int l;
    for(l = 0; *ptr; ++l)
        chksum ^= ~(*ptr++) & 0x7f;
    DBG("send: %s%c", buf, (char)chksum);
    if(write_tty(buf, l)) return 1;
    if(write_tty(&chksum, 1)) return 1;
    last_chksum = chksum;
    return 0;
}
int send_cmd(uint8_t cmd){
    uint8_t s[2];
    s[0] = cmd;
    s[1] = ~(cmd) & 0x7f;
    if(write_tty(s, 2)) return 1;
    last_chksum = s[1];
    return 0;
}

/**
 * Wait for answer with checksum
 */
trans_status wait_checksum(){
    double d0 = dtime();
    uint8_t chr;
    do{
        if(read_tty(&chr, 1)) break;
    }while(dtime() - d0 < 0.1);
    if(dtime() - d0 >= 0.1) return TRANS_TIMEOUT;
    if(chr != last_chksum) return TRANS_BADCHSUM;
    return TRANS_SUCCEED;
}


/**
 * wait for answer (not more than 0.1s)
 * @param rdata (o)  - readed data
 * @param rdlen (o)  - its length (static array - THREAD UNSAFE)
 * @return transaction status
 */
trans_status wait4answer(uint8_t **rdata, int *rdlen){
    *rdlen = 0;
    static uint8_t buf[128];
    int L = 0;
    trans_status st = wait_checksum();
    if(st != TRANS_SUCCEED) return st;
    double d0 = dtime();
    do{
        if((L = read_tty(buf, sizeof(buf)))) break;
    }while(dtime() - d0 < 0.1);
    if(!L) return TRANS_TIMEOUT;
    *rdata = buf;
    *rdlen = L;
    return TRANS_SUCCEED;
}

/**
 * Try to connect to `device` at different speeds
 * @return connection speed if success or 0
 */
int try_connect(char *device){
    if(!device) return 0;
    green(_("Connecting to %s... "), device);
    for(curspd = 0; curspd < speedssize; ++curspd){
        tty_init(device, Bspeeds[curspd]);
        DBG("Try speed %d", speeds[curspd]);
        int ctr;
        for(ctr = 0; ctr < 10; ++ctr){ // 10 tries to send data
            if(send_cmd(CMD_COMM_TEST)) continue;
            else break;
        }
        if(ctr == 10) continue; // error sending data
        uint8_t *rd;
        int l;
        // OK, now check an answer
        if(TRANS_SUCCEED != wait4answer(&rd, &l) || l != 1 || *rd != ANS_COMM_TEST) continue;
        DBG("Got it!");
        green(_("Connection established at B%d.\n"), speeds[curspd]);
        return speeds[curspd];
    }
    green(_("No connection!"));
    return 0;
}

/**
 * run terminal emulation: send user's commands with checksum and show answers
 */
void run_terminal(){
    green(_("Work in terminal mode without echo\n"));
    int rb;
    uint8_t buf[BUFLEN];
    size_t L;
    setup_con();
    while(1){
        if((L = read_tty(buf, BUFLEN))){
            printf(_("Get data: "));
            uint8_t *ptr = buf;
            while(L--){
                uint8_t c = *ptr++;
                printf("0x%02x", c);
                if(c > 31) printf("(%c)", (char)c);
                printf(" ");
            }
            printf("\n");
        }
        if((rb = read_console())){
            printf("Send command: %c ... ", (char)rb);
            send_cmd((uint8_t)rb);
            if(TRANS_SUCCEED != wait_checksum()) printf(_("Error.\n"));
            else printf(_("Done.\n"));
        }
    }
}

/**
 * Run as daemon
 */
void daemonize(){
}
