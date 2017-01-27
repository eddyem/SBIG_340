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

// communication errors
typedef enum{
    TRANS_SUCCEED = 0,  // no errors
    TRANS_BADCHSUM,     // wrong checksum
    TRANS_TIMEOUT       // no data for 0.1s
} trans_status;

// change heater
typedef enum{
    HEATER_LEAVE = 0,   // leave unchanged
    HEATER_ON,          // turn on
    HEATER_OFF          // turn off
} heater_cmd;

// terminal timeout (seconds)
#define     WAIT_TMOUT      (0.1)
/******************************** Commands definition ********************************/
// communications test
#define     CMD_COMM_TEST           'E'
#define     CMD_HEATER              'g'
#define     CMD_CHANGE_BAUDRATE     'B'
#define     CMD_FIRMWARE_WERSION    'V'

/******************************** Answers definition ********************************/
#define     ANS_COMM_TEST           'O'
#define     ANS_CHANGE_BAUDRATE     'S'

void run_terminal();
void daemonize();
int open_serial(char *dev);
int get_curspeed();
int try_connect(char *device);
void heater(heater_cmd cmd);
void list_speeds();
int term_setspeed(int speed);
char *get_firmvare_version();

#endif // __TERM_H__
