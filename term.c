/*                                                                                                  geany_encoding=koi8-r
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
static int speeds[] = {
    9600,
    19200,
    38400,
    57600,
    115200,
    230400,
    460800
};

void list_speeds(){
    green(_("Speeds available:\n"));
    for(int i = 0; i < speedssize; ++i)
        printf("\t%d\n", speeds[i]);
}

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
    DBG("cmd done");
    if(write_tty(&chksum, 1)) return 1;
    DBG("checksum done");
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
        DBG("wait..");
    }while(dtime() - d0 < WAIT_TMOUT);
    if(dtime() - d0 >= WAIT_TMOUT) return TRANS_TIMEOUT;
    DBG("chksum: got 0x%x, need 0x%x", chr, last_chksum);
    if(chr != last_chksum) return TRANS_BADCHSUM;
    return TRANS_SUCCEED;
}

/**
 * read string from terminal (with timeout)
 * @param str (o) - buffer for string
 * @param L       - its length
 * @return number of characters read
 */
size_t read_string(uint8_t *str, int L){
    size_t r = 0, l;
    uint8_t *ptr = str;
    double d0 = dtime();
    do{
        if((l = read_tty(ptr, L))){
            r += l; L -= l; ptr += l;
            d0 = dtime();
        }
    }while(dtime() - d0 < WAIT_TMOUT);
    return r;
}

/**
 * wait for answer (not more than WAIT_TMOUT seconds)
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
    }while(dtime() - d0 < WAIT_TMOUT);
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
 * Change terminal speed to `speed`
 * @return 0 if all OK
 */
int term_setspeed(int speed){
    int spdidx = 0;
    size_t L;
    for(; spdidx < speedssize; ++spdidx)
        if(speeds[spdidx] == speed) break;
    if(spdidx == speedssize){
        WARNX(_("Wrong speed: %d!"), speed);
        list_speeds();
        return 1;
    }
    if(spdidx == curspd){
        printf(_("Already connected at %d\n"), speeds[spdidx]);
        return 0;
    }
    green(_("Try to change speed to %d\n"), speed);
    uint8_t msg[7] = {CMD_CHANGE_BAUDRATE, spdidx + '0', 0};
    if(send_data(msg)){
        WARNX(_("Error during message send"));
        return 1;
    }
    if(TRANS_SUCCEED != wait_checksum()){
        WARNX(_("Bad checksum"));
        return 1;
    }
    tty_init(NULL, Bspeeds[spdidx]); // change speed & wait 'S' as answer
    double d0 = dtime();
    do{
        if((L = read_tty(msg, 1))){
            DBG("READ %c", msg[0]);
            if(ANS_CHANGE_BAUDRATE == msg[0])
                break;
        }
    }while(dtime() - d0 < WAIT_TMOUT);
    if(L != 1 || msg[0] != ANS_CHANGE_BAUDRATE){
        WARNX(_("Didn't receive the answer"));
        return 1;
    }
    // now send "Test" and wait for "TestOk":
    if(write_tty((const uint8_t*)"Test", 4)){
        WARNX(_("Error in communications"));
        return 1;
    }
    d0 = dtime();
    if((L = read_string(msg, 6))) msg[L] = 0;
    DBG("got %zd: %s", L, msg);
    if(L != 6 || strcmp((char*)msg, "TestOk")){
        WARNX(_("Received wrong answer!"));
        return 1;
    }
    if(write_tty((const uint8_t*)"k", 1)){
        WARNX(_("Error in communications"));
        return 1;
    }
    green(_("Speed changed!\n"));
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
            if(rb > 31){
                printf("Send command: %c ... ", (char)rb);
                send_cmd((uint8_t)rb);
                if(TRANS_SUCCEED != wait_checksum()) printf(_("Error.\n"));
                else printf(_("Done.\n"));
            }
        }
    }
}

/**
 * Run as daemon
 */
void daemonize(){
}

void heater(heater_cmd cmd){
    if(cmd == HEATER_LEAVE) return;
    uint8_t buf[3] = {CMD_HEATER, 0, 0};
    if(cmd == HEATER_ON) buf[1] = 1;
    int i;
    for(i = 0; i < 10 && send_data(buf); ++i);
    trans_status st = TRANS_TIMEOUT;
    if(i < 10) st = wait_checksum();
    if(i == 10 || st != TRANS_SUCCEED){
        WARNX(_("Can't send heater command: %s"), (st==TRANS_TIMEOUT) ? _("no answer") : _("bad checksum"));
    }
}

/**
 * @return static buffer with version string, for example, "V1.10" or "T2.15" ('T' means testing) or NULL
 */
char *get_firmvare_version(){
    static char buf[256];
    if(TRANS_SUCCEED != send_cmd(CMD_FIRMWARE_WERSION)) return NULL;
    if(TRANS_SUCCEED != wait_checksum()) return NULL;
    uint8_t V[2];
    if(2 != read_string(V, 2)) return NULL;
    snprintf(buf, 256, "%c%d.%d", (V[0] &0x80)?'T':'V', V[0]&0x7f, V[1]);
    return buf;
}
