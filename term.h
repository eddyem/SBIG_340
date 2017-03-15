/*                                                                                                  geany_encoding=koi8-r
 * term.h - functions to work with serial terminal
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
#pragma once
#ifndef __TERM_H__
#define __TERM_H__
#include "imfunctions.h"

// terminal timeout (seconds)
#define     WAIT_TMOUT      (0.2)
// timeout waitint 'D'
#define     EXP_DONE_TMOUT  (5.0)
// dataportion transfer timeout
#define     IMTRANS_TMOUT   (3.0)
// image size
#define     IMWIDTH         (640)
#define     IM_CROPWIDTH    (512)
#define     IMHEIGHT        (480)
#define     MAX_SUBFRAME_SZ (127)
// maximal expposition time in 100th of us
#define     MAX_EXPTIME_100 ((uint64_t)0x63ffff)

// communication errors
typedef enum{
    TRANS_SUCCEED = 0,  // no errors
    TRANS_ERROR,        // some error occured
    TRANS_BADCHSUM,     // wrong checksum
    TRANS_TIMEOUT,      // no data for 0.1s
    TRANS_TRYAGAIN,     // checksum return 0x7f - maybe try again?
    TRANS_BUSY          // image exposure in progress
} trans_status;

// change heater
typedef enum{
    HEATER_LEAVE = 0,   // leave unchanged
    HEATER_ON,          // turn on
    HEATER_OFF          // turn off
} heater_cmd;

/******************************** Commands definition ********************************/
// communications test
#define     CMD_COMM_TEST           'E'
#define     CMD_HEATER              'g'
#define     CMD_CHANGE_BAUDRATE     'B'
#define     CMD_FIRMWARE_VERSION    'V'
#define     CMD_SHUTTER_OPEN        'O'
#define     CMD_SHUTTER_CLOSE       'C'
#define     CMD_SHUTTER_DEENERGIZE  'K'
#define     CMD_DEFINE_SUBFRAME     'S'
#define     CMD_TAKE_IMAGE          'T'
#define     CMD_ABORT_IMAGE         'A'
#define     CMD_XFER_IMAGE          'X'

/******************************** Image transfer ********************************/
#define     IMTRANS_CONTINUE        'K'
#define     IMTRANS_RESEND          'R'
#define     IMTRANS_STOP            'S'

/******************************** Answers definition ********************************/
#define     ANS_COMM_TEST           'O'
#define     ANS_CHANGE_BAUDRATE     'S'
#define     ANS_EXP_IN_PROGRESS     'E'
#define     ANS_RDOUT_IN_PROGRESS   'R'
#define     ANS_EXP_DONE            'D'

void run_terminal();
int open_serial(char *dev);
int get_curspeed();
int try_connect(char *device, int speed);
void heater(heater_cmd cmd);
void list_speeds();
void abort_image();
int term_setspeed(int speed);
char *get_firmvare_version();
int shutter_command(char *cmd);
imsubframe *define_subframe(char *parm);
int start_exposition(imstorage *im, char *imtype);
int wait4image();
uint16_t *get_image(imstorage *img);

#endif // __TERM_H__
