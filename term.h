/*
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

/******************************** Commands definition ********************************/
// communications test
#define     CMD_COMM_TEST           'E'

/******************************** Answers definition ********************************/
#define     ANS_COMM_TEST           'O'

void run_terminal();
void daemonize();
int open_serial(char *dev);
int get_curspeed();
int try_connect(char *device);

#endif // __TERM_H__
