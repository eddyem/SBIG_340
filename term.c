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
#ifndef CLIENT

#include "usefull_macros.h"
#include "term.h"
#include <strings.h> // strncasecmp
#include <time.h>    // time(NULL)

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
int send_data(uint8_t *buf, int len){
    if(len < 1) return 1;
    uint8_t chksum = 0, *ptr = buf;
    int l;
    for(l = 0; l < len; ++l)
        chksum ^= ~(*ptr++) & 0x7f;
    DBG("send: %s (chksum: 0x%X)", buf, chksum);
    if(write_tty(buf, len)) return 1;
    DBG("cmd sent");
    if(write_tty(&chksum, 1)) return 1;
    DBG("checksum sent");
    last_chksum = chksum;
    return 0;
}
int send_cmd(uint8_t cmd){
    uint8_t s[2];
    s[0] = cmd;
    s[1] = ~(cmd) & 0x7f;
    DBG("Write %c", cmd);
    if(write_tty(s, 2)) return 1;
    last_chksum = s[1];
    return 0;
}

/**
 * Wait for answer with checksum
 */
trans_status wait_checksum(){
    uint8_t chr;
    int r;
    double d0 = dtime();
    do{
        if((r = read_tty(&chr, 1)) && chr == last_chksum) break;
        //DBG("wait..");
    }while(dtime() - d0 < WAIT_TMOUT);
    if(dtime() - d0 >= WAIT_TMOUT) return TRANS_TIMEOUT;
    DBG("chksum: got 0x%x, need 0x%x", chr, last_chksum);
    if(chr != last_chksum){
        if(chr == 0x7f) return TRANS_TRYAGAIN;
        else if(chr == ANS_EXP_IN_PROGRESS) return TRANS_BUSY;
        else return TRANS_BADCHSUM;
    }
    return TRANS_SUCCEED;
}

/**
 * send command and wait for checksum
 * @return TRANS_SUCCEED if all OK
 */
trans_status send_cmd_cs(uint8_t cmd){
    if(send_cmd(cmd)) return TRANS_ERROR;
    return wait_checksum();
}

static int download_in_progress = 0; // == 1 when image downloading runs
/**
 * Abort image exposition
 * Used only on exit, so don't check commands status
 */
void abort_image(){
    uint8_t tmpbuf[4096];
    if(download_in_progress){
        read_tty(tmpbuf, 4096);
        send_cmd(IMTRANS_STOP);
        download_in_progress = 0;
    }
    read_tty(tmpbuf, 4096);
    send_cmd_cs(CMD_ABORT_IMAGE);
    read_tty(tmpbuf, 4096);
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
    if(rdlen) *rdlen = 0;
    static uint8_t buf[128];
    int L = 0;
    trans_status st = wait_checksum();
    if(st != TRANS_SUCCEED) return st;
    double d0 = dtime();
    do{
        if((L = read_tty(buf, sizeof(buf)))) break;
    }while(dtime() - d0 < WAIT_TMOUT);
    DBG("read %d bytes, first: 0x%x",L, buf[0]);
    if(!L) return TRANS_TIMEOUT;
    if(rdata) *rdata = buf;
    if(rdlen) *rdlen = L;
    return TRANS_SUCCEED;
}

/**
 * check if given baudrate right
 * @return its number in `speeds` array or -1 if fault
 */
int chkspeed(int speed){
    int spdidx = 0;
    for(; spdidx < speedssize; ++spdidx)
        if(speeds[spdidx] == speed) break;
    if(spdidx == speedssize){
        WARNX(_("Wrong speed: %d!"), speed);
        list_speeds();
        return -1;
    }
    return spdidx;
}

/**
 * Try to connect to `device` at given speed (or try all speeds, if speed == 0)
 * @return connection speed if success or 0
 */
int try_connect(char *device, int speed){
    if(!device) return 0;
    int spdstart = 0, spdmax = speedssize;
    if(speed){
        if((spdstart = chkspeed(speed)) < 0) return 0;
        spdmax = spdstart + 1;
    }
    uint8_t tmpbuf[4096];
    green(_("Connecting to %s... "), device);
    for(curspd = spdstart; curspd < spdmax; ++curspd){
        tty_init(device, Bspeeds[curspd]);
        read_tty(tmpbuf, 4096); // clear rbuf
        DBG("Try speed %d", speeds[curspd]);
        int ctr;
        for(ctr = 0; ctr < 10; ++ctr){ // 10 tries to send data
            read_tty(tmpbuf, 4096); // clear rbuf
            if(send_cmd(CMD_COMM_TEST)) continue;
            else break;
        }
        if(ctr == 10) continue; // error sending data
        uint8_t *rd;
        int l;
        // OK, now check an answer
        trans_status st = wait4answer(&rd, &l);
        DBG("st: %d", st);
        if(st == TRANS_BUSY){ // busy - send command 'abort exp'
            send_cmd(CMD_ABORT_IMAGE);
            --curspd;
            continue;
        }
        if(st == TRANS_TRYAGAIN){ // there was an error in last communications - try again
            --curspd;
            continue;
        }
        if(TRANS_SUCCEED != st || l != 1 || *rd != ANS_COMM_TEST) continue;
        DBG("Got it!");
        green(_("Connection established at B%d.\n"), speeds[curspd]);
        return speeds[curspd];
    }
    green(_("No connection!\n"));
    return 0;
}

/**
 * Change terminal speed to `speed`
 * @return 0 if all OK
 */
int term_setspeed(int speed){
    size_t L;
    int spdidx = chkspeed(speed);
    if(spdidx < 0) return 1;
    if(spdidx == curspd){
        printf(_("Already connected at %d\n"), speeds[spdidx]);
        return 0;
    }
    green(_("Try to change speed to %d\n"), speed);
    uint8_t msg[7] = {CMD_CHANGE_BAUDRATE, spdidx + '0'};
    if(send_data(msg, 2)){
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
            printf(_("Get %zd bytes: "), L);
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

void heater(heater_cmd cmd){
    if(cmd == HEATER_LEAVE) return;
    uint8_t buf[2] = {CMD_HEATER, 0};
    if(cmd == HEATER_ON) buf[1] = 1;
    int i;
    for(i = 0; i < 10 && send_data(buf, 2); ++i);
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
    if(TRANS_SUCCEED != send_cmd(CMD_FIRMWARE_VERSION)) return NULL;
    if(TRANS_SUCCEED != wait_checksum()) return NULL;
    uint8_t V[2];
    if(2 != read_string(V, 2)) return NULL;
    snprintf(buf, 256, "%c%d.%d", (V[0] &0x80)?'T':'V', V[0]&0x7f, V[1]);
    return buf;
}

/**
 * Send command to shutter
 * @param cmd (i) - command (register-independent): o - open, c - close, k - de-energize
 * cmd may include 'k' with 'o' or 'c' (means "open/close and de-energize")
 * @return 1 in case of wrong command
 */
int shutter_command(char *cmd){
    if(!cmd) return 1;
    int deenerg = 0, openclose = 0, N = 0;
    while(*cmd){
        char c = *cmd++;
        if(N > 2) return 1; // too much commands
        if(c == 'o' || c == 'O'){
            ++N; if(openclose) return 1; // already meet 'o' or 'c'
            openclose = 1; // open
        }else if(c == 'c' || c == 'C'){
            ++N; if(openclose) return 1;
            openclose = -1; // close
        }else if(c == 'k' || c == 'K'){
            ++N; deenerg = 1;
        }
        else if(c != '\'' && c != '"') return 1; // wrong symbol in command
    }
    if(openclose){
        if(TRANS_SUCCEED != send_cmd_cs(openclose > 0 ? CMD_SHUTTER_OPEN : CMD_SHUTTER_CLOSE))
            return 1;
    }
    if(deenerg){
       if(TRANS_SUCCEED != send_cmd_cs(CMD_SHUTTER_DEENERGIZE))
            return 1;
    }
    return 0;
}

/**
 * Define subframe region
 * TODO: test this function. It doesnt work
 * @param parm (i) - parameters in format Xstart,Ystart,size
 * `parm` can be Xstart,Ystart for default size (127px)
 * @return structure allocated here (should be free'd outside)
 */
imsubframe *define_subframe(char *parm){
    if(!parm) return NULL;
    // default parameters
    uint16_t X = 0, Y = 0;
    uint8_t sz = 127;
    char *eptr;
    long int L = strtol(parm, &eptr, 10);
    DBG("L=%ld, parm=%s, eptr=%s",L,parm,eptr);
    if(eptr == parm || !*eptr || *eptr != ','){
        WARNX(_("Subframe parameter should have format Xstart,Ystart,size or Xstart,Ystart when size=127"));
        return NULL;
    }
    if(L > IMWIDTH - 1 || L < 1){
        WARNX(_("Xstart should be in range 1..%d"), IMWIDTH - 1 );
        return NULL;
    }
    parm = eptr + 1;
    X = (uint16_t)L;
    L = strtol(parm, &eptr, 10);
    if(eptr == parm){
        WARNX(_("Wrong Ystart format"));
        return NULL;
    }
    if(L > IMHEIGHT - 1 || L < 1){
        WARNX(_("Ystart should be in range 1..%d"), IMHEIGHT - 1 );
        return NULL;
    }
    Y = (uint16_t)L;
    if(*eptr){
        if(*eptr != ','){
            WARNX(_("Wrong size format"));
            return NULL;
        }
        parm = eptr + 1;
        L = strtol(parm, &eptr, 10);
        if(L > MAX_SUBFRAME_SZ || L < 1){
            WARNX(_("Subframe size could be in range 1..%d"), MAX_SUBFRAME_SZ);
            return NULL;
        }
        sz = (uint8_t)L;
    }
    if(X+sz > IMWIDTH){
        WARNX(_("Xstart+size should be less or equal %d"), IMWIDTH);
        return NULL;
    }
    if(Y+sz > IMHEIGHT){
        WARNX(_("Ystart+size should be less or equal %d"), IMHEIGHT);
        return NULL;
    }
    // now all OK, send command
    uint8_t cmd[6] = {CMD_DEFINE_SUBFRAME, 0};
    cmd[1] = (X>>8) & 0xff;
    cmd[2] = X & 0xff;
    cmd[3] = (Y>>8) & 0xff;
    cmd[4] = Y & 0xff;
    cmd[5] = sz;
    if(send_data(cmd, 6)){
        WARNX(_("Error sending command"));
        return NULL;
    }
    wait4answer(NULL, NULL);
    // ALL OK!
    imsubframe *F = MALLOC(imsubframe, 1);
    F->Xstart = X, F->Ystart = Y, F->size = sz;
    return F;
}

/**
 * Send command to start exposition
 * @param binning - binning to expose
 * @param exptime - exposition time
 * @param imtype  - autodark, light or dark
 * @return 0 if all OK
 */
int start_exposition(imstorage *im, char *imtype){
    FNAME();
    double exptime = im->exptime;
    uint64_t exp100us = exptime * 10000.;
    static uint8_t cmd[6] = {CMD_TAKE_IMAGE}; // `static` to save all data after first call
    int binning = im->binning;
    image_type it = IMTYPE_AUTODARK;
    const char *m = "autodark";
    if(exptime < 5e-5){// 50us
        WARNX(_("Exposition time should be not less than 50us"));
        return 1;
    }
    DBG("exp: %lu", exp100us);
    cmd[1] = (exp100us >> 16) & 0xff;
    cmd[2] = (exp100us >> 8) & 0xff;
    cmd[3] = exp100us & 0xff;
    if(exp100us > MAX_EXPTIME_100){
        WARNX(_("Exposition time too large! Max value: %gs"), ((double)MAX_EXPTIME_100)/10000.);
        return 2;
    }
    const char *bngs[] = {"full", "cropped", "binned 2x2"};
    const char *b;
    if(binning != 0xff){ // check binning for non-subframe
        if(binning > 2 || binning < 0){
            WARNX(_("Bad binning size: %d, should be 0 (full), 1 (crop) or 2 (binned)"), binning);
            return 3;
        }
        b = bngs[binning];
    }else b = "subframe";
    cmd[4] = binning;
    // and now check image type
    if(imtype){
        int L = strlen(imtype);
        if(!L){ WARNX(_("Empty image type")); return 4;}
        if(0 == strncasecmp(imtype, "autodark", L)){
            if(binning == 0){
                WARNX(_("Auto dark mode don't support full image"));
                return 5;
            }
            cmd[5] = 2;}
        else if(0 == strncasecmp(imtype, "dark", L)) { cmd[5] = 0; m = "dark";  it = IMTYPE_DARK; }
        else if(0 == strncasecmp(imtype, "light", L)){ cmd[5] = 1; m = "light"; it = IMTYPE_LIGHT;}
        else{
            WARNX(_("Wrong image type: %s, should be \"autodark\", \"light\" or \"dark\""), imtype);
            return 6;
        }
    }else{
        it = im->imtype;
        if(it == IMTYPE_DARK) m = "dark";
        else if(it == IMTYPE_LIGHT) m = "light";
    }
    if(it != IMTYPE_DARK){
        if(shutter_command("ok")){ // open shutter
            WARNX(_("Can't open shutter"));
            return 8;
        }
    }
    green("Start expose for %g seconds, mode \"%s\", %s image\n", exptime, m, b);
    if(send_data(cmd, 6)){
        WARNX(_("Error sending command"));
        return 7;
    }
    DBG("send: %c %u %u %u %u %u", cmd[0], cmd[1],cmd[2],cmd[3],cmd[4],cmd[5]);
    if(TRANS_SUCCEED != wait_checksum()){
        WARNX(_("Didn't get the respond"));
        return 8;
    }
    im->imtype = it;
    size_t W, H;
    switch(im->binning){ // set image size
        case 1: // cropped
            W = IM_CROPWIDTH;
            H = IMHEIGHT;
        break;
        case 2: // binned
            W = IMWIDTH / 2;
            H = IMHEIGHT / 2;
        break;
        case 0xff: // subframe
            W = H = im->subframe->size;
            DBG("subfrsz: %d", im->subframe->size);
        break;
        case 0: // full image
        default:
            W = IMWIDTH;
            H = IMHEIGHT;
    }
    im->W = W; im->H = H;
    DBG("W=%zd, H=%zd\n", im->W, im->H);
    im->exposetime = time(NULL);
    return 0;
}

static char indi[] = "|/-\\";
/**
 * Wait till image ready
 * @return 0 if all OK
 */
int wait4image(){
    uint8_t rd = 0;
    char *iptr = indi;
    int stage = 1; // 1 - exp in progress, 2 - readout, 3 - done
    printf("\nExposure in progress  ");
    fflush(stdout);
    while(rd != ANS_EXP_DONE){
        int L = 0;
        double d0 = dtime();
        do{
            if((L = read_tty(&rd, 1))) break;
        }while(dtime() - d0 < EXP_DONE_TMOUT);
        if(!L){
            printf("\n");
            WARNX(_("CCD not answer"));
            return 1;
        }
        int nxtstage = 1;
        if(rd != ANS_EXP_IN_PROGRESS){
            if(rd == ANS_RDOUT_IN_PROGRESS) nxtstage = 2;
            else nxtstage = 3;
        }
        if(nxtstage == stage){
            printf("\b%c", *iptr++); // rotating line
            fflush(stdout);
            if(!*iptr) iptr = indi;
        }else{
            stage = nxtstage;
            if(stage == 2){
                printf(_("\nReadout  "));
                fflush(stdout);
            }else printf(_("\nDone!\n"));
        }
    }
    return 0;
}

/**
 * Collect data by serial terminal
 * @param img - parameters of exposed image
 * @return array with image data (allocated here) or NULL
 */
uint16_t *get_image(imstorage *img){
    char *iptr = indi;
    size_t L = img->W * img->H, rest = L * sizeof(uint16_t); // rest is datasize in bytes
    DBG("L = %zd, W=%zd, H=%zd", L, img->W, img->H);
    uint16_t *buff = MALLOC(uint16_t, L);
    if(TRANS_SUCCEED != send_cmd_cs(CMD_XFER_IMAGE)){
        WARNX(_("Error sending transfer command"));
        FREE(buff);
        return NULL;
    }
    download_in_progress = 1;
    #ifdef EBUG
    double tstart = dtime();
    #endif
    DBG("rest = %zd", rest);
    uint8_t *getdataportion(uint8_t *start, size_t l){ // return last byte read + 1
        int i;
        uint8_t cs = 0;
        for(i = 0; i < 4; ++i){ // four tries to get datablock
            size_t r = 0, got = 0;
            uint8_t *ptr = start;
            double d0 = dtime();
            do{
                if((r = read_tty(ptr, l))){
                    d0 = dtime();
                    ptr += r;
                    got += r;
                    l -= r;
                }
            }while(l && dtime() - d0 < IMTRANS_TMOUT);
            //DBG("got: %zd, time: %g, l=%zd", got, dtime()-d0, l);
            if(l){
                cs = IMTRANS_STOP;
                write_tty(&cs, 1);
                return NULL; // nothing to read
            }
            --ptr; // *ptr is checksum
            while(start < ptr) cs ^= *start++;
            //DBG("got checksum: %x, calc: %x", *ptr, cs);
            if(*ptr == cs){ // all OK
                //DBG("Checksum good");
                cs = IMTRANS_CONTINUE;
                write_tty(&cs, 1);
                return ptr;
            }else{ // bad checksum
                DBG("Ask to resend data");
                cs = IMTRANS_RESEND;
                write_tty(&cs, 1);
            }
        }
        DBG("not reached");
        cs = IMTRANS_STOP;
        write_tty(&cs, 1);
        return NULL;
    }
    uint8_t *bptr = (uint8_t*) buff;
    //int i = 0;
    // size of single block: 4096 pix in full frame or 1x1bin mode, 1024 in binned mode, subfrmsize in subframe mode
    size_t dpsize = 4096*2 + 1;
    if(img->binning == 2) dpsize = 1024*2 + 1;
    else if(img->binning == 0xff) dpsize = 2*img->subframe->size + 1;
    printf("Transfer data  "); fflush(stdout);
    do{
        size_t need = (rest > dpsize) ? dpsize : rest + 1;
        //DBG("I want %zd bytes", need);
        printf("\b%c", *iptr++); // rotating line
        fflush(stdout);
        if(!*iptr) iptr = indi;
        uint8_t *ptr = getdataportion(bptr, need);
        if(!ptr){
            printf("\n");
            WARNX(_("Error receiving data"));
            FREE(buff);
            download_in_progress = 0;
            return NULL;
        }
        rest -= need - 1;
        //DBG("need: %zd", need);
        bptr = ptr;
    }while(rest);
    printf("\b Done!\n");
    DBG("Got full data packet, capture time: %.1f seconds", dtime() - tstart);
    download_in_progress = 0;
    return buff;
}


#endif // CLIENT
