/*
 *                                                                                                  geany_encoding=koi8-r
 * debayer.h
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
 *
 */

#pragma once
#ifndef __DEBAYER_H__
#define __DEBAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "imfunctions.h"
int write_debayer(imstorage *img, uint16_t black);
void modifytimestamp(const char *filename, imstorage *img);

#ifdef __cplusplus
}
#endif
#endif // __DEBAYER_H__
