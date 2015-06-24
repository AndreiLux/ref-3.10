/* linux/drivers/video/exynos_decon/panel/s6e3ha2_wqhd_aid_dimming.h
*
* Header file for Samsung AID Dimming Driver.
*
* Copyright (c) 2013 Samsung Electronics
* Minwoo Kim <minwoo7945.kim@samsung.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __S6E3HA2_WQHD_AID_DIMMING_H__
#define __S6E3HA2_WQHD_AID_DIMMING_H__

// start of aid sheet in rev.E opmanual
static  signed char HA2_RErtbl2nit[10] = { 0, 29, 27, 26, 21, 18, 14, 11, 7, 0 };
static  signed char HA2_RErtbl3nit[10] = { 0, 27, 26, 24, 20, 17, 14, 11, 7, 0 };
static  signed char HA2_RErtbl4nit[10] = { 0, 31, 24, 21, 17, 15, 14, 11, 7, 0 };
static  signed char HA2_RErtbl5nit[10] = { 0, 28, 24, 20, 17, 15, 14, 11, 7, 0 };
static  signed char HA2_RErtbl6nit[10] = { 0, 25, 23, 20, 16, 15, 14, 11, 7, 0 };
static  signed char HA2_RErtbl7nit[10] = { 0, 25, 23, 19, 15, 14, 12, 11, 7, 0 };
static  signed char HA2_RErtbl8nit[10] = { 0, 25, 21, 18, 13, 13, 12, 10, 7, 0 };
static  signed char HA2_RErtbl9nit[10] = { 0, 22, 21, 17, 13, 12, 12, 10, 7, 0 };
static  signed char HA2_RErtbl10nit[10] = { 0, 22, 20, 16, 12, 11, 11, 9, 7, 0 };
static  signed char HA2_RErtbl11nit[10] = { 0, 22, 19, 15, 11, 11, 10, 9, 6, 0 };
static  signed char HA2_RErtbl12nit[10] = { 0, 17, 19, 15, 11, 10, 10, 8, 6, 0 };
static  signed char HA2_RErtbl13nit[10] = { 0, 17, 18, 14, 10, 10, 9, 7, 6, 0 };
static  signed char HA2_RErtbl14nit[10] = { 0, 17, 17, 14, 10, 10, 9, 8, 6, 0 };
static  signed char HA2_RErtbl15nit[10] = { 0, 17, 17, 14, 10, 10, 8, 8, 6, 0 };
static  signed char HA2_RErtbl16nit[10] = { 0, 17, 16, 12, 10, 9, 8, 8, 6, 0 };
static  signed char HA2_RErtbl17nit[10] = { 0, 17, 16, 12, 9, 9, 8, 7, 6, 0 };
static  signed char HA2_RErtbl19nit[10] = { 0, 14, 15, 11, 8, 8, 7, 7, 6, 0 };
static  signed char HA2_RErtbl20nit[10] = { 0, 14, 14, 11, 8, 8, 7, 7, 6, 0 };
static  signed char HA2_RErtbl21nit[10] = { 0, 14, 14, 10, 8, 8, 7, 7, 6, 0 };
static  signed char HA2_RErtbl22nit[10] = { 0, 14, 14, 10, 8, 8, 7, 7, 6, 0 };
static  signed char HA2_RErtbl24nit[10] = { 0, 14, 13, 9, 7, 7, 7, 7, 6, 0 };
static  signed char HA2_RErtbl25nit[10] = { 0, 14, 13, 8, 6, 6, 6, 7, 6, 0 };
static  signed char HA2_RErtbl27nit[10] = { 0, 14, 12, 8, 6, 6, 6, 7, 6, 0 };
static  signed char HA2_RErtbl29nit[10] = { 0, 11, 12, 8, 6, 6, 7, 7, 6, 0 };
static  signed char HA2_RErtbl30nit[10] = { 0, 11, 12, 8, 6, 6, 6, 7, 6, 0 };
static  signed char HA2_RErtbl32nit[10] = { 0, 10, 11, 8, 6, 6, 6, 6, 5, 0 };
static  signed char HA2_RErtbl34nit[10] = { 0, 10, 11, 7, 5, 5, 5, 6, 5, 0 };
static  signed char HA2_RErtbl37nit[10] = { 0, 10, 10, 7, 5, 5, 5, 6, 5, 0 };
static  signed char HA2_RErtbl39nit[10] = { 0, 10, 10, 7, 5, 5, 5, 6, 5, 0 };
static  signed char HA2_RErtbl41nit[10] = { 0, 10, 9, 7, 5, 5, 5, 5, 5, 0 };
static  signed char HA2_RErtbl44nit[10] = { 0, 9, 9, 6, 4, 4, 5, 5, 5, 0 };
static  signed char HA2_RErtbl47nit[10] = { 0, 7, 8, 6, 4, 4, 5, 5, 5, 0 };
static  signed char HA2_RErtbl50nit[10] = { 0, 6, 8, 6, 4, 4, 5, 5, 5, 0 };
static  signed char HA2_RErtbl53nit[10] = { 0, 6, 8, 5, 3, 4, 5, 5, 5, 0 };
static  signed char HA2_RErtbl56nit[10] = { 0, 6, 7, 5, 3, 4, 4, 6, 5, 0 };
static  signed char HA2_RErtbl60nit[10] = { 0, 4, 7, 5, 3, 4, 5, 5, 5, 0 };
static  signed char HA2_RErtbl64nit[10] = { 0, 4, 7, 5, 3, 3, 4, 6, 5, 0 };
static  signed char HA2_RErtbl68nit[10] = { 0, 4, 6, 5, 3, 3, 4, 5, 5, 0 };
static  signed char HA2_RErtbl72nit[10] = { 0, 3, 6, 4, 2, 3, 4, 5, 4, 0 };
static  signed char HA2_RErtbl77nit[10] = { 0, 3, 5, 3, 3, 2, 3, 6, 4, 0 };
static  signed char HA2_RErtbl82nit[10] = { 0, 1, 6, 4, 3, 3, 4, 5, 3, 0 };
static  signed char HA2_RErtbl87nit[10] = { 0, 3, 5, 4, 3, 3, 4, 4, 3, 0 };
static  signed char HA2_RErtbl93nit[10] = { 0, 2, 7, 5, 4, 4, 5, 5, 2, 0 };
static  signed char HA2_RErtbl98nit[10] = { 0, 1, 5, 3, 2, 2, 4, 5, 3, 0 };
static  signed char HA2_RErtbl105nit[10] = { 0, 2, 5, 3, 3, 3, 4, 5, 2, 0 };
static  signed char HA2_RErtbl111nit[10] = { 0, 2, 5, 3, 2, 3, 4, 4, 2, 0 };
static  signed char HA2_RErtbl119nit[10] = { 0, 2, 4, 2, 2, 2, 4, 5, 0, 0 };
static  signed char HA2_RErtbl126nit[10] = { 0, 3, 3, 3, 2, 2, 3, 5, 0, 0 };
static  signed char HA2_RErtbl134nit[10] = { 0, 2, 4, 2, 2, 2, 3, 5, 1, 0 };
static  signed char HA2_RErtbl143nit[10] = { 0, 3, 6, 3, 2, 4, 4, 5, 1, 0 };
static  signed char HA2_RErtbl152nit[10] = { 0, 1, 3, 2, 2, 2, 5, 5, 1, 0 };
static  signed char HA2_RErtbl162nit[10] = { 0, 1, 2, 1, 2, 2, 3, 3, 1, 0 };
static  signed char HA2_RErtbl172nit[10] = { 0, 0, 3, 2, 2, 3, 4, 4, 2, 0 };
static  signed char HA2_RErtbl183nit[10] = { 0, 3, 3, 1, 1, 2, 4, 4, 1, 0 };
static  signed char HA2_RErtbl195nit[10] = { 0, 4, 2, 2, 1, 1, 4, 3, 0, 0 };
static  signed char HA2_RErtbl207nit[10] = { 0, 0, 2, 1, 1, 1, 2, 2, 0, 0 };
static  signed char HA2_RErtbl220nit[10] = { 0, 0, 2, 1, 1, 1, 2, 2, 0, 0 };
static  signed char HA2_RErtbl234nit[10] = { 0, 2, 1, 1, 1, 1, 2, 2, 0, 0 };
static  signed char HA2_RErtbl249nit[10] = { 0, 0, 1, 1, 0, 1, 2, 1, -1, 0 };
static  signed char HA2_RErtbl265nit[10] = { 0, 0, 1, 1, 1, 0, 1, 1, 0, 0 };
static  signed char HA2_RErtbl282nit[10] = { 0, 0, 0, 0, 0, 1, 0, 0, -1, 0 };
static  signed char HA2_RErtbl300nit[10] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0 };
static  signed char HA2_RErtbl316nit[10] = { 0, 0, 0, 0, -1, 0, 0, -1, 0, 0 };
static  signed char HA2_RErtbl333nit[10] = { 0, 0, 1, -1, -1, 0, 0, 0, 0, 0 };
static  signed char HA2_RErtbl360nit[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static  signed char HA2_REctbl2nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, 0, -4, 0, -4, -6, 1, -5, -9, 1, -4, -4, 0, -3, -3, 0, -1, -1, 0, -1, -7, 0, -6};
static  signed char HA2_REctbl3nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -2, -5, 0, -5, -6, 0, -6, -7, 1, -5, -5, 0, -3, -2, 0, -2, -2, 0, -1, -5, 0, -5};
static  signed char HA2_REctbl4nit[30] = {0, 0, 0, 0, 0, 0,-4, 0, -5, -5, 0, -4, -7, 1, -6, -6, 1, -5, -3, 0, -2, -2, 0, -1, -2, 0, -1, -4, 0, -5};
static  signed char HA2_REctbl5nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -6, -6, 1, -8, -7, 0, -5, -5, 1, -5, -3, 0, -2, -2, 0, -2, -1, 0, -1, -4, 0, -5};
static  signed char HA2_REctbl6nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -7, -5, 1, -5, -5, 1, -5, -4, 1, -5, -3, 0, -3, -2, 0, -1, -1, 0, -1, -3, 0, -4};
static  signed char HA2_REctbl7nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -6, -5, 0, -7, -5, 1, -5, -5, 1, -6, -3, 1, -3, -1, 0, -1, -1, 0, 0, -3, 0, -3};
static  signed char HA2_REctbl8nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -9, -4, 1, -5, -5, 1, -6, -5, 1, -6, -2, 1, -2, -2, 0, -1, -1, 0, -1, -3, 0, -3};
static  signed char HA2_REctbl9nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -7, -5, 1, -7, -5, 1, -5, -4, 1, -6, -3, 0, -3, -1, 0, -1, -1, 0, 0, -2, 0, -3};
static  signed char HA2_REctbl10nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -8, -4, 1, -7, -3, 1, -4, -5, 1, -6, -2, 0, -2, -2, 0, -1, 0, 0, -1, -2, 0, -2};
static  signed char HA2_REctbl11nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -10, -3, 1, -6, -5, 1, -7, -4, 1, -5, -3, 0, -3, -1, 0, 0, 0, 0, -1, -2, 0, -2};
static  signed char HA2_REctbl12nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -8, -3, 1, -5, -3, 1, -5, -3, 1, -5, -3, 0, -2, -1, 0, -1, -1, 0, 0, -1, 0, -2};
static  signed char HA2_REctbl13nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -7, -4, 1, -7, -4, 1, -7, -3, 1, -4, -2, 0, -2, -1, 0, 0, -1, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl14nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -9, -3, 1, -5, -5, 1, -7, -3, 1, -4, -3, 0, -2, -1, 0, -1, 0, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl15nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -9, -3, 1, -5, -5, 1, -7, -3, 1, -4, -3, 0, -2, -1, 0, -1, 0, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl16nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -9, -4, 1, -8, -3, 1, -6, -4, 1, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl17nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -10, -4, 1, -7, -3, 1, -7, -4, 1, -4, -3, 0, -2, -1, 0, 0, 0, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl19nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -10, -3, 1, -6, -2, 1, -5, -4, 0, -4, -1, 0, -2, -1, 0, 0, 0, 0, 0, -1, 0, -1};
static  signed char HA2_REctbl20nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -10, -2, 1, -5, -3, 1, -5, -3, 0, -4, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl21nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -4, 1, -8, -3, 1, -5, -4, 0, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl22nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -9, -4, 1, -7, -2, 1, -4, -3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl24nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -10, -5, 1, -8, -2, 1, -4, -3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl25nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -7, -4, 1, -7, -2, 1, -4, -2, 1, -2, -1, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl27nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -11, -4, 1, -7, -2, 1, -4, -2, 0, -3, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl29nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 1, -6, -3, 0, -4, -3, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1};
static  signed char HA2_REctbl30nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 1, -5, -3, 0, -4, -3, 0, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl32nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 1, -4, -2, 0, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl34nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 1, -7, -3, 0, -3, -3, 0, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl37nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -10, -3, 1, -6, -2, 0, -2, -3, 0, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl39nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -11, -3, 0, -6, -3, 0, -3, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl41nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -12, -4, 0, -6, -2, 0, -2, -2, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl44nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -9, -3, 0, -4, -2, 0, -2, -2, 0, -3, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -12, -3, 0, -4, -2, 0, -2, -1, 0, -2, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl50nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -10, -3, 0, -3, -2, 0, -2, -2, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 0, -5, -2, 0, -2, -2, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl56nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -11, -4, 0, -5, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl60nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -10, -3, 0, -4, -2, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -9, -3, 0, -4, 0, 0, -1, -1, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -10, -2, 0, -3, -1, 0, -2, 0, 0, -1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl72nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -10, -2, 0, -3, -1, 0, -2, 0, 0, -2, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl77nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -9, -3, 0, -4, 0, 0, -1, -1, 0, -2, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl82nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -8, -2, 0, -2, -2, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl87nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -10, -3, 0, -3, -1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -10, -2, 0, -4, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl98nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -7, -2, 0, -2, -1, 0, -1, 0, 0, 0, 1, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl105nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -8, -2, 0, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl111nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -7, -2, 0, -2, -1, 0, -1, -1, 0, -1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl119nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -6, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl126nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -7, -1, 0, -1, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl134nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -5, -1, 0, -1, -1, 0, 0, 0, 0, -1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl143nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -7, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl152nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -6, -1, 0, -2, 0, 0, 0, 0, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl162nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -6, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl172nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -5, 0, 0, 0, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl183nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0};
static  signed char HA2_REctbl195nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl207nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl220nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl234nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_REctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static  signed char HA2_aid9825[3] = { 0xB2, 0xE3, 0x90 };
static  signed char HA2_aid9713[3] = { 0xB2, 0xC6, 0x90 };
static  signed char HA2_aid9635[3] = { 0xB2, 0xB2, 0x90 };
static  signed char HA2_aid9523[3] = { 0xB2, 0x95, 0x90 };
static  signed char HA2_aid9445[3] = { 0xB2, 0x81, 0x90 };
static  signed char HA2_aid9344[3] = { 0xB2, 0x67, 0x90 };
static  signed char HA2_aid9270[3] = { 0xB2, 0x54, 0x90 };
static  signed char HA2_aid9200[3] = { 0xB2, 0x42, 0x90 };
static  signed char HA2_aid9130[3] = { 0xB2, 0x30, 0x90 };
static  signed char HA2_aid9030[3] = { 0xB2, 0x16, 0x90 };
static  signed char HA2_aid8964[3] = { 0xB2, 0x05, 0x90 };
static  signed char HA2_aid8894[3] = { 0xB2, 0xF3, 0x80 };
static  signed char HA2_aid8832[3] = { 0xB2, 0xE3, 0x80 };
static  signed char HA2_aid8723[3] = { 0xB2, 0xC7, 0x80 };
static  signed char HA2_aid8653[3] = { 0xB2, 0xB5, 0x80 };
static  signed char HA2_aid8575[3] = { 0xB2, 0xA1, 0x80 };
static  signed char HA2_aid8393[3] = { 0xB2, 0x72, 0x80 };
static  signed char HA2_aid8284[3] = { 0xB2, 0x56, 0x80 };
static  signed char HA2_aid8218[3] = { 0xB2, 0x45, 0x80 };
static  signed char HA2_aid8148[3] = { 0xB2, 0x33, 0x80 };
static  signed char HA2_aid7974[3] = { 0xB2, 0x06, 0x80 };
static  signed char HA2_aid7896[3] = { 0xB2, 0xF2, 0x70 };
static  signed char HA2_aid7717[3] = { 0xB2, 0xC4, 0x70 };
static  signed char HA2_aid7535[3] = { 0xB2, 0x95, 0x70 };
static  signed char HA2_aid7469[3] = { 0xB2, 0x84, 0x70 };
static  signed char HA2_aid7290[3] = { 0xB2, 0x56, 0x70 };
static  signed char HA2_aid7104[3] = { 0xB2, 0x26, 0x70 };
static  signed char HA2_aid6848[3] = { 0xB2, 0xE4, 0x60 };
static  signed char HA2_aid6669[3] = { 0xB2, 0xB6, 0x60 };
static  signed char HA2_aid6491[3] = { 0xB2, 0x88, 0x60 };
static  signed char HA2_aid6231[3] = { 0xB2, 0x45, 0x60 };
static  signed char HA2_aid5947[3] = { 0xB2, 0xFC, 0x50 };
static  signed char HA2_aid5675[3] = { 0xB2, 0xB6, 0x50 };
static  signed char HA2_aid5419[3] = { 0xB2, 0x74, 0x50 };
static  signed char HA2_aid5140[3] = { 0xB2, 0x2C, 0x50 };
static  signed char HA2_aid4752[3] = { 0xB2, 0xC8, 0x40 };
static  signed char HA2_aid4383[3] = { 0xB2, 0x69, 0x40 };
static  signed char HA2_aid4006[3] = { 0xB2, 0x08, 0x40 };
static  signed char HA2_aid3622[3] = { 0xB2, 0xA5, 0x30 };
static  signed char HA2_aid3133[3] = { 0xB2, 0x27, 0x30 };
static  signed char HA2_aid2620[3] = { 0xB2, 0xA3, 0x20 };
static  signed char HA2_aid2158[3] = { 0xB2, 0x2C, 0x20 };
static  signed char HA2_aid1611[3] = { 0xB2, 0x9F, 0x10 };
static  signed char HA2_aid1005[3] = { 0xB2, 0x03, 0x10 };

// end of aid sheet in rev.E opmanual


static  unsigned char HA2_elv1[3] = {
        0xB6, 0x8C, 0x16
};
static  unsigned char HA2_elv2[3] = {
        0xB6, 0x8C, 0x15
};
static  unsigned char HA2_elv3[3] = {
        0xB6, 0x8C, 0x14
};
static  unsigned char HA2_elv4[3] = {
        0xB6, 0x8C, 0x13
};
static  unsigned char HA2_elv5[3] = {
        0xB6, 0x8C, 0x12
};
static  unsigned char HA2_elv6[3] = {
        0xB6, 0x8C, 0x11
};
static  unsigned char HA2_elv7[3] = {
        0xB6, 0x8C, 0x10
};
static  unsigned char HA2_elv8[3] = {
        0xB6, 0x8C, 0x0F
};
static  unsigned char HA2_elv9[3] = {
        0xB6, 0x8C, 0x0E
};
static  unsigned char HA2_elv10[3] = {
        0xB6, 0x8C, 0x0D
};
static  unsigned char HA2_elv11[3] = {
        0xB6, 0x8C, 0x0C
};
static  unsigned char HA2_elv12[3] = {
        0xB6, 0x8C, 0x0B
};
static  unsigned char HA2_elv13[3] = {
        0xB6, 0x8C, 0x0A
};


static  unsigned char HA2_elvCaps1[3] = {
        0xB6, 0x9C, 0x16
};
static  unsigned char HA2_elvCaps2[3] = {
        0xB6, 0x9C, 0x15
};
static  unsigned char HA2_elvCaps3[3] = {
        0xB6, 0x9C, 0x14
};
static  unsigned char HA2_elvCaps4[3] = {
        0xB6, 0x9C, 0x13
};
static  unsigned char HA2_elvCaps5[3] = {
        0xB6, 0x9C, 0x12
};
static  unsigned char HA2_elvCaps6[3] = {
        0xB6, 0x9C, 0x11
};
static  unsigned char HA2_elvCaps7[3] = {
        0xB6, 0x9C, 0x10
};
static  unsigned char HA2_elvCaps8[3] = {
        0xB6, 0x9C, 0x0F
};
static  unsigned char HA2_elvCaps9[3] = {
        0xB6, 0x9C, 0x0E
};
static  unsigned char HA2_elvCaps10[3] = {
        0xB6, 0x9C, 0x0D
};
static  unsigned char HA2_elvCaps11[3] = {
        0xB6, 0x9C, 0x0C
};
static  unsigned char HA2_elvCaps12[3] = {
        0xB6, 0x9C, 0x0B
};
static  unsigned char HA2_elvCaps13[3] = {
        0xB6, 0x9C, 0x0A
};


#ifdef CONFIG_LCD_HMT

static  signed char HA2_HMTrtbl10nit[10] = { 0, 8, 10, 8, 6, 4, 2, 2, 1, 0 };
static  signed char HA2_HMTrtbl11nit[10] = { 0, 13, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HA2_HMTrtbl12nit[10] = { 0, 11, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HA2_HMTrtbl13nit[10] = { 0, 11, 9, 7, 5, 3, 2, 3, 0, 0 };
static  signed char HA2_HMTrtbl14nit[10] = { 0, 13, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HA2_HMTrtbl15nit[10] = { 0, 9, 10, 7, 6, 4, 4, 4, 1, 0 };
static  signed char HA2_HMTrtbl16nit[10] = { 0, 10, 10, 7, 5, 5, 3, 3, 1, 0 };
static  signed char HA2_HMTrtbl17nit[10] = { 0, 8, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HA2_HMTrtbl19nit[10] = { 0, 11, 9, 7, 5, 4, 3, 3, 2, 0 };
static  signed char HA2_HMTrtbl20nit[10] = { 0, 10, 9, 7, 5, 5, 4, 3, 2, 0 };
static  signed char HA2_HMTrtbl21nit[10] = { 0, 9, 9, 7, 5, 4, 4, 4, 3, 0 };
static  signed char HA2_HMTrtbl22nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 4, 0 };
static  signed char HA2_HMTrtbl23nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 3, 0 };
static  signed char HA2_HMTrtbl25nit[10] = { 0, 12, 9, 7, 5, 5, 4, 4, 4, 0 };
static  signed char HA2_HMTrtbl27nit[10] = { 0, 7, 9, 6, 5, 4, 5, 4, 3, 0 };
static  signed char HA2_HMTrtbl29nit[10] = { 0, 10, 9, 7, 5, 5, 5, 7, 5, 0 };
static  signed char HA2_HMTrtbl31nit[10] = { 0, 8, 9, 7, 5, 5, 5, 5, 4, 0 };
static  signed char HA2_HMTrtbl33nit[10] = { 0, 10, 9, 6, 6, 5, 6, 7, 4, 0 };
static  signed char HA2_HMTrtbl35nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 4, 0 };
static  signed char HA2_HMTrtbl37nit[10] = { 0, 6, 9, 6, 5, 5, 6, 6, 2, 0 };
static  signed char HA2_HMTrtbl39nit[10] = { 0, 6, 9, 6, 5, 5, 6, 5, 2, 0 };
static  signed char HA2_HMTrtbl41nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 3, 0 };
static  signed char HA2_HMTrtbl44nit[10] = { 0, 6, 10, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HA2_HMTrtbl47nit[10] = { 0, 8, 10, 7, 5, 5, 5, 5, 1, 0 };
static  signed char HA2_HMTrtbl50nit[10] = { 0, 8, 10, 7, 6, 6, 6, 5, 0, 0 };
static  signed char HA2_HMTrtbl53nit[10] = { 0, 9, 9, 7, 6, 6, 6, 6, 0, 0 };
static  signed char HA2_HMTrtbl56nit[10] = { 0, 11, 9, 7, 5, 6, 6, 6, 2, 0 };
static  signed char HA2_HMTrtbl60nit[10] = { 0, 11, 9, 7, 5, 6, 6, 5, 1, 0 };
static  signed char HA2_HMTrtbl64nit[10] = { 0, 7, 10, 7, 6, 6, 6, 5, 1, 0 };
static  signed char HA2_HMTrtbl68nit[10] = { 0, 7, 9, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HA2_HMTrtbl72nit[10] = { 0, 7, 9, 6, 5, 5, 5, 6, 2, 0 };
static  signed char HA2_HMTrtbl77nit[10] = { 0, 6, 5, 4, 3, 3, 3, 4, -1, 0 };
static  signed char HA2_HMTrtbl82nit[10] = { 0, 3, 6, 4, 3, 3, 5, 5, 0, 0 };
static  signed char HA2_HMTrtbl87nit[10] = { 0, 1, 6, 4, 3, 2, 4, 5, 0, 0 };
static  signed char HA2_HMTrtbl93nit[10] = { 0, 3, 5, 3, 2, 3, 5, 4, 2, 0 };
static  signed char HA2_HMTrtbl99nit[10] = { 0, 3, 5, 3, 3, 3, 4, 4, 1, 0 };
static  signed char HA2_HMTrtbl105nit[10] = { 0, 6, 5, 3, 3, 3, 4, 4, 1, 0 };


static  signed char HA2_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -3, 3, -7, -3, 2, -5, -4, 2, -5, -1, 2, -4, -1, 0, -2, -1, 0, 0, 0, 0, -1};
static  signed char HA2_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -7, -4, 2, -6, -3, 2, -5, -2, 1, -4, -2, 0, -2, 0, 0, -1, 0, 0, 0};
static  signed char HA2_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -6, -4, 3, -7, -3, 3, -6, -3, 2, -4, -1, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -6, -3, 4, -9, -3, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -2, 3, -6, -3, 2, -5, -2, 2, -5, -1, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static  signed char HA2_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -5, 3, -8, -2, 2, -4, -2, 2, -4, -1, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -8, -4, 2, -5, -3, 2, -5, -1, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, -3, 3, -7, -4, 2, -5, -2, 2, -5, 0, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -8, -2, 3, -8, -5, 1, -4, -3, 2, -5, 0, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -8, -4, 3, -8, -3, 2, -4, -2, 2, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -3, 3, -8, -4, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -5, 3, -8, -2, 2, -4, -1, 2, -4, -2, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -8, -3, 2, -4, -1, 2, -4, -2, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -10, -4, 3, -7, -3, 1, -4, 0, 2, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA2_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -7, -4, 1, -4, -1, 1, -4, -1, 1, -2, -1, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 2, -6, -4, 1, -4, 0, 2, -4, -2, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -3, 3, -6, -3, 1, -4, -1, 2, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -3, 3, -7, -2, 1, -3, -1, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -4, 3, -6, -2, 1, -3, -1, 1, -4, -2, 0, -2, -1, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -8, -3, 3, -6, -4, 1, -4, -2, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -4, 2, -6, -3, 1, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA2_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -3, 3, -6, -3, 1, -4, -2, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -3, -1, 0, -2, 0, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA2_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -4, 2, -6, -2, 1, -4, -3, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -5, 2, -6, -2, 2, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA2_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 2, 0, 1};
static  signed char HA2_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 1, -4, -2, 2, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static  signed char HA2_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 2, -5, -2, 1, -4, -1, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -3, 2, -5, -2, 1, -4, -1, 0, -2, -1, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -1, 2, -4, -2, 1, -4, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HA2_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -2, 2, -4, -1, 1, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -9, -2, 1, -4, -2, 0, -2, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HA2_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -2, 1, -3, 0, 0, -2, -1, 0, -1, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static  signed char HA2_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HA2_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -2, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static  signed char HA2_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -7, -2, 1, -4, 0, 1, -3, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HA2_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -8, -4, 1, -4, 0, 1, -3, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};


static  unsigned char HA2_HMTaid8001[4] = {
        0xB2, 0x03, 0x12, 0x03
};

static  unsigned char HA2_HMTaid7003[4] = {
        0xB2, 0x03, 0x13, 0x04
};

static unsigned char HA2_HMTelv[3] = {
        0xB6, 0x8C, 0x0A
};
static unsigned char HA2_HMTelvCaps[3] = {
        0xB6, 0x9C, 0x0A
};

#endif

/********************************************** HA3 **********************************************/

// start of aid sheet in rev.B
static signed char HA3_rtbl2nit[10] = {0, 17, 17, 15, 13, 13, 13, 12, 7, 0};
static signed char HA3_rtbl3nit[10] = {0, 17, 18, 17, 15, 14, 14, 11, 7, 0};
static signed char HA3_rtbl4nit[10] = {0, 18, 17, 16, 13, 13, 13, 11, 7, 0};
static signed char HA3_rtbl5nit[10] = {0, 20, 18, 17, 14, 14, 14, 12, 7, 0};
static signed char HA3_rtbl6nit[10] = {0, 20, 18, 16, 13, 13, 13, 10, 6, 0};
static signed char HA3_rtbl7nit[10] = {0, 19, 18, 16, 13, 12, 11, 9, 6, 0};
static signed char HA3_rtbl8nit[10] = {0, 21, 17, 15, 13, 13, 12, 10, 6, 0};
static signed char HA3_rtbl9nit[10] = {0, 19, 17, 15, 13, 13, 12, 10, 6, 0};
static signed char HA3_rtbl10nit[10] = {0, 19, 17, 15, 13, 13, 11, 10, 6, 0};
static signed char HA3_rtbl11nit[10] = {0, 19, 16, 14, 12, 12, 11, 9, 6, 0};
static signed char HA3_rtbl12nit[10] = {0, 17, 16, 14, 11, 11, 10, 9, 6, 0};
static signed char HA3_rtbl13nit[10] = {0, 18, 16, 14, 11, 11, 10, 9, 6, 0};
static signed char HA3_rtbl14nit[10] = {0, 19, 15, 13, 11, 10, 9, 8, 6, 0};
static signed char HA3_rtbl15nit[10] = {0, 16, 15, 12, 10, 10, 9, 8, 6, 0};
static signed char HA3_rtbl16nit[10] = {0, 15, 15, 12, 10, 10, 8, 8, 6, 0};
static signed char HA3_rtbl17nit[10] = {0, 15, 15, 12, 10, 9, 8, 8, 5, 0};
static signed char HA3_rtbl19nit[10] = {0, 17, 13, 10, 9, 9, 8, 7, 5, 0};
static signed char HA3_rtbl20nit[10] = {0, 14, 13, 10, 8, 8, 7, 7, 5, 0};
static signed char HA3_rtbl21nit[10] = {0, 13, 13, 10, 8, 8, 7, 7, 5, 0};
static signed char HA3_rtbl22nit[10] = {0, 15, 12, 9, 8, 8, 7, 7, 5, 0};
static signed char HA3_rtbl24nit[10] = {0, 12, 12, 9, 7, 7, 6, 6, 5, 0};
static signed char HA3_rtbl25nit[10] = {0, 11, 12, 9, 7, 7, 6, 7, 5, 0};
static signed char HA3_rtbl27nit[10] = {0, 10, 11, 8, 7, 7, 6, 6, 5, 0};
static signed char HA3_rtbl29nit[10] = {0, 14, 10, 8, 6, 6, 6, 6, 5, 0};
static signed char HA3_rtbl30nit[10] = {0, 14, 10, 8, 6, 6, 5, 5, 4, 0};
static signed char HA3_rtbl32nit[10] = {0, 9, 10, 8, 6, 6, 5, 6, 5, 0};
static signed char HA3_rtbl34nit[10] = {0, 12, 9, 7, 6, 6, 5, 6, 5, 0};
static signed char HA3_rtbl37nit[10] = {0, 10, 9, 7, 5, 5, 5, 6, 5, 0};
static signed char HA3_rtbl39nit[10] = {0, 10, 8, 6, 5, 5, 5, 5, 5, 0};
static signed char HA3_rtbl41nit[10] = {0, 9, 8, 6, 5, 5, 5, 5, 5, 0};
static signed char HA3_rtbl44nit[10] = {0, 10, 7, 6, 4, 4, 4, 5, 4, 0};
static signed char HA3_rtbl47nit[10] = {0, 10, 7, 6, 4, 4, 4, 5, 4, 0};
static signed char HA3_rtbl50nit[10] = {0, 8, 7, 5, 4, 4, 4, 5, 4, 0};
static signed char HA3_rtbl53nit[10] = {0, 7, 7, 5, 4, 4, 4, 5, 4, 0};
static signed char HA3_rtbl56nit[10] = {0, 9, 6, 5, 4, 4, 3, 4, 4, 0};
static signed char HA3_rtbl60nit[10] = {0, 6, 6, 5, 3, 3, 3, 4, 4, 0};
static signed char HA3_rtbl64nit[10] = {0, 7, 6, 5, 3, 3, 3, 4, 4, 0};
static signed char HA3_rtbl68nit[10] = {0, 6, 5, 4, 3, 3, 3, 4, 4, 0};
static signed char HA3_rtbl72nit[10] = {0, 5, 5, 4, 3, 3, 3, 4, 4, 0};
static signed char HA3_rtbl77nit[10] = {0, 7, 5, 4, 4, 3, 4, 4, 4, 0};
static signed char HA3_rtbl82nit[10] = {0, 8, 5, 4, 3, 3, 4, 4, 4, 0};
static signed char HA3_rtbl87nit[10] = {0, 6, 4, 3, 2, 2, 3, 4, 3, 0};
static signed char HA3_rtbl93nit[10] = {0, 5, 4, 3, 3, 2, 3, 4, 2, 0};
static signed char HA3_rtbl98nit[10] = {0, 6, 4, 4, 3, 3, 4, 4, 3, 0};
static signed char HA3_rtbl105nit[10] = {0, 6, 4, 3, 3, 3, 4, 4, 2, 0};
static signed char HA3_rtbl111nit[10] = {0, 5, 4, 3, 3, 3, 3, 4, 2, 0};
static signed char HA3_rtbl119nit[10] = {0, 3, 4, 3, 3, 2, 4, 5, 2, 0};
static signed char HA3_rtbl126nit[10] = {0, 3, 4, 3, 3, 2, 4, 4, 2, 0};
static signed char HA3_rtbl134nit[10] = {0, 5, 3, 2, 3, 3, 4, 5, 2, 0};
static signed char HA3_rtbl143nit[10] = {0, 5, 3, 2, 2, 3, 3, 4, 2, 0};
static signed char HA3_rtbl152nit[10] = {0, 4, 3, 3, 3, 3, 3, 4, 2, 0};
static signed char HA3_rtbl162nit[10] = {0, 4, 3, 3, 3, 4, 5, 5, 3, 0};
static signed char HA3_rtbl172nit[10] = {0, 1, 3, 2, 2, 2, 4, 4, 2, 0};
static signed char HA3_rtbl183nit[10] = {0, 2, 3, 2, 2, 2, 4, 4, 2, 0};
static signed char HA3_rtbl195nit[10] = {0, 5, 2, 2, 2, 2, 4, 4, 2, 0};
static signed char HA3_rtbl207nit[10] = {0, 3, 2, 2, 2, 2, 3, 4, 2, 0};
static signed char HA3_rtbl220nit[10] = {0, 1, 2, 2, 2, 2, 3, 4, 1, 0};
static signed char HA3_rtbl234nit[10] = {0, 0, 2, 1, 1, 1, 3, 3, 1, 0};
static signed char HA3_rtbl249nit[10] = {0, 0, 1, 1, 1, 1, 3, 3, 1, 0};
static signed char HA3_rtbl265nit[10] = {0, 3, 1, 1, 1, 1, 2, 3, 2, 0};
static signed char HA3_rtbl282nit[10] = {0, 0, 1, 1, 1, 1, 2, 2, 0, 0};
static signed char HA3_rtbl300nit[10] = {0, 0, 1, 0, 1, 1, 2, 2, 1, 0};
static signed char HA3_rtbl316nit[10] = {0, 0, 1, 0, 0, 0, 1, 1, 1, 0};
static signed char HA3_rtbl333nit[10] = {0, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char HA3_rtbl360nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HA3_ctbl2nit[30] = {0, 0, 0, 0, 0, 0,8, 0, 5, 2, 0, 3, -7, 1, 3, -8, 3, 4, -4, 1, 2, -1, 1, -1, -2, 0, -1, -8, 1, -7};
static signed char HA3_ctbl3nit[30] = {0, 0, 0, 0, 0, 0,8, 0, 4, -2, 1, 0, -8, 0, 2, -8, 2, 1, -6, 1, -1, -2, 0, -2, -1, 1, -1, -5, 1, -5};
static signed char HA3_ctbl4nit[30] = {0, 0, 0, 0, 0, 0,8, 0, -2, -2, 1, 0, -7, 1, 2, -8, 1, -2, -4, 1, 0, -1, 1, -1, -3, 0, -2, -3, 1, -4};
static signed char HA3_ctbl5nit[30] = {0, 0, 0, 0, 0, 0,7, 0, -1, -3, 1, -1, -7, 2, 1, -8, 2, -3, -4, 1, -2, -2, 0, -3, -2, 0, -1, -3, 1, -4};
static signed char HA3_ctbl6nit[30] = {0, 0, 0, 0, 0, 0,4, 0, -3, -3, 2, -2, -8, 1, -2, -4, 2, -3, -5, 1, -2, -1, 0, -2, -1, 1, -1, -2, 1, -3};
static signed char HA3_ctbl7nit[30] = {0, 0, 0, 0, 0, 0,1, 0, -5, -4, 2, -2, -5, 1, -1, -6, 2, -4, -4, 1, -2, -1, 1, -1, -1, 1, -1, -1, 1, -2};
static signed char HA3_ctbl8nit[30] = {0, 0, 0, 0, 0, 0,0, 1, -5, -4, 2, -2, -5, 2, -2, -6, 2, -4, -2, 1, -2, -2, 0, -2, -2, 0, -2, -2, 0, -3};
static signed char HA3_ctbl9nit[30] = {0, 0, 0, 0, 0, 0,0, 2, -5, -4, 2, -2, -5, 2, -2, -6, 1, -5, -3, 0, -3, -2, 0, -2, -1, 0, -1, -2, 0, -3};
static signed char HA3_ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -6, -5, 2, -3, -4, 1, -2, -7, 1, -6, -4, 0, -3, 0, 0, -1, -2, 0, -2, -1, 0, -2};
static signed char HA3_ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -6, -5, 2, -4, -3, 2, -2, -5, 2, -4, -3, 0, -2, -2, 0, -2, -1, 0, -1, -1, 0, -2};
static signed char HA3_ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -6, -3, 1, -3, -4, 2, -3, -5, 2, -5, -2, 1, -2, -1, 0, -1, -2, 0, -2, 0, 0, -1};
static signed char HA3_ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -7, -3, 1, -3, -3, 2, -3, -6, 2, -6, -2, 0, -2, -1, 0, -2, -2, 0, -1, 0, 0, -1};
static signed char HA3_ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -8, -2, 2, -3, -3, 2, -3, -5, 2, -5, -2, 0, -2, -1, 0, -1, -1, 0, -1, 0, 0, -1};
static signed char HA3_ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -8, -3, 2, -3, -3, 2, -3, -4, 2, -4, -2, 0, -2, -1, 0, -1, -1, 0, -2, 0, 0, 0};
static signed char HA3_ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -9, -3, 2, -3, -3, 2, -2, -7, 2, -5, -1, 0, -1, -1, 0, -1, -1, 0, -2, 0, 0, 0};
static signed char HA3_ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -9, -5, 0, -5, -5, 0, -4, -4, 2, -5, -2, 1, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -8, -4, 2, -4, -3, 2, -2, -4, 1, -6, -3, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -8, -3, 2, -4, -3, 2, -1, -5, 1, -6, -2, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -7, -3, 2, -4, -4, 1, -2, -6, 1, -6, -1, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -10, -3, 2, -4, -3, 2, -1, -4, 1, -5, -2, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-3, 1, -9, -4, 1, -5, -3, 2, -1, -4, 1, -5, -1, 0, 1, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -9, -3, 1, -4, -4, 1, -2, -4, 1, -5, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-1, 2, -7, -2, 2, -3, -5, 1, -2, -4, 1, -4, -1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -10, -4, 1, -4, -4, 1, -1, -2, 1, -4, -2, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0};
static signed char HA3_ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -11, -5, 0, -5, -3, 1, 0, -4, 1, -6, -1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0};
static signed char HA3_ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -10, -4, 0, -4, -4, 1, -1, -3, 1, -5, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -9, -4, 1, -5, -3, 1, 0, -3, 1, -5, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -10, -4, 0, -4, -3, 1, 0, -2, 1, -4, -1, 0, 0, 1, 0, 0, -1, 0, -1, 0, 0, 0};
static signed char HA3_ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -10, -3, 2, -3, -3, 1, 0, -3, 1, -5, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -10, -3, 1, -4, -3, 1, 0, -2, 1, -4, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -12, -3, 1, -2, -3, 1, 0, -2, 1, -4, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -11, -4, 0, -3, -3, 0, 0, -2, 1, -4, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -10, -3, 1, -3, -3, 0, 0, -2, 1, -4, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-5, 1, -9, -3, 1, -3, -3, 0, 0, -3, 0, -5, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -11, -3, 1, -2, -2, 0, 0, -2, 1, -5, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -10, -2, 1, -2, -3, 0, 1, -1, 1, -4, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-7, 1, -11, -2, 0, -2, -3, 0, 1, -1, 1, -4, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -10, -2, 1, -3, -1, 0, 2, -1, 1, -4, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -9, -3, 0, -4, -2, 0, 1, -1, 0, -4, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -10, -3, 0, -3, -2, 0, 1, -2, 0, -5, 1, 0, 1, 1, 0, 1, -1, 0, -1, 1, 0, 1};
static signed char HA3_ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -10, -2, 0, -2, -2, 0, 0, -2, 0, -3, 1, 0, 1, -1, 0, -1, -1, 0, -1, 1, 0, 1};
static signed char HA3_ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -9, -2, 0, -2, -1, 0, 1, -2, 0, -4, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static signed char HA3_ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -7, -2, 0, -2, -1, 0, 1, -2, 0, -4, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, -1, 0};
static signed char HA3_ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -8, -2, 0, -2, -1, 0, 0, -1, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 1};
static signed char HA3_ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-5, 2, -7, -2, 0, -2, -1, 0, 1, -1, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, -1, 1};
static signed char HA3_ctbl111nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -1, 0, -2, -2, 0, 0, 0, 0, -2, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -6, -2, 0, -2, -1, 0, 1, -2, 0, -3, 1, 0, 2, -1, 0, -1, 0, 0, 0, 0, -1, 0};
static signed char HA3_ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-6, 1, -5, -2, 0, -2, -1, 0, 0, -1, 0, -2, 1, 0, 1, -1, 0, 0, 0, 0, 0, 0, -1, 0};
static signed char HA3_ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -1, 0, -2, 0, 0, 1, -1, 0, -1, 1, 0, 1, -1, 0, -1, 0, 0, 0, 0, -1, 0};
static signed char HA3_ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -1, 0, -1, -1, 0, 0, -1, 0, -2, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};
static signed char HA3_ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-3, 2, -3, -2, 0, -1, -1, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0};
static signed char HA3_ctbl162nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -2, -1, 0, 0, -1, 0, 1, -1, 0, -2, 1, 0, 0, -1, 0, -1, 0, 0, 0, 0, -1, 0};
static signed char HA3_ctbl172nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -4, -1, 0, -1, 0, 0, 1, -1, 0, -1, 1, 0, 1, -1, 0, 0, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-5, 0, -2, -1, 0, 0, 0, 0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-4, 1, -2, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static signed char HA3_ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-3, 0, -1, -1, 0, 0, 0, 0, 1, -1, 0, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -1, 0, 0, 1, 0, 0, 1, -1, 0, -1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char HA3_ctbl234nit[30] = {0, 0, 0, 0, 0, 0,-2, 0, -1, 0, 0, -1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0};
static signed char HA3_ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HA3_ctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char HA3_aid9810[3] = {0xB1, 0x90, 0xE3};
static signed char HA3_aid9702[3] = {0xB1, 0x90, 0xC7};
static signed char HA3_aid9624[3] = {0xB1, 0x90, 0xB3};
static signed char HA3_aid9512[3] = {0xB1, 0x90, 0x96};
static signed char HA3_aid9434[3] = {0xB1, 0x90, 0x82};
static signed char HA3_aid9333[3] = {0xB1, 0x90, 0x68};
static signed char HA3_aid9260[3] = {0xB1, 0x90, 0x55};
static signed char HA3_aid9186[3] = {0xB1, 0x90, 0x42};
static signed char HA3_aid9120[3] = {0xB1, 0x90, 0x31};
static signed char HA3_aid9058[3] = {0xB1, 0x90, 0x21};
static signed char HA3_aid8957[3] = {0xB1, 0x90, 0x07};
static signed char HA3_aid8891[3] = {0xB1, 0x80, 0xF6};
static signed char HA3_aid8826[3] = {0xB1, 0x80, 0xE5};
static signed char HA3_aid8752[3] = {0xB1, 0x80, 0xD2};
static signed char HA3_aid8651[3] = {0xB1, 0x80, 0xB8};
static signed char HA3_aid8574[3] = {0xB1, 0x80, 0xA4};
static signed char HA3_aid8399[3] = {0xB1, 0x80, 0x77};
static signed char HA3_aid8322[3] = {0xB1, 0x80, 0x63};
static signed char HA3_aid8248[3] = {0xB1, 0x80, 0x50};
static signed char HA3_aid8147[3] = {0xB1, 0x80, 0x36};
static signed char HA3_aid8000[3] = {0xB1, 0x80, 0x10};
static signed char HA3_aid7895[3] = {0xB1, 0x70, 0xF5};
static signed char HA3_aid7713[3] = {0xB1, 0x70, 0xC6};
static signed char HA3_aid7558[3] = {0xB1, 0x70, 0x9E};
static signed char HA3_aid7457[3] = {0xB1, 0x70, 0x84};
static signed char HA3_aid7279[3] = {0xB1, 0x70, 0x56};
static signed char HA3_aid7128[3] = {0xB1, 0x70, 0x2F};
static signed char HA3_aid6845[3] = {0xB1, 0x60, 0xE6};
static signed char HA3_aid6702[3] = {0xB1, 0x60, 0xC1};
static signed char HA3_aid6519[3] = {0xB1, 0x60, 0x92};
static signed char HA3_aid6256[3] = {0xB1, 0x60, 0x4E};
static signed char HA3_aid5977[3] = {0xB1, 0x60, 0x06};
static signed char HA3_aid5709[3] = {0xB1, 0x50, 0xC1};
static signed char HA3_aid5465[3] = {0xB1, 0x50, 0x82};
static signed char HA3_aid5171[3] = {0xB1, 0x50, 0x36};
static signed char HA3_aid4833[3] = {0xB1, 0x40, 0xDF};
static signed char HA3_aid4771[3] = {0xB1, 0x40, 0xCF};
static signed char HA3_aid4074[3] = {0xB1, 0x40, 0x1B};
static signed char HA3_aid3721[3] = {0xB1, 0x30, 0xC0};
static signed char HA3_aid3260[3] = {0xB1, 0x30, 0x49};
static signed char HA3_aid2814[3] = {0xB1, 0x20, 0xD6};
static signed char HA3_aid2264[3] = {0xB1, 0x20, 0x48};
static signed char HA3_aid1694[3] = {0xB1, 0x10, 0xB5};
static signed char HA3_aid1058[3] = {0xB1, 0x10, 0x11};
static signed char HA3_aid1004[3] = {0xB1, 0x10, 0x03};

static  unsigned char HA3_elv1[3] = {
        0xB5, 0xAC, 0x14
};
static  unsigned char HA3_elv2[3] = {
        0xB5, 0xAC, 0x13
};
static  unsigned char HA3_elv3[3] = {
        0xB5, 0xAC, 0x12
};
static  unsigned char HA3_elv4[3] = {
        0xB5, 0xAC, 0x11
};
static  unsigned char HA3_elv5[3] = {
        0xB5, 0xAC, 0x10
};
static  unsigned char HA3_elv6[3] = {
        0xB5, 0xAC, 0x0F
};
static  unsigned char HA3_elv7[3] = {
        0xB5, 0xAC, 0x0E
};
static  unsigned char HA3_elv8[3] = {
        0xB5, 0xAC, 0x0D
};
static  unsigned char HA3_elv9[3] = {
        0xB5, 0xAC, 0x0C
};
static  unsigned char HA3_elv10[3] = {
        0xB5, 0xAC, 0x0B
};
static  unsigned char HA3_elv11[3] = {
        0xB5, 0xAC, 0x0A
};


static  unsigned char HA3_elvCaps1[3] = {
        0xB5, 0xBC, 0x14
};
static  unsigned char HA3_elvCaps2[3] = {
        0xB5, 0xBC, 0x13
};
static  unsigned char HA3_elvCaps3[3] = {
        0xB5, 0xBC, 0x12
};
static  unsigned char HA3_elvCaps4[3] = {
        0xB5, 0xBC, 0x11
};
static  unsigned char HA3_elvCaps5[3] = {
        0xB5, 0xBC, 0x10
};
static  unsigned char HA3_elvCaps6[3] = {
        0xB5, 0xBC, 0x0F
};
static  unsigned char HA3_elvCaps7[3] = {
        0xB5, 0xBC, 0x0E
};
static  unsigned char HA3_elvCaps8[3] = {
        0xB5, 0xBC, 0x0D
};
static  unsigned char HA3_elvCaps9[3] = {
        0xB5, 0xBC, 0x0C
};
static  unsigned char HA3_elvCaps10[3] = {
        0xB5, 0xBC, 0x0B
};
static  unsigned char HA3_elvCaps11[3] = {
        0xB5, 0xBC, 0x0A
};


#ifdef CONFIG_LCD_HMT

static  signed char HA3_HMTrtbl10nit[10] = { 0, 8, 10, 8, 6, 4, 2, 2, 1, 0 };
static  signed char HA3_HMTrtbl11nit[10] = { 0, 13, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HA3_HMTrtbl12nit[10] = { 0, 11, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HA3_HMTrtbl13nit[10] = { 0, 11, 9, 7, 5, 3, 2, 3, 0, 0 };
static  signed char HA3_HMTrtbl14nit[10] = { 0, 13, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HA3_HMTrtbl15nit[10] = { 0, 9, 10, 7, 6, 4, 4, 4, 1, 0 };
static  signed char HA3_HMTrtbl16nit[10] = { 0, 10, 10, 7, 5, 5, 3, 3, 1, 0 };
static  signed char HA3_HMTrtbl17nit[10] = { 0, 8, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HA3_HMTrtbl19nit[10] = { 0, 11, 9, 7, 5, 4, 3, 3, 2, 0 };
static  signed char HA3_HMTrtbl20nit[10] = { 0, 10, 9, 7, 5, 5, 4, 3, 2, 0 };
static  signed char HA3_HMTrtbl21nit[10] = { 0, 9, 9, 7, 5, 4, 4, 4, 3, 0 };
static  signed char HA3_HMTrtbl22nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 4, 0 };
static  signed char HA3_HMTrtbl23nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 3, 0 };
static  signed char HA3_HMTrtbl25nit[10] = { 0, 12, 9, 7, 5, 5, 4, 4, 4, 0 };
static  signed char HA3_HMTrtbl27nit[10] = { 0, 7, 9, 6, 5, 4, 5, 4, 3, 0 };
static  signed char HA3_HMTrtbl29nit[10] = { 0, 10, 9, 7, 5, 5, 5, 7, 5, 0 };
static  signed char HA3_HMTrtbl31nit[10] = { 0, 8, 9, 7, 5, 5, 5, 5, 4, 0 };
static  signed char HA3_HMTrtbl33nit[10] = { 0, 10, 9, 6, 6, 5, 6, 7, 4, 0 };
static  signed char HA3_HMTrtbl35nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 4, 0 };
static  signed char HA3_HMTrtbl37nit[10] = { 0, 6, 9, 6, 5, 5, 6, 6, 2, 0 };
static  signed char HA3_HMTrtbl39nit[10] = { 0, 6, 9, 6, 5, 5, 6, 5, 2, 0 };
static  signed char HA3_HMTrtbl41nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 3, 0 };
static  signed char HA3_HMTrtbl44nit[10] = { 0, 6, 10, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HA3_HMTrtbl47nit[10] = { 0, 8, 10, 7, 5, 5, 5, 5, 1, 0 };
static  signed char HA3_HMTrtbl50nit[10] = { 0, 8, 10, 7, 6, 6, 6, 5, 0, 0 };
static  signed char HA3_HMTrtbl53nit[10] = { 0, 9, 9, 7, 6, 6, 6, 6, 0, 0 };
static  signed char HA3_HMTrtbl56nit[10] = { 0, 11, 9, 7, 5, 6, 6, 6, 2, 0 };
static  signed char HA3_HMTrtbl60nit[10] = { 0, 11, 9, 7, 5, 6, 6, 5, 1, 0 };
static  signed char HA3_HMTrtbl64nit[10] = { 0, 7, 10, 7, 6, 6, 6, 5, 1, 0 };
static  signed char HA3_HMTrtbl68nit[10] = { 0, 7, 9, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HA3_HMTrtbl72nit[10] = { 0, 7, 9, 6, 5, 5, 5, 6, 2, 0 };
static  signed char HA3_HMTrtbl77nit[10] = { 0, 6, 5, 4, 3, 3, 3, 4, -1, 0 };
static  signed char HA3_HMTrtbl82nit[10] = { 0, 3, 6, 4, 3, 3, 5, 5, 0, 0 };
static  signed char HA3_HMTrtbl87nit[10] = { 0, 1, 6, 4, 3, 2, 4, 5, 0, 0 };
static  signed char HA3_HMTrtbl93nit[10] = { 0, 3, 5, 3, 2, 3, 5, 4, 2, 0 };
static  signed char HA3_HMTrtbl99nit[10] = { 0, 3, 5, 3, 3, 3, 4, 4, 1, 0 };
static  signed char HA3_HMTrtbl105nit[10] = { 0, 6, 5, 3, 3, 3, 4, 4, 1, 0 };


static  signed char HA3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -3, 3, -7, -3, 2, -5, -4, 2, -5, -1, 2, -4, -1, 0, -2, -1, 0, 0, 0, 0, -1};
static  signed char HA3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -7, -4, 2, -6, -3, 2, -5, -2, 1, -4, -2, 0, -2, 0, 0, -1, 0, 0, 0};
static  signed char HA3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -6, -4, 3, -7, -3, 3, -6, -3, 2, -4, -1, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -6, -3, 4, -9, -3, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -2, 3, -6, -3, 2, -5, -2, 2, -5, -1, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static  signed char HA3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -5, 3, -8, -2, 2, -4, -2, 2, -4, -1, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -8, -4, 2, -5, -3, 2, -5, -1, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, -3, 3, -7, -4, 2, -5, -2, 2, -5, 0, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -8, -2, 3, -8, -5, 1, -4, -3, 2, -5, 0, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -8, -4, 3, -8, -3, 2, -4, -2, 2, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -3, 3, -8, -4, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -5, 3, -8, -2, 2, -4, -1, 2, -4, -2, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -8, -3, 2, -4, -1, 2, -4, -2, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -10, -4, 3, -7, -3, 1, -4, 0, 2, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HA3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -7, -4, 1, -4, -1, 1, -4, -1, 1, -2, -1, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 2, -6, -4, 1, -4, 0, 2, -4, -2, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -3, 3, -6, -3, 1, -4, -1, 2, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -3, 3, -7, -2, 1, -3, -1, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -4, 3, -6, -2, 1, -3, -1, 1, -4, -2, 0, -2, -1, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -8, -3, 3, -6, -4, 1, -4, -2, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -4, 2, -6, -3, 1, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -3, 3, -6, -3, 1, -4, -2, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -3, -1, 0, -2, 0, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -4, 2, -6, -2, 1, -4, -3, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -5, 2, -6, -2, 2, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HA3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 2, 0, 1};
static  signed char HA3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 1, -4, -2, 2, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static  signed char HA3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 2, -5, -2, 1, -4, -1, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -3, 2, -5, -2, 1, -4, -1, 0, -2, -1, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -1, 2, -4, -2, 1, -4, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HA3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -2, 2, -4, -1, 1, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -9, -2, 1, -4, -2, 0, -2, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HA3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -2, 1, -3, 0, 0, -2, -1, 0, -1, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static  signed char HA3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HA3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -2, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static  signed char HA3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -7, -2, 1, -4, 0, 1, -3, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HA3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -8, -4, 1, -4, 0, 1, -3, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};


static  unsigned char HA3_HMTaid8001[4] = {
        0xB2, 0x03, 0x12, 0x03
};

static  unsigned char HA3_HMTaid7003[4] = {
        0xB2, 0x03, 0x13, 0x04
};

static unsigned char HA3_HMTelv[3] = {
        0xB5, 0xAC, 0x0A
};
static unsigned char HA3_HMTelvCaps[3] = {
        0xB5, 0xBC, 0x0A
};

#endif

/********************************************** HF3 **********************************************/

// start of aid sheet in rev.A opmanual
static signed char HF3_rtbl2nit[10] = {0, 27, 28, 32, 30, 27, 22, 16, 9, 0};
static signed char HF3_rtbl3nit[10] = {0, 27, 28, 26, 25, 21, 19, 14, 8, 0};
static signed char HF3_rtbl4nit[10] = {0, 29, 27, 24, 24, 20, 19, 13, 8, 0};
static signed char HF3_rtbl5nit[10] = {0, 24, 25, 22, 21, 18, 18, 12, 7, 0};
static signed char HF3_rtbl6nit[10] = {0, 21, 22, 20, 19, 16, 16, 11, 6, 0};
static signed char HF3_rtbl7nit[10] = {0, 19, 20, 17, 17, 15, 14, 10, 6, 0};
static signed char HF3_rtbl8nit[10] = {0, 20, 20, 17, 16, 14, 13, 9, 6, 0};
static signed char HF3_rtbl9nit[10] = {0, 20, 18, 15, 14, 13, 13, 9, 6, 0};
static signed char HF3_rtbl10nit[10] = {0, 21, 17, 15, 14, 13, 12, 9, 6, 0};
static signed char HF3_rtbl11nit[10] = {0, 20, 16, 14, 13, 12, 11, 8, 6, 0};
static signed char HF3_rtbl12nit[10] = {0, 14, 15, 13, 12, 11, 11, 8, 5, 0};
static signed char HF3_rtbl13nit[10] = {0, 18, 15, 12, 12, 10, 10, 8, 5, 0};
static signed char HF3_rtbl14nit[10] = {0, 13, 14, 12, 11, 10, 10, 8, 5, 0};
static signed char HF3_rtbl15nit[10] = {0, 12, 13, 11, 10, 9, 9, 7, 5, 0};
static signed char HF3_rtbl16nit[10] = {0, 17, 13, 10, 10, 9, 9, 7, 5, 0};
static signed char HF3_rtbl17nit[10] = {0, 14, 13, 10, 10, 8, 9, 7, 5, 0};
static signed char HF3_rtbl19nit[10] = {0, 16, 12, 9, 9, 8, 8, 7, 5, 0};
static signed char HF3_rtbl20nit[10] = {0, 11, 12, 9, 9, 8, 8, 7, 5, 0};
static signed char HF3_rtbl21nit[10] = {0, 10, 11, 9, 9, 8, 8, 7, 5, 0};
static signed char HF3_rtbl22nit[10] = {0, 13, 11, 8, 9, 7, 8, 6, 5, 0};
static signed char HF3_rtbl24nit[10] = {0, 9, 10, 8, 8, 7, 7, 6, 5, 0};
static signed char HF3_rtbl25nit[10] = {0, 13, 10, 8, 8, 6, 7, 6, 5, 0};
static signed char HF3_rtbl27nit[10] = {0, 8, 9, 7, 7, 6, 7, 6, 5, 0};
static signed char HF3_rtbl29nit[10] = {0, 13, 9, 7, 7, 6, 7, 6, 5, 0};
static signed char HF3_rtbl30nit[10] = {0, 9, 9, 7, 7, 6, 6, 6, 4, 0};
static signed char HF3_rtbl32nit[10] = {0, 10, 8, 6, 6, 5, 6, 5, 4, 0};
static signed char HF3_rtbl34nit[10] = {0, 10, 8, 6, 6, 5, 6, 5, 4, 0};
static signed char HF3_rtbl37nit[10] = {0, 10, 7, 6, 6, 4, 5, 5, 4, 0};
static signed char HF3_rtbl39nit[10] = {0, 9, 7, 5, 5, 4, 5, 5, 4, 0};
static signed char HF3_rtbl41nit[10] = {0, 7, 7, 5, 5, 4, 5, 5, 4, 0};
static signed char HF3_rtbl44nit[10] = {0, 8, 6, 5, 5, 4, 5, 4, 4, 0};
static signed char HF3_rtbl47nit[10] = {0, 7, 6, 5, 5, 4, 4, 4, 4, 0};
static signed char HF3_rtbl50nit[10] = {0, 9, 5, 4, 4, 3, 4, 4, 4, 0};
static signed char HF3_rtbl53nit[10] = {0, 7, 5, 4, 4, 3, 4, 4, 4, 0};
static signed char HF3_rtbl56nit[10] = {0, 6, 5, 4, 4, 3, 4, 4, 4, 0};
static signed char HF3_rtbl60nit[10] = {0, 6, 4, 4, 3, 3, 3, 4, 3, 0};
static signed char HF3_rtbl64nit[10] = {0, 4, 4, 3, 3, 2, 3, 3, 3, 0};
static signed char HF3_rtbl68nit[10] = {0, 2, 4, 3, 3, 2, 3, 3, 3, 0};
static signed char HF3_rtbl72nit[10] = {0, 2, 3, 3, 2, 2, 2, 3, 3, 0};
static signed char HF3_rtbl77nit[10] = {0, 2, 3, 2, 2, 2, 3, 4, 4, 0};
static signed char HF3_rtbl82nit[10] = {0, 4, 4, 3, 2, 2, 3, 4, 3, 0};
static signed char HF3_rtbl87nit[10] = {0, 2, 4, 3, 2, 2, 3, 4, 3, 0};
static signed char HF3_rtbl93nit[10] = {0, 3, 3, 2, 2, 2, 4, 5, 3, 0};
static signed char HF3_rtbl98nit[10] = {0, 3, 3, 3, 3, 2, 4, 5, 3, 0};
static signed char HF3_rtbl105nit[10] = {0, 3, 3, 2, 2, 2, 4, 5, 2, 0};
static signed char HF3_rtbl110nit[10] = {0, 3, 3, 2, 2, 2, 4, 4, 1, 0};
static signed char HF3_rtbl119nit[10] = {0, 5, 3, 3, 3, 3, 5, 6, 2, 0};
static signed char HF3_rtbl126nit[10] = {0, 3, 2, 2, 2, 2, 4, 5, 2, 0};
static signed char HF3_rtbl134nit[10] = {0, 5, 2, 2, 1, 2, 4, 4, 1, 0};
static signed char HF3_rtbl143nit[10] = {0, 5, 2, 2, 2, 2, 4, 4, 2, 0};
static signed char HF3_rtbl152nit[10] = {0, 6, 2, 2, 2, 2, 4, 5, 3, 0};
static signed char HF3_rtbl162nit[10] = {0, 4, 1, 1, 2, 3, 5, 5, 3, 0};
static signed char HF3_rtbl172nit[10] = {0, 2, 2, 2, 2, 3, 5, 5, 3, 0};
static signed char HF3_rtbl183nit[10] = {0, 2, 2, 1, 2, 2, 5, 5, 2, 0};
static signed char HF3_rtbl195nit[10] = {0, 5, 1, 1, 2, 2, 4, 5, 2, 0};
static signed char HF3_rtbl207nit[10] = {0, 3, 1, 1, 2, 2, 4, 5, 2, 0};
static signed char HF3_rtbl220nit[10] = {0, 1, 1, 1, 2, 2, 4, 4, 2, 0};
static signed char HF3_rtbl234nit[10] = {0, 0, 1, 0, 1, 1, 3, 4, 2, 0};
static signed char HF3_rtbl249nit[10] = {0, 5, 0, 0, 1, 1, 2, 2, 0, 0};
static signed char HF3_rtbl265nit[10] = {0, 0, 1, 1, 0, 1, 2, 2, 1, 0};
static signed char HF3_rtbl282nit[10] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 0};
static signed char HF3_rtbl300nit[10] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char HF3_rtbl316nit[10] = {0, 1, 0, 0, -1, 0, 1, 1, 0, 0};
static signed char HF3_rtbl333nit[10] = {0, 0, 0, -1, -1, 0, 0, 0, 0, 0};
static signed char HF3_rtbl360nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HF3_ctbl2nit[30] = {0, 0, 0, 0, 0, 0,-19, 2, -9, -5, 2, 1, -4, 2, -1, -7, 3, -2, -5, 2, -2, -2, 0, -2, -2, 0, -1, -9, 0, -9};
static signed char HF3_ctbl3nit[30] = {0, 0, 0, 0, 0, 0,-9, 2, -4, -7, 2, -1, -4, 2, -1, -6, 3, -2, -6, 1, -3, -1, 0, -2, -1, 0, -1, -8, 0, -8};
static signed char HF3_ctbl4nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -2, -7, 3, -1, -3, 2, -3, -5, 3, -2, -5, 1, -4, -3, 0, -2, -1, 0, -1, -6, 0, -6};
static signed char HF3_ctbl5nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -3, -7, 3, -2, -5, 2, -4, -5, 3, -2, -3, 2, -2, -3, 0, -4, 0, 0, 0, -5, 0, -6};
static signed char HF3_ctbl6nit[30] = {0, 0, 0, 0, 0, 0,-9, 4, -7, -5, 3, -3, -5, 2, -4, -5, 2, -5, -5, 2, -3, -2, 0, -2, -3, 0, -2, -3, 0, -4};
static signed char HF3_ctbl7nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -9, 3, -5, -4, 2, -5, -5, 2, -5, -4, 2, -5, -2, 0, -3, -2, 0, -1, -2, 0, -3};
static signed char HF3_ctbl8nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -6, -7, 2, -4, -5, 2, -7, -4, 3, -3, -4, 1, -4, -1, 0, -2, -2, 0, -1, -2, 0, -3};
static signed char HF3_ctbl9nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -6, -5, 3, -6, -3, 3, -6, -5, 3, -6, -3, 1, -3, -1, 0, -2, -1, 0, -1, -2, 0, -2};
static signed char HF3_ctbl10nit[30] = {0, 0, 0, 0, 0, 0,-7, 4, -8, -4, 2, -5, -3, 3, -6, -5, 2, -6, -4, 1, -4, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl11nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -6, 2, -6, -3, 2, -6, -4, 3, -6, -3, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl12nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -5, 2, -6, -3, 2, -4, -4, 3, -6, -3, 1, -3, 0, 0, -1, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl13nit[30] = {0, 0, 0, 0, 0, 0,-5, 3, -8, -5, 3, -7, -4, 1, -4, -5, 3, -6, -2, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -4, 2, -5, -3, 2, -4, -5, 3, -6, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl15nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -10, -4, 2, -5, -2, 2, -4, -5, 3, -6, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl16nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -4, 3, -6, -3, 1, -4, -5, 3, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -5, 3, -6, -3, 1, -4, -5, 2, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl19nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -10, -5, 2, -6, -3, 1, -4, -5, 2, -6, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl20nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -4, 2, -6, -4, 1, -4, -4, 2, -5, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl21nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -12, -5, 2, -6, -3, 1, -3, -4, 2, -6, -1, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl22nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -10, -4, 3, -6, -3, 1, -3, -3, 2, -5, 0, 0, -2, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl24nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -12, -5, 2, -6, -3, 1, -2, -3, 2, -5, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl25nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -11, -4, 2, -5, -3, 1, -2, -2, 2, -5, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl27nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -12, -4, 2, -6, -3, 1, -2, -1, 2, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl29nit[30] = {0, 0, 0, 0, 0, 0,-5, 5, -11, -4, 2, -6, -3, 0, -2, -1, 2, -4, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl30nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -3, 2, -5, -3, 1, -2, -2, 2, -4, 0, 0, -2, 0, 0, 0, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl32nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -11, -2, 2, -4, -3, 1, -2, -2, 2, -4, 0, 1, -2, 1, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl34nit[30] = {0, 0, 0, 0, 0, 0,-3, 5, -11, -3, 2, -5, -3, 0, -2, -3, 2, -5, 0, 0, -2, 1, 0, 1, 0, 0, 1, 1, 0, 2};
static signed char HF3_ctbl37nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -12, -3, 2, -4, -2, 0, -1, -2, 2, -4, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl39nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -3, 2, -5, -2, 0, -1, -2, 2, -4, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -4, 2, -5, -1, 0, -1, -1, 2, -4, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl44nit[30] = {0, 0, 0, 0, 0, 0,-3, 5, -12, -4, 2, -4, 0, 0, 0, -1, 2, -4, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -12, -3, 1, -3, -1, 0, -1, -1, 1, -4, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl50nit[30] = {0, 0, 0, 0, 0, 0,-3, 6, -12, -3, 1, -4, -2, 0, 0, -2, 2, -6, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 5, -11, -4, 1, -4, -1, 0, 0, -1, 2, -5, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl56nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -10, -2, 1, -3, -2, 0, 0, -1, 2, -6, 1, 0, 1, 1, 0, 1, 1, 0, 2, 1, 0, 2};
static signed char HF3_ctbl60nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -11, 0, 1, -2, -1, 0, 0, -1, 2, -5, 1, 0, 1, 1, 0, 1, 1, 0, 2, 1, 0, 2};
static signed char HF3_ctbl64nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, 0, 1, -2, 0, 0, 0, -2, 2, -6, 1, 0, 1, 2, 0, 2, 2, 0, 2, 1, 0, 2};
static signed char HF3_ctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, 0, 1, -2, 0, 0, 0, -1, 2, -5, 1, 0, 1, 2, 0, 2, 2, 0, 2, 1, 0, 2};
static signed char HF3_ctbl72nit[30] = {0, 0, 0, 0, 0, 0,-2, 5, -12, -1, 0, -2, -1, 0, 0, -1, 2, -4, 1, 0, 1, 2, 0, 2, 2, 0, 2, 1, 0, 2};
static signed char HF3_ctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 5, -10, 0, 1, -3, -1, 0, 0, 0, 2, -5, 1, 0, 1, 2, 0, 2, 2, 0, 2, 1, 0, 2};
static signed char HF3_ctbl82nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -1, 0, -2, -2, 0, 0, -2, 2, -5, 1, 0, 1, 2, 0, 2, 2, 0, 2, 1, 0, 2};
static signed char HF3_ctbl87nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -2, 0, -2, 0, 0, 1, 0, 1, -4, 1, 0, 1, 1, 0, 2, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl93nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -7, 0, 0, -2, 0, 0, 0, 0, 1, -4, 1, 0, 1, 1, 0, 2, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl98nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, 0, 0, -1, 0, 0, 0, 0, 1, -4, 1, 0, 1, 1, 0, 2, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl105nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -7, 0, 0, -1, 0, 0, 0, 0, 1, -2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl110nit[30] = {0, 0, 0, 0, 0, 0,-2, 3, -7, 0, 0, -1, 1, 0, 1, 0, 1, -2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl119nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, 0, 0, 0, 0, 0, 0, 0, 1, -2, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 2};
static signed char HF3_ctbl126nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char HF3_ctbl134nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -2, 0, -1, 0, 0, 0, 0, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char HF3_ctbl143nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, 0, 0, 0, 0, 0, 0, -2, 0, -1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char HF3_ctbl152nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, 0, 0, -1, -1, 0, -1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1};
static signed char HF3_ctbl162nit[30] = {0, 0, 0, 0, 0, 0,-2, 2, -4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl172nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -4, 1, 0, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1};
static signed char HF3_ctbl183nit[30] = {0, 0, 0, 0, 0, 0,-2, 1, -4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1};
static signed char HF3_ctbl195nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -4, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl207nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -3, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl220nit[30] = {0, 0, 0, 0, 0, 0,-1, 1, -3, 1, 0, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl234nit[30] = {0, 0, 0, 0, 0, 0,0, 0, -1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1};
static signed char HF3_ctbl249nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl265nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl282nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl300nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl316nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl333nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char HF3_ctbl360nit[30] = {0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static signed char HF3_aid9779[3] = {0xB1, 0x90, 0xD7};
static signed char HF3_aid9701[3] = {0xB1, 0x90, 0xC3};
static signed char HF3_aid9585[3] = {0xB1, 0x90, 0xA5};
static signed char HF3_aid9507[3] = {0xB1, 0x90, 0x91};
static signed char HF3_aid9394[3] = {0xB1, 0x90, 0x74};
static signed char HF3_aid9328[3] = {0xB1, 0x90, 0x63};
static signed char HF3_aid9259[3] = {0xB1, 0x90, 0x51};
static signed char HF3_aid9154[3] = {0xB1, 0x90, 0x36};
static signed char HF3_aid9088[3] = {0xB1, 0x90, 0x25};
static signed char HF3_aid9026[3] = {0xB1, 0x90, 0x15};
static signed char HF3_aid8956[3] = {0xB1, 0x90, 0x03};
static signed char HF3_aid8894[3] = {0xB1, 0x80, 0xF3};
static signed char HF3_aid8839[3] = {0xB1, 0x80, 0xE5};
static signed char HF3_aid8754[3] = {0xB1, 0x80, 0xCF};
static signed char HF3_aid8661[3] = {0xB1, 0x80, 0xB7};
static signed char HF3_aid8587[3] = {0xB1, 0x80, 0xA4};
static signed char HF3_aid8408[3] = {0xB1, 0x80, 0x76};
static signed char HF3_aid8335[3] = {0xB1, 0x80, 0x63};
static signed char HF3_aid8249[3] = {0xB1, 0x80, 0x4D};
static signed char HF3_aid8129[3] = {0xB1, 0x80, 0x2E};
static signed char HF3_aid7974[3] = {0xB1, 0x80, 0x06};
static signed char HF3_aid7904[3] = {0xB1, 0x70, 0xF4};
static signed char HF3_aid7694[3] = {0xB1, 0x70, 0xBE};
static signed char HF3_aid7578[3] = {0xB1, 0x70, 0xA0};
static signed char HF3_aid7469[3] = {0xB1, 0x70, 0x84};
static signed char HF3_aid7263[3] = {0xB1, 0x70, 0x4F};
static signed char HF3_aid7147[3] = {0xB1, 0x70, 0x31};
static signed char HF3_aid6828[3] = {0xB1, 0x60, 0xDF};
static signed char HF3_aid6712[3] = {0xB1, 0x60, 0xC1};
static signed char HF3_aid6510[3] = {0xB1, 0x60, 0x8D};
static signed char HF3_aid6238[3] = {0xB1, 0x60, 0x47};
static signed char HF3_aid5982[3] = {0xB1, 0x60, 0x05};
static signed char HF3_aid5707[3] = {0xB1, 0x50, 0xBE};
static signed char HF3_aid5427[3] = {0xB1, 0x50, 0x76};
static signed char HF3_aid5163[3] = {0xB1, 0x50, 0x32};
static signed char HF3_aid4802[3] = {0xB1, 0x40, 0xD5};
static signed char HF3_aid4425[3] = {0xB1, 0x40, 0x74};
static signed char HF3_aid4057[3] = {0xB1, 0x40, 0x15};
static signed char HF3_aid3653[3] = {0xB1, 0x30, 0xAD};
static signed char HF3_aid3222[3] = {0xB1, 0x30, 0x3E};
static signed char HF3_aid2741[3] = {0xB1, 0x20, 0xC2};
static signed char HF3_aid2151[3] = {0xB1, 0x20, 0x2A};
static signed char HF3_aid1568[3] = {0xB1, 0x10, 0x94};
static signed char HF3_aid1005[3] = {0xB1, 0x10, 0x03};


// end of aid sheet in rev.E opmanual


static  unsigned char HF3_elv1[3] = {
        0xB5, 0xAC, 0x16
};
static  unsigned char HF3_elv2[3] = {
        0xB5, 0xAC, 0x15
};
static  unsigned char HF3_elv3[3] = {
        0xB5, 0xAC, 0x14
};
static  unsigned char HF3_elv4[3] = {
        0xB5, 0xAC, 0x13
};
static  unsigned char HF3_elv5[3] = {
        0xB5, 0xAC, 0x12
};
static  unsigned char HF3_elv6[3] = {
        0xB5, 0xAC, 0x11
};
static  unsigned char HF3_elv7[3] = {
        0xB5, 0xAC, 0x10
};
static  unsigned char HF3_elv8[3] = {
        0xB5, 0xAC, 0x0F
};
static  unsigned char HF3_elv9[3] = {
        0xB5, 0xAC, 0x0E
};
static  unsigned char HF3_elv10[3] = {
        0xB5, 0xAC, 0x0D
};
static  unsigned char HF3_elv11[3] = {
        0xB5, 0xAC, 0x0C
};
static  unsigned char HF3_elv12[3] = {
        0xB5, 0xAC, 0x0B
};
static  unsigned char HF3_elv13[3] = {
        0xB5, 0xAC, 0x0A
};


static  unsigned char HF3_elvCaps1[3] = {
        0xB5, 0xBC, 0x16
};
static  unsigned char HF3_elvCaps2[3] = {
        0xB5, 0xBC, 0x15
};
static  unsigned char HF3_elvCaps3[3] = {
        0xB5, 0xBC, 0x14
};
static  unsigned char HF3_elvCaps4[3] = {
        0xB5, 0xBC, 0x13
};
static  unsigned char HF3_elvCaps5[3] = {
        0xB5, 0xBC, 0x12
};
static  unsigned char HF3_elvCaps6[3] = {
        0xB5, 0xBC, 0x11
};
static  unsigned char HF3_elvCaps7[3] = {
        0xB5, 0xBC, 0x10
};
static  unsigned char HF3_elvCaps8[3] = {
        0xB5, 0xBC, 0x0F
};
static  unsigned char HF3_elvCaps9[3] = {
        0xB5, 0xBC, 0x0E
};
static  unsigned char HF3_elvCaps10[3] = {
        0xB5, 0xBC, 0x0D
};
static  unsigned char HF3_elvCaps11[3] = {
        0xB5, 0xBC, 0x0C
};
static  unsigned char HF3_elvCaps12[3] = {
        0xB5, 0xBC, 0x0B
};
static  unsigned char HF3_elvCaps13[3] = {
        0xB5, 0xBC, 0x0A
};


#ifdef CONFIG_LCD_HMT

static  signed char HF3_HMTrtbl10nit[10] = { 0, 8, 10, 8, 6, 4, 2, 2, 1, 0 };
static  signed char HF3_HMTrtbl11nit[10] = { 0, 13, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HF3_HMTrtbl12nit[10] = { 0, 11, 9, 7, 5, 4, 2, 2, 0, 0 };
static  signed char HF3_HMTrtbl13nit[10] = { 0, 11, 9, 7, 5, 3, 2, 3, 0, 0 };
static  signed char HF3_HMTrtbl14nit[10] = { 0, 13, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HF3_HMTrtbl15nit[10] = { 0, 9, 10, 7, 6, 4, 4, 4, 1, 0 };
static  signed char HF3_HMTrtbl16nit[10] = { 0, 10, 10, 7, 5, 5, 3, 3, 1, 0 };
static  signed char HF3_HMTrtbl17nit[10] = { 0, 8, 9, 7, 5, 4, 3, 3, 1, 0 };
static  signed char HF3_HMTrtbl19nit[10] = { 0, 11, 9, 7, 5, 4, 3, 3, 2, 0 };
static  signed char HF3_HMTrtbl20nit[10] = { 0, 10, 9, 7, 5, 5, 4, 3, 2, 0 };
static  signed char HF3_HMTrtbl21nit[10] = { 0, 9, 9, 7, 5, 4, 4, 4, 3, 0 };
static  signed char HF3_HMTrtbl22nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 4, 0 };
static  signed char HF3_HMTrtbl23nit[10] = { 0, 9, 9, 6, 5, 4, 3, 3, 3, 0 };
static  signed char HF3_HMTrtbl25nit[10] = { 0, 12, 9, 7, 5, 5, 4, 4, 4, 0 };
static  signed char HF3_HMTrtbl27nit[10] = { 0, 7, 9, 6, 5, 4, 5, 4, 3, 0 };
static  signed char HF3_HMTrtbl29nit[10] = { 0, 10, 9, 7, 5, 5, 5, 7, 5, 0 };
static  signed char HF3_HMTrtbl31nit[10] = { 0, 8, 9, 7, 5, 5, 5, 5, 4, 0 };
static  signed char HF3_HMTrtbl33nit[10] = { 0, 10, 9, 6, 6, 5, 6, 7, 4, 0 };
static  signed char HF3_HMTrtbl35nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 4, 0 };
static  signed char HF3_HMTrtbl37nit[10] = { 0, 6, 9, 6, 5, 5, 6, 6, 2, 0 };
static  signed char HF3_HMTrtbl39nit[10] = { 0, 6, 9, 6, 5, 5, 6, 5, 2, 0 };
static  signed char HF3_HMTrtbl41nit[10] = { 0, 11, 9, 7, 6, 6, 6, 6, 3, 0 };
static  signed char HF3_HMTrtbl44nit[10] = { 0, 6, 10, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HF3_HMTrtbl47nit[10] = { 0, 8, 10, 7, 5, 5, 5, 5, 1, 0 };
static  signed char HF3_HMTrtbl50nit[10] = { 0, 8, 10, 7, 6, 6, 6, 5, 0, 0 };
static  signed char HF3_HMTrtbl53nit[10] = { 0, 9, 9, 7, 6, 6, 6, 6, 0, 0 };
static  signed char HF3_HMTrtbl56nit[10] = { 0, 11, 9, 7, 5, 6, 6, 6, 2, 0 };
static  signed char HF3_HMTrtbl60nit[10] = { 0, 11, 9, 7, 5, 6, 6, 5, 1, 0 };
static  signed char HF3_HMTrtbl64nit[10] = { 0, 7, 10, 7, 6, 6, 6, 5, 1, 0 };
static  signed char HF3_HMTrtbl68nit[10] = { 0, 7, 9, 7, 6, 5, 5, 5, 1, 0 };
static  signed char HF3_HMTrtbl72nit[10] = { 0, 7, 9, 6, 5, 5, 5, 6, 2, 0 };
static  signed char HF3_HMTrtbl77nit[10] = { 0, 6, 5, 4, 3, 3, 3, 4, -1, 0 };
static  signed char HF3_HMTrtbl82nit[10] = { 0, 3, 6, 4, 3, 3, 5, 5, 0, 0 };
static  signed char HF3_HMTrtbl87nit[10] = { 0, 1, 6, 4, 3, 2, 4, 5, 0, 0 };
static  signed char HF3_HMTrtbl93nit[10] = { 0, 3, 5, 3, 2, 3, 5, 4, 2, 0 };
static  signed char HF3_HMTrtbl99nit[10] = { 0, 3, 5, 3, 3, 3, 4, 4, 1, 0 };
static  signed char HF3_HMTrtbl105nit[10] = { 0, 6, 5, 3, 3, 3, 4, 4, 1, 0 };


static  signed char HF3_HMTctbl10nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -6, -3, 3, -7, -3, 2, -5, -4, 2, -5, -1, 2, -4, -1, 0, -2, -1, 0, 0, 0, 0, -1};
static  signed char HF3_HMTctbl11nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -7, -4, 2, -6, -3, 2, -5, -2, 1, -4, -2, 0, -2, 0, 0, -1, 0, 0, 0};
static  signed char HF3_HMTctbl12nit[30] = {0, 0, 0, 0, 0, 0,-8, 3, -6, -4, 3, -7, -3, 3, -6, -3, 2, -4, -1, 1, -3, -1, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl13nit[30] = {0, 0, 0, 0, 0, 0,-7, 2, -6, -3, 4, -9, -3, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl14nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -2, 3, -6, -3, 2, -5, -2, 2, -5, -1, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0};
static  signed char HF3_HMTctbl15nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -7, -5, 3, -8, -2, 2, -4, -2, 2, -4, -1, 1, -4, -2, 0, -2, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl16nit[30] = {0, 0, 0, 0, 0, 0,-6, 2, -6, -3, 3, -8, -4, 2, -5, -3, 2, -5, -1, 1, -4, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl17nit[30] = {0, 0, 0, 0, 0, 0,-4, 2, -6, -3, 3, -7, -4, 2, -5, -2, 2, -5, 0, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl19nit[30] = {0, 0, 0, 0, 0, 0,-7, 3, -8, -2, 3, -8, -5, 1, -4, -3, 2, -5, 0, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl20nit[30] = {0, 0, 0, 0, 0, 0,-6, 3, -8, -4, 3, -8, -3, 2, -4, -2, 2, -4, -1, 1, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl21nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -8, -3, 3, -8, -4, 2, -4, -2, 2, -5, -1, 1, -3, 0, 0, 0, -1, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl22nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -8, -5, 3, -8, -2, 2, -4, -1, 2, -4, -2, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl23nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -8, -3, 2, -4, -1, 2, -4, -2, 1, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl25nit[30] = {0, 0, 0, 0, 0, 0,-8, 4, -10, -4, 3, -7, -3, 1, -4, 0, 2, -4, -2, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static  signed char HF3_HMTctbl27nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -8, -4, 3, -7, -4, 1, -4, -1, 1, -4, -1, 1, -2, -1, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl29nit[30] = {0, 0, 0, 0, 0, 0,-6, 4, -10, -3, 2, -6, -4, 1, -4, 0, 2, -4, -2, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl31nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -3, 3, -6, -3, 1, -4, -1, 2, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl33nit[30] = {0, 0, 0, 0, 0, 0,-4, 3, -8, -3, 3, -7, -2, 1, -3, -1, 1, -4, -1, 1, -2, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl35nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -4, 3, -6, -2, 1, -3, -1, 1, -4, -2, 0, -2, -1, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl37nit[30] = {0, 0, 0, 0, 0, 0,-3, 3, -8, -3, 3, -6, -4, 1, -4, -2, 1, -4, -1, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl39nit[30] = {0, 0, 0, 0, 0, 0,-5, 4, -9, -4, 2, -6, -3, 1, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HF3_HMTctbl41nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -10, -3, 3, -6, -3, 1, -4, -2, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl44nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -3, -1, 0, -2, 0, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HF3_HMTctbl47nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -4, 2, -6, -2, 1, -4, -3, 1, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl50nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -5, 2, -6, -2, 2, -4, -1, 1, -3, -1, 0, -2, -1, 0, -1, 0, 0, 0, 2, 0, 2};
static  signed char HF3_HMTctbl53nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -9, -4, 2, -6, -3, 1, -4, 0, 1, -2, -1, 0, -1, -1, 0, -1, 0, 0, 0, 2, 0, 1};
static  signed char HF3_HMTctbl56nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 1, -4, -2, 2, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 1};
static  signed char HF3_HMTctbl60nit[30] = {0, 0, 0, 0, 0, 0,-4, 5, -10, -2, 2, -5, -2, 1, -4, -1, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl64nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -3, 2, -5, -2, 1, -4, -1, 0, -2, -1, 0, -2, 0, 0, 0, -1, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl68nit[30] = {0, 0, 0, 0, 0, 0,-4, 4, -9, -1, 2, -4, -2, 1, -4, -1, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 0};
static  signed char HF3_HMTctbl72nit[30] = {0, 0, 0, 0, 0, 0,-3, 4, -8, -2, 2, -4, -1, 1, -4, -2, 1, -2, -1, 0, -2, 0, 0, -1, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl77nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -9, -2, 1, -4, -2, 0, -2, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HF3_HMTctbl82nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -2, 1, -3, 0, 0, -2, -1, 0, -1, 0, 0, -1, 0, 0, 1, 1, 0, 1};
static  signed char HF3_HMTctbl87nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -3, 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};
static  signed char HF3_HMTctbl93nit[30] = {0, 0, 0, 0, 0, 0,-1, 3, -8, -2, 2, -4, -1, 1, -2, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2};
static  signed char HF3_HMTctbl99nit[30] = {0, 0, 0, 0, 0, 0,0, 3, -7, -2, 1, -4, 0, 1, -3, 0, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
static  signed char HF3_HMTctbl105nit[30] = {0, 0, 0, 0, 0, 0,-1, 4, -8, -4, 1, -4, 0, 1, -3, -1, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 3};


static  unsigned char HF3_HMTaid8001[4] = {
        0xB2, 0x03, 0x12, 0x03
};

static  unsigned char HF3_HMTaid7003[4] = {
        0xB2, 0x03, 0x13, 0x04
};

static unsigned char HF3_HMTelv[3] = {
        0xB5, 0xAC, 0x0A
};
static unsigned char HF3_HMTelvCaps[3] = {
        0xB5, 0xBC, 0x0A
};

#endif


#endif

