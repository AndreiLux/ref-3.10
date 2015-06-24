/*
 * drivers/video/decon/panels/S6E3HA0_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3ha3_wqhd_aid_dimming.h"
#endif
#include "s6e3ha3_s6e3ha2_wqhd_param.h"
#include "../dsim.h"
#include <video/mipi_display.h>

extern unsigned dynamic_lcd_type;
extern unsigned char esd_support;

unsigned char ELVSS_LEN;
unsigned char ELVSS_REG;
unsigned char ELVSS_CMD_CNT;
unsigned char AID_CMD_CNT;
unsigned char AID_REG_OFFSET;
unsigned char TSET_LEN;
unsigned char TSET_REG;
unsigned char TSET_MINUS_OFFSET;
unsigned char VINT_REG2;


#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = { SEQ_HBM_OFF, SEQ_HBM_ON };
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = { S6E3HA2_SEQ_ACL_OFF, S6E3HA2_SEQ_ACL_8 };
static const unsigned char *ACL_OPR_TABLE_HA2[ACL_OPR_MAX] = { S6E3HA2_SEQ_ACL_OFF_OPR, S6E3HA2_SEQ_ACL_ON_OPR };
static const unsigned char *ACL_OPR_TABLE_HA3[ACL_OPR_MAX] = { S6E3HA3_SEQ_ACL_OFF_OPR, S6E3HA3_SEQ_ACL_ON_OPR };
static const unsigned char *ACL_OPR_TABLE_HF3[ACL_OPR_MAX] = { S6E3HF3_SEQ_ACL_OFF_OPR, S6E3HF3_SEQ_ACL_ON_OPR };

static const unsigned int br_tbl[256] = {
        2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,   // 16
        16, 17, 18, 19, 20, 21, 22, 23, 25, 27, 29, 31, 33, 36, // 14
        39, 41, 41, 44, 44, 47, 47, 50, 50, 53, 53, 56, 56, 56, // 14
        60, 60, 60, 64, 64, 64, 68, 68, 68, 72, 72, 72, 72, 77, // 14
        77, 77, 82, 82, 82, 82, 87, 87, 87, 87, 93, 93, 93, 93, // 14
        98, 98, 98, 98, 98, 105, 105, 105, 105, 111, 111, 111,  // 12
        111, 111, 111, 119, 119, 119, 119, 119, 126, 126, 126,  // 11
        126, 126, 126, 134, 134, 134, 134, 134, 134, 134, 143,
        143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152,
        152, 152, 162, 162, 162, 162, 162, 162, 162, 172, 172,
        172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183,
        183, 183, 183, 183, 195, 195, 195, 195, 195, 195, 195,
        195, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
        220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 234,
        234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 249,
        249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
        265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265,
        265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
        282, 282, 282, 300, 300, 300, 300, 300, 300, 300, 300,
        300, 300, 300, 300, 316, 316, 316, 316, 316, 316, 316,
        316, 316, 316, 316, 316, 333, 333, 333, 333, 333, 333,
        333, 333, 333, 333, 333, 333, 360       //7
};


static const unsigned int hbm_interpolation_br_tbl[256] = {
        2, 2, 2, 4, 7, 9, 11, 14, 16, 19, 21, 23, 26, 28, 30, 33,
        35, 37, 40, 42, 45, 47, 49, 52, 54, 56, 59, 61, 63, 66, 68, 71,
        73, 75, 78, 80, 82, 85, 87, 89, 92, 94, 97, 99, 101, 104, 106, 108,
        111, 113, 115, 118, 120, 123, 125, 127, 130, 132, 134, 137, 139, 141, 144, 146,
        149, 151, 153, 156, 158, 160, 163, 165, 167, 170, 172, 175, 177, 179, 182, 184,
        186, 189, 191, 193, 196, 198, 201, 203, 205, 208, 210, 212, 215, 217, 219, 222,
        224, 227, 229, 231, 234, 236, 238, 241, 243, 245, 248, 250, 253, 255, 257, 260,
        262, 264, 267, 269, 271, 274, 276, 279, 281, 283, 286, 288, 290, 293, 295, 297,
        300, 302, 305, 307, 309, 312, 314, 316, 319, 321, 323, 326, 328, 331, 333, 335,
        338, 340, 342, 345, 347, 349, 352, 354, 357, 359, 361, 364, 366, 368, 371, 373,
        375, 378, 380, 383, 385, 387, 390, 392, 394, 397, 399, 401, 404, 406, 409, 411,
        413, 416, 418, 420, 423, 425, 427, 430, 432, 435, 437, 439, 442, 444, 446, 449,
        451, 453, 456, 458, 461, 463, 465, 468, 470, 472, 475, 477, 479, 482, 484, 487,
        489, 491, 494, 496, 498, 501, 503, 505, 508, 510, 513, 515, 517, 520, 522, 524,
        527, 529, 531, 534, 536, 539, 541, 543, 546, 548, 550, 553, 555, 557, 560, 562,
        565, 567, 569, 572, 574, 576, 579, 581, 583, 586, 588, 591, 593, 595, 598, 600
};
static const short center_gamma[NUM_VREF][CI_MAX] = {
        {0x000, 0x000, 0x000},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x080, 0x080, 0x080},
        {0x100, 0x100, 0x100},
};

// aid sheet in rev.E opmanual
struct SmtDimInfo dimming_info_HA2_RE[MAX_BR_INFO] = {  // add hbm array
	{.br = 2, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl2nit, .cTbl = HA2_REctbl2nit, .aid = HA2_aid9825, .elvCaps = HA2_elvCaps5, .elv = HA2_elv5, .way = W1},
	{.br = 3, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl3nit, .cTbl = HA2_REctbl3nit, .aid = HA2_aid9713, .elvCaps = HA2_elvCaps4, .elv = HA2_elv4, .way = W1},
	{.br = 4, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl4nit, .cTbl = HA2_REctbl4nit, .aid = HA2_aid9635, .elvCaps = HA2_elvCaps3, .elv = HA2_elv3, .way = W1},
	{.br = 5, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl5nit, .cTbl = HA2_REctbl5nit, .aid = HA2_aid9523, .elvCaps = HA2_elvCaps2, .elv = HA2_elv2, .way = W1},
	{.br = 6, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl6nit, .cTbl = HA2_REctbl6nit, .aid = HA2_aid9445, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 7, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl7nit, .cTbl = HA2_REctbl7nit, .aid = HA2_aid9344, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 8, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl8nit, .cTbl = HA2_REctbl8nit, .aid = HA2_aid9270, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 9, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl9nit, .cTbl = HA2_REctbl9nit, .aid = HA2_aid9200, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 10, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl10nit, .cTbl = HA2_REctbl10nit, .aid = HA2_aid9130, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 11, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl11nit, .cTbl = HA2_REctbl11nit, .aid = HA2_aid9030, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 12, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl12nit, .cTbl = HA2_REctbl12nit, .aid = HA2_aid8964, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 13, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl13nit, .cTbl = HA2_REctbl13nit, .aid = HA2_aid8894, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 14, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl14nit, .cTbl = HA2_REctbl14nit, .aid = HA2_aid8832, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 15, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl15nit, .cTbl = HA2_REctbl15nit, .aid = HA2_aid8723, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 16, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl16nit, .cTbl = HA2_REctbl16nit, .aid = HA2_aid8653, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 17, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl17nit, .cTbl = HA2_REctbl17nit, .aid = HA2_aid8575, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 19, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl19nit, .cTbl = HA2_REctbl19nit, .aid = HA2_aid8393, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 20, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl20nit, .cTbl = HA2_REctbl20nit, .aid = HA2_aid8284, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 21, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl21nit, .cTbl = HA2_REctbl21nit, .aid = HA2_aid8218, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 22, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl22nit, .cTbl = HA2_REctbl22nit, .aid = HA2_aid8148, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 24, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl24nit, .cTbl = HA2_REctbl24nit, .aid = HA2_aid7974, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 25, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl25nit, .cTbl = HA2_REctbl25nit, .aid = HA2_aid7896, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 27, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl27nit, .cTbl = HA2_REctbl27nit, .aid = HA2_aid7717, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 29, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl29nit, .cTbl = HA2_REctbl29nit, .aid = HA2_aid7535, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 30, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl30nit, .cTbl = HA2_REctbl30nit, .aid = HA2_aid7469, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 32, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl32nit, .cTbl = HA2_REctbl32nit, .aid = HA2_aid7290, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 34, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl34nit, .cTbl = HA2_REctbl34nit, .aid = HA2_aid7104, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 37, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl37nit, .cTbl = HA2_REctbl37nit, .aid = HA2_aid6848, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 39, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl39nit, .cTbl = HA2_REctbl39nit, .aid = HA2_aid6669, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 41, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl41nit, .cTbl = HA2_REctbl41nit, .aid = HA2_aid6491, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 44, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl44nit, .cTbl = HA2_REctbl44nit, .aid = HA2_aid6231, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 47, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl47nit, .cTbl = HA2_REctbl47nit, .aid = HA2_aid5947, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 50, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl50nit, .cTbl = HA2_REctbl50nit, .aid = HA2_aid5675, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 53, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl53nit, .cTbl = HA2_REctbl53nit, .aid = HA2_aid5419, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 56, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl56nit, .cTbl = HA2_REctbl56nit, .aid = HA2_aid5140, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 60, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl60nit, .cTbl = HA2_REctbl60nit, .aid = HA2_aid4752, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 64, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl64nit, .cTbl = HA2_REctbl64nit, .aid = HA2_aid4383, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 68, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl68nit, .cTbl = HA2_REctbl68nit, .aid = HA2_aid4006, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 72, .refBr = 115, .cGma = gma2p15, .rTbl = HA2_RErtbl72nit, .cTbl = HA2_REctbl72nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps1, .elv = HA2_elv1, .way = W1},
	{.br = 77, .refBr = 122, .cGma = gma2p15, .rTbl = HA2_RErtbl77nit, .cTbl = HA2_REctbl77nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps2, .elv = HA2_elv2, .way = W1},
	{.br = 82, .refBr = 129, .cGma = gma2p15, .rTbl = HA2_RErtbl82nit, .cTbl = HA2_REctbl82nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps2, .elv = HA2_elv2, .way = W1},
	{.br = 87, .refBr = 136, .cGma = gma2p15, .rTbl = HA2_RErtbl87nit, .cTbl = HA2_REctbl87nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps2, .elv = HA2_elv2, .way = W1},
	{.br = 93, .refBr = 145, .cGma = gma2p15, .rTbl = HA2_RErtbl93nit, .cTbl = HA2_REctbl93nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps3, .elv = HA2_elv3, .way = W1},
	{.br = 98, .refBr = 151, .cGma = gma2p15, .rTbl = HA2_RErtbl98nit, .cTbl = HA2_REctbl98nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps3, .elv = HA2_elv3, .way = W1},
	{.br = 105, .refBr = 161, .cGma = gma2p15, .rTbl = HA2_RErtbl105nit, .cTbl = HA2_REctbl105nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps3, .elv = HA2_elv3, .way = W1},
	{.br = 111, .refBr = 167, .cGma = gma2p15, .rTbl = HA2_RErtbl111nit, .cTbl = HA2_REctbl111nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps4, .elv = HA2_elv4, .way = W1},
	{.br = 119, .refBr = 180, .cGma = gma2p15, .rTbl = HA2_RErtbl119nit, .cTbl = HA2_REctbl119nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps4, .elv = HA2_elv4, .way = W1},
	{.br = 126, .refBr = 189, .cGma = gma2p15, .rTbl = HA2_RErtbl126nit, .cTbl = HA2_REctbl126nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps5, .elv = HA2_elv5, .way = W1},
	{.br = 134, .refBr = 201, .cGma = gma2p15, .rTbl = HA2_RErtbl134nit, .cTbl = HA2_REctbl134nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps5, .elv = HA2_elv5, .way = W1},
	{.br = 143, .refBr = 210, .cGma = gma2p15, .rTbl = HA2_RErtbl143nit, .cTbl = HA2_REctbl143nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps6, .elv = HA2_elv6, .way = W1},
	{.br = 152, .refBr = 220, .cGma = gma2p15, .rTbl = HA2_RErtbl152nit, .cTbl = HA2_REctbl152nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps7, .elv = HA2_elv7, .way = W1},
	{.br = 162, .refBr = 236, .cGma = gma2p15, .rTbl = HA2_RErtbl162nit, .cTbl = HA2_REctbl162nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps7, .elv = HA2_elv7, .way = W1},
	{.br = 172, .refBr = 244, .cGma = gma2p15, .rTbl = HA2_RErtbl172nit, .cTbl = HA2_REctbl172nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
	{.br = 183, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl183nit, .cTbl = HA2_REctbl183nit, .aid = HA2_aid3622, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
	{.br = 195, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl195nit, .cTbl = HA2_REctbl195nit, .aid = HA2_aid3133, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
	{.br = 207, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl207nit, .cTbl = HA2_REctbl207nit, .aid = HA2_aid2620, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
	{.br = 220, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl220nit, .cTbl = HA2_REctbl220nit, .aid = HA2_aid2158, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
	{.br = 234, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl234nit, .cTbl = HA2_REctbl234nit, .aid = HA2_aid1611, .elvCaps = HA2_elvCaps8, .elv = HA2_elv8, .way = W1},
    {.br = 249, .refBr = 260, .cGma = gma2p15, .rTbl = HA2_RErtbl249nit, .cTbl = HA2_REctbl249nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps9, .elv = HA2_elv9,.way = W1},        // 249 ~ 360 acl off
	{.br = 265, .refBr = 273, .cGma = gma2p15, .rTbl = HA2_RErtbl265nit, .cTbl = HA2_REctbl265nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps10, .elv = HA2_elv10, .way = W1},
	{.br = 282, .refBr = 290, .cGma = gma2p15, .rTbl = HA2_RErtbl282nit, .cTbl = HA2_REctbl282nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps10, .elv = HA2_elv10, .way = W1},
	{.br = 300, .refBr = 306, .cGma = gma2p15, .rTbl = HA2_RErtbl300nit, .cTbl = HA2_REctbl300nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps11, .elv = HA2_elv11, .way = W1},
	{.br = 316, .refBr = 318, .cGma = gma2p15, .rTbl = HA2_RErtbl316nit, .cTbl = HA2_REctbl316nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps12, .elv = HA2_elv12, .way = W1},
	{.br = 333, .refBr = 338, .cGma = gma2p15, .rTbl = HA2_RErtbl333nit, .cTbl = HA2_REctbl333nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps12, .elv = HA2_elv12, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = HA2_RErtbl360nit, .cTbl = HA2_REctbl360nit, .aid = HA2_aid1005, .elvCaps = HA2_elvCaps13, .elv = HA2_elv13, .way = W2},
/*hbm interpolation */
    {.br = 382,.refBr = 382,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 407,.refBr = 407,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 433,.refBr = 433,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 461,.refBr = 461,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 491,.refBr = 491,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 517,.refBr = 517,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
    {.br = 545,.refBr = 545,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W3},      // hbm is acl on
/* hbm */
    {.br = 600,.refBr = 600,.cGma = gma2p20,.rTbl = HA2_RErtbl360nit,.cTbl = HA2_REctbl360nit,.aid = HA2_aid1005,.elvCaps = HA2_elvCaps13,.elv = HA2_elv13,.way = W4},      // hbm is acl on
};



// aid sheet in rev.B opmanual
struct SmtDimInfo dimming_info_HA3[MAX_BR_INFO] = { // add hbm array
	{.br = 2, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl2nit, .cTbl = HA3_ctbl2nit, .aid = HA3_aid9810, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1},
	{.br = 3, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl3nit, .cTbl = HA3_ctbl3nit, .aid = HA3_aid9702, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1},
	{.br = 4, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl4nit, .cTbl = HA3_ctbl4nit, .aid = HA3_aid9624, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1},
	{.br = 5, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl5nit, .cTbl = HA3_ctbl5nit, .aid = HA3_aid9512, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1},
	{.br = 6, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl6nit, .cTbl = HA3_ctbl6nit, .aid = HA3_aid9434, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 7, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl7nit, .cTbl = HA3_ctbl7nit, .aid = HA3_aid9333, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 8, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl8nit, .cTbl = HA3_ctbl8nit, .aid = HA3_aid9260, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 9, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl9nit, .cTbl = HA3_ctbl9nit, .aid = HA3_aid9186, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 10, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl10nit, .cTbl = HA3_ctbl10nit, .aid = HA3_aid9120, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 11, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl11nit, .cTbl = HA3_ctbl11nit, .aid = HA3_aid9058, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 12, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl12nit, .cTbl = HA3_ctbl12nit, .aid = HA3_aid8957, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 13, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl13nit, .cTbl = HA3_ctbl13nit, .aid = HA3_aid8891, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 14, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl14nit, .cTbl = HA3_ctbl14nit, .aid = HA3_aid8826, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 15, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl15nit, .cTbl = HA3_ctbl15nit, .aid = HA3_aid8752, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 16, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl16nit, .cTbl = HA3_ctbl16nit, .aid = HA3_aid8651, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 17, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl17nit, .cTbl = HA3_ctbl17nit, .aid = HA3_aid8574, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 19, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl19nit, .cTbl = HA3_ctbl19nit, .aid = HA3_aid8399, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 20, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl20nit, .cTbl = HA3_ctbl20nit, .aid = HA3_aid8322, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 21, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl21nit, .cTbl = HA3_ctbl21nit, .aid = HA3_aid8248, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 22, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl22nit, .cTbl = HA3_ctbl22nit, .aid = HA3_aid8147, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 24, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl24nit, .cTbl = HA3_ctbl24nit, .aid = HA3_aid8000, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 25, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl25nit, .cTbl = HA3_ctbl25nit, .aid = HA3_aid7895, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 27, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl27nit, .cTbl = HA3_ctbl27nit, .aid = HA3_aid7713, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl29nit, .cTbl = HA3_ctbl29nit, .aid = HA3_aid7558, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 30, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl30nit, .cTbl = HA3_ctbl30nit, .aid = HA3_aid7457, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 32, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl32nit, .cTbl = HA3_ctbl32nit, .aid = HA3_aid7279, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 34, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl34nit, .cTbl = HA3_ctbl34nit, .aid = HA3_aid7128, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 37, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl37nit, .cTbl = HA3_ctbl37nit, .aid = HA3_aid6845, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 39, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl39nit, .cTbl = HA3_ctbl39nit, .aid = HA3_aid6702, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 41, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl41nit, .cTbl = HA3_ctbl41nit, .aid = HA3_aid6519, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 44, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl44nit, .cTbl = HA3_ctbl44nit, .aid = HA3_aid6256, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 47, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl47nit, .cTbl = HA3_ctbl47nit, .aid = HA3_aid5977, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 50, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl50nit, .cTbl = HA3_ctbl50nit, .aid = HA3_aid5709, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 53, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl53nit, .cTbl = HA3_ctbl53nit, .aid = HA3_aid5465, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 56, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl56nit, .cTbl = HA3_ctbl56nit, .aid = HA3_aid5171, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 60, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl60nit, .cTbl = HA3_ctbl60nit, .aid = HA3_aid4833, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 64, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl64nit, .cTbl = HA3_ctbl64nit, .aid = HA3_aid4771, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 68, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl68nit, .cTbl = HA3_ctbl68nit, .aid = HA3_aid4074, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 72, .refBr = 113, .cGma = gma2p15, .rTbl = HA3_rtbl72nit, .cTbl = HA3_ctbl72nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 77, .refBr = 121, .cGma = gma2p15, .rTbl = HA3_rtbl77nit, .cTbl = HA3_ctbl77nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 82, .refBr = 129, .cGma = gma2p15, .rTbl = HA3_rtbl82nit, .cTbl = HA3_ctbl82nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 87, .refBr = 136, .cGma = gma2p15, .rTbl = HA3_rtbl87nit, .cTbl = HA3_ctbl87nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1},
	{.br = 93, .refBr = 144, .cGma = gma2p15, .rTbl = HA3_rtbl93nit, .cTbl = HA3_ctbl93nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1},
	{.br = 98, .refBr = 151, .cGma = gma2p15, .rTbl = HA3_rtbl98nit, .cTbl = HA3_ctbl98nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1},
	{.br = 105, .refBr = 162, .cGma = gma2p15, .rTbl = HA3_rtbl105nit, .cTbl = HA3_ctbl105nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1},
	{.br = 111, .refBr = 167, .cGma = gma2p15, .rTbl = HA3_rtbl111nit, .cTbl = HA3_ctbl111nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1},
	{.br = 119, .refBr = 180, .cGma = gma2p15, .rTbl = HA3_rtbl119nit, .cTbl = HA3_ctbl119nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1},
	{.br = 126, .refBr = 191, .cGma = gma2p15, .rTbl = HA3_rtbl126nit, .cTbl = HA3_ctbl126nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1},
	{.br = 134, .refBr = 199, .cGma = gma2p15, .rTbl = HA3_rtbl134nit, .cTbl = HA3_ctbl134nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1},
	{.br = 143, .refBr = 209, .cGma = gma2p15, .rTbl = HA3_rtbl143nit, .cTbl = HA3_ctbl143nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1},
	{.br = 152, .refBr = 222, .cGma = gma2p15, .rTbl = HA3_rtbl152nit, .cTbl = HA3_ctbl152nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1},
	{.br = 162, .refBr = 239, .cGma = gma2p15, .rTbl = HA3_rtbl162nit, .cTbl = HA3_ctbl162nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1},
	{.br = 172, .refBr = 247, .cGma = gma2p15, .rTbl = HA3_rtbl172nit, .cTbl = HA3_ctbl172nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps6, .elv = HA3_elv6, .way = W1},
	{.br = 183, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl183nit, .cTbl = HA3_ctbl183nit, .aid = HA3_aid3721, .elvCaps = HA3_elvCaps6, .elv = HA3_elv6, .way = W1},
	{.br = 195, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl195nit, .cTbl = HA3_ctbl195nit, .aid = HA3_aid3260, .elvCaps = HA3_elvCaps6, .elv = HA3_elv6, .way = W1},
	{.br = 207, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl207nit, .cTbl = HA3_ctbl207nit, .aid = HA3_aid2814, .elvCaps = HA3_elvCaps7, .elv = HA3_elv7, .way = W1},
	{.br = 220, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl220nit, .cTbl = HA3_ctbl220nit, .aid = HA3_aid2264, .elvCaps = HA3_elvCaps7, .elv = HA3_elv7, .way = W1},
	{.br = 234, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl234nit, .cTbl = HA3_ctbl234nit, .aid = HA3_aid1694, .elvCaps = HA3_elvCaps7, .elv = HA3_elv7, .way = W1},
    {.br = 249, .refBr = 262, .cGma = gma2p15, .rTbl = HA3_rtbl249nit, .cTbl = HA3_ctbl249nit, .aid = HA3_aid1058, .elvCaps = HA3_elvCaps7, .elv = HA3_elv7, .way = W1},    // 249 ~ 360 acl off
	{.br = 265, .refBr = 275, .cGma = gma2p15, .rTbl = HA3_rtbl265nit, .cTbl = HA3_ctbl265nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps8, .elv = HA3_elv8, .way = W1},
	{.br = 282, .refBr = 292, .cGma = gma2p15, .rTbl = HA3_rtbl282nit, .cTbl = HA3_ctbl282nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps9, .elv = HA3_elv9, .way = W1},
	{.br = 300, .refBr = 308, .cGma = gma2p15, .rTbl = HA3_rtbl300nit, .cTbl = HA3_ctbl300nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps9, .elv = HA3_elv9, .way = W1},
	{.br = 316, .refBr = 323, .cGma = gma2p15, .rTbl = HA3_rtbl316nit, .cTbl = HA3_ctbl316nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps10, .elv = HA3_elv10, .way = W1},
	{.br = 333, .refBr = 341, .cGma = gma2p15, .rTbl = HA3_rtbl333nit, .cTbl = HA3_ctbl333nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps10, .elv = HA3_elv10, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = HA3_rtbl360nit, .cTbl = HA3_ctbl360nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps11, .elv = HA3_elv11, .way = W2},
/*hbm interpolation */
    {.br = 382,.refBr = 382,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 407,.refBr = 407,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 433,.refBr = 433,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 461,.refBr = 461,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 491,.refBr = 491,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 517,.refBr = 517,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
    {.br = 545,.refBr = 545,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W3},  // hbm is acl on
/* hbm */
    {.br = 600,.refBr = 600,.cGma = gma2p20,.rTbl = HA3_rtbl360nit,.cTbl = HA3_ctbl360nit,.aid = HA3_aid1004,.elvCaps = HA3_elvCaps11,.elv = HA3_elv11,.way = W4},  // hbm is acl on
};

// aid sheet in rev.A
struct SmtDimInfo dimming_info_HF3[MAX_BR_INFO] = { // add hbm array
	{.br = 2, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl2nit, .cTbl = HF3_ctbl2nit, .aid = HF3_aid9779, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1},
	{.br = 3, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl3nit, .cTbl = HF3_ctbl3nit, .aid = HF3_aid9701, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1},
	{.br = 4, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl4nit, .cTbl = HF3_ctbl4nit, .aid = HF3_aid9585, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1},
	{.br = 5, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl5nit, .cTbl = HF3_ctbl5nit, .aid = HF3_aid9507, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1},
	{.br = 6, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl6nit, .cTbl = HF3_ctbl6nit, .aid = HF3_aid9394, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 7, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl7nit, .cTbl = HF3_ctbl7nit, .aid = HF3_aid9328, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 8, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl8nit, .cTbl = HF3_ctbl8nit, .aid = HF3_aid9259, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 9, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl9nit, .cTbl = HF3_ctbl9nit, .aid = HF3_aid9154, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 10, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl10nit, .cTbl = HF3_ctbl10nit, .aid = HF3_aid9088, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 11, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl11nit, .cTbl = HF3_ctbl11nit, .aid = HF3_aid9026, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 12, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl12nit, .cTbl = HF3_ctbl12nit, .aid = HF3_aid8956, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 13, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl13nit, .cTbl = HF3_ctbl13nit, .aid = HF3_aid8894, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 14, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl14nit, .cTbl = HF3_ctbl14nit, .aid = HF3_aid8839, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 15, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl15nit, .cTbl = HF3_ctbl15nit, .aid = HF3_aid8754, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 16, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl16nit, .cTbl = HF3_ctbl16nit, .aid = HF3_aid8661, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 17, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl17nit, .cTbl = HF3_ctbl17nit, .aid = HF3_aid8587, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 19, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl19nit, .cTbl = HF3_ctbl19nit, .aid = HF3_aid8408, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 20, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl20nit, .cTbl = HF3_ctbl20nit, .aid = HF3_aid8335, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 21, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl21nit, .cTbl = HF3_ctbl21nit, .aid = HF3_aid8249, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 22, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl22nit, .cTbl = HF3_ctbl22nit, .aid = HF3_aid8129, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 24, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl24nit, .cTbl = HF3_ctbl24nit, .aid = HF3_aid7974, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 25, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl25nit, .cTbl = HF3_ctbl25nit, .aid = HF3_aid7904, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 27, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl27nit, .cTbl = HF3_ctbl27nit, .aid = HF3_aid7694, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 29, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl29nit, .cTbl = HF3_ctbl29nit, .aid = HF3_aid7578, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 30, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl30nit, .cTbl = HF3_ctbl30nit, .aid = HF3_aid7469, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 32, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl32nit, .cTbl = HF3_ctbl32nit, .aid = HF3_aid7263, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 34, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl34nit, .cTbl = HF3_ctbl34nit, .aid = HF3_aid7147, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 37, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl37nit, .cTbl = HF3_ctbl37nit, .aid = HF3_aid6828, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 39, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl39nit, .cTbl = HF3_ctbl39nit, .aid = HF3_aid6712, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 41, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl41nit, .cTbl = HF3_ctbl41nit, .aid = HF3_aid6510, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 44, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl44nit, .cTbl = HF3_ctbl44nit, .aid = HF3_aid6238, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 47, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl47nit, .cTbl = HF3_ctbl47nit, .aid = HF3_aid5982, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 50, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl50nit, .cTbl = HF3_ctbl50nit, .aid = HF3_aid5707, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 53, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl53nit, .cTbl = HF3_ctbl53nit, .aid = HF3_aid5427, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 56, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl56nit, .cTbl = HF3_ctbl56nit, .aid = HF3_aid5163, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 60, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl60nit, .cTbl = HF3_ctbl60nit, .aid = HF3_aid4802, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 64, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl64nit, .cTbl = HF3_ctbl64nit, .aid = HF3_aid4425, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 68, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl68nit, .cTbl = HF3_ctbl68nit, .aid = HF3_aid4057, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 72, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl72nit, .cTbl = HF3_ctbl72nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1},
	{.br = 77, .refBr = 117, .cGma = gma2p15, .rTbl = HF3_rtbl77nit, .cTbl = HF3_ctbl77nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1},
	{.br = 82, .refBr = 124, .cGma = gma2p15, .rTbl = HF3_rtbl82nit, .cTbl = HF3_ctbl82nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1},
	{.br = 87, .refBr = 131, .cGma = gma2p15, .rTbl = HF3_rtbl87nit, .cTbl = HF3_ctbl87nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1},
	{.br = 93, .refBr = 140, .cGma = gma2p15, .rTbl = HF3_rtbl93nit, .cTbl = HF3_ctbl93nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1},
	{.br = 98, .refBr = 148, .cGma = gma2p15, .rTbl = HF3_rtbl98nit, .cTbl = HF3_ctbl98nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1},
	{.br = 105, .refBr = 155, .cGma = gma2p15, .rTbl = HF3_rtbl105nit, .cTbl = HF3_ctbl105nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1},
	{.br = 111, .refBr = 164, .cGma = gma2p15, .rTbl = HF3_rtbl110nit, .cTbl = HF3_ctbl110nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1},
	{.br = 119, .refBr = 175, .cGma = gma2p15, .rTbl = HF3_rtbl119nit, .cTbl = HF3_ctbl119nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1},
	{.br = 126, .refBr = 181, .cGma = gma2p15, .rTbl = HF3_rtbl126nit, .cTbl = HF3_ctbl126nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1},
	{.br = 134, .refBr = 193, .cGma = gma2p15, .rTbl = HF3_rtbl134nit, .cTbl = HF3_ctbl134nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1},
	{.br = 143, .refBr = 203, .cGma = gma2p15, .rTbl = HF3_rtbl143nit, .cTbl = HF3_ctbl143nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps6, .elv = HF3_elv6, .way = W1},
	{.br = 152, .refBr = 215, .cGma = gma2p15, .rTbl = HF3_rtbl152nit, .cTbl = HF3_ctbl152nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1},
	{.br = 162, .refBr = 227, .cGma = gma2p15, .rTbl = HF3_rtbl162nit, .cTbl = HF3_ctbl162nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1},
	{.br = 172, .refBr = 242, .cGma = gma2p15, .rTbl = HF3_rtbl172nit, .cTbl = HF3_ctbl172nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 183, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl183nit, .cTbl = HF3_ctbl183nit, .aid = HF3_aid3653, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 195, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl195nit, .cTbl = HF3_ctbl195nit, .aid = HF3_aid3222, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 207, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl207nit, .cTbl = HF3_ctbl207nit, .aid = HF3_aid2741, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 220, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl220nit, .cTbl = HF3_ctbl220nit, .aid = HF3_aid2151, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 234, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl234nit, .cTbl = HF3_ctbl234nit, .aid = HF3_aid1568, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1},
	{.br = 249, .refBr = 254, .cGma = gma2p15, .rTbl = HF3_rtbl249nit, .cTbl = HF3_ctbl249nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps9, .elv = HF3_elv9, .way = W1},	// 249 ~ 360 acl off
	{.br = 265, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl265nit, .cTbl = HF3_ctbl265nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1},
	{.br = 282, .refBr = 288, .cGma = gma2p15, .rTbl = HF3_rtbl282nit, .cTbl = HF3_ctbl282nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1},
	{.br = 300, .refBr = 304, .cGma = gma2p15, .rTbl = HF3_rtbl300nit, .cTbl = HF3_ctbl300nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps11, .elv = HF3_elv11, .way = W1},
	{.br = 316, .refBr = 317, .cGma = gma2p15, .rTbl = HF3_rtbl316nit, .cTbl = HF3_ctbl316nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1},
	{.br = 333, .refBr = 337, .cGma = gma2p15, .rTbl = HF3_rtbl333nit, .cTbl = HF3_ctbl333nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = HF3_rtbl360nit, .cTbl = HF3_ctbl360nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W2},
/*hbm interpolation */
	{.br = 382,.refBr = 382,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 407,.refBr = 407,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 433,.refBr = 433,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 461,.refBr = 461,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 491,.refBr = 491,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 517,.refBr = 517,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
	{.br = 545,.refBr = 545,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W3},	// hbm is acl on
/* hbm */
	{.br = 600,.refBr = 600,.cGma = gma2p20,.rTbl = HF3_rtbl360nit,.cTbl = HF3_ctbl360nit,.aid = HF3_aid1005,.elvCaps = HF3_elvCaps13,.elv = HF3_elv13,.way = W4},	// hbm is acl on
};


static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
        int     i, j;
        int     ret = 0;
        unsigned int index = 0;
        unsigned char *result = brInfo->gamma;

        result[index++] = OLED_CMD_GAMMA;

        for (i = V255; i >= V0; i--) {
                for (j = 0; j < CI_MAX; j++) {
                        if (i == V255) {
                                result[index++] = (unsigned char) ((center_gamma[i][j] >> 8) & 0x01);
                                result[index++] = (unsigned char) center_gamma[i][j] & 0xff;
                        }
                        else {
                                result[index++] = (unsigned char) center_gamma[i][j] & 0xff;
                        }
                }
        }
        result[index++] = 0x00;
        result[index++] = 0x00;

        return ret;
}


static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, u8 * hbm)
{
        int     ret = 0;
        unsigned int index = 0;
        unsigned char *result = brInfo->gamma;

        memset(result, 0, OLED_CMD_GAMMA_CNT);

        result[index++] = OLED_CMD_GAMMA;

        memcpy(result + 1, hbm, S6E3HA2_HBMGAMMA_LEN);

        return ret;
}

/* gamma interpolaion table */
const unsigned int tbl_hbm_inter[7] = {
        94, 201, 311, 431, 559, 670, 789
};

static int interpolation_gamma_to_hbm(struct SmtDimInfo *dimInfo, int br_idx)
{
        int     i, j;
        int     ret = 0;
        int     idx = 0;
        int     tmp = 0;
        int     hbmcnt, refcnt, gap = 0;
        int     ref_idx = 0;
        int     hbm_idx = 0;
        int     rst = 0;
        int     hbm_tmp, ref_tmp;
        unsigned char *result = dimInfo[br_idx].gamma;

        for (i = 0; i < MAX_BR_INFO; i++) {
                if (dimInfo[i].br == S6E3HA2_MAX_BRIGHTNESS)
                        ref_idx = i;

                if (dimInfo[i].br == S6E3HA2_HBM_BRIGHTNESS)
                        hbm_idx = i;
        }

        if ((ref_idx == 0) || (hbm_idx == 0)) {
                dsim_info("%s failed to get index ref index : %d, hbm index : %d\n", __func__, ref_idx, hbm_idx);
                ret = -EINVAL;
                goto exit;
        }

        result[idx++] = OLED_CMD_GAMMA;
        tmp = (br_idx - ref_idx) - 1;

        hbmcnt = 1;
        refcnt = 1;

        for (i = V255; i >= V0; i--) {
                for (j = 0; j < CI_MAX; j++) {
                        if (i == V255) {
                                hbm_tmp = (dimInfo[hbm_idx].gamma[hbmcnt] << 8) | (dimInfo[hbm_idx].gamma[hbmcnt + 1]);
                                ref_tmp = (dimInfo[ref_idx].gamma[refcnt] << 8) | (dimInfo[ref_idx].gamma[refcnt + 1]);

                                if (hbm_tmp > ref_tmp) {
                                        gap = hbm_tmp - ref_tmp;
                                        rst = (gap * tbl_hbm_inter[tmp]) >> 10;
                                        rst += ref_tmp;
                                }
                                else {
                                        gap = ref_tmp - hbm_tmp;
                                        rst = (gap * tbl_hbm_inter[tmp]) >> 10;
                                        rst = ref_tmp - rst;
                                }
                                result[idx++] = (unsigned char) ((rst >> 8) & 0x01);
                                result[idx++] = (unsigned char) rst & 0xff;
                                hbmcnt += 2;
                                refcnt += 2;
                        }
                        else {
                                hbm_tmp = dimInfo[hbm_idx].gamma[hbmcnt++];
                                ref_tmp = dimInfo[ref_idx].gamma[refcnt++];

                                if (hbm_tmp > ref_tmp) {
                                        gap = hbm_tmp - ref_tmp;
                                        rst = (gap * tbl_hbm_inter[tmp]) >> 10;
                                        rst += ref_tmp;
                                }
                                else {
                                        gap = ref_tmp - hbm_tmp;
                                        rst = (gap * tbl_hbm_inter[tmp]) >> 10;
                                        rst = ref_tmp - rst;
                                }
                                result[idx++] = (unsigned char) rst & 0xff;
                        }
                }
        }

        dsim_info("tmp index : %d\n", tmp);

      exit:
        return ret;
}
static int init_dimming(struct dsim_device *dsim, u8 * mtp, u8 * hbm)
{
        int     i, j;
        int     pos = 0;
        int     ret = 0;
        short   temp;
        int     method = 0;
        struct dim_data *dimming;
        unsigned char panelrev = 0x00;
        struct panel_private *panel = &dsim->priv;
        struct SmtDimInfo *diminfo = NULL;
        int     string_offset;
        char    string_buf[1024];

        dimming = (struct dim_data *) kmalloc(sizeof(struct dim_data), GFP_KERNEL);
        if (!dimming) {
                dsim_err("failed to allocate memory for dim data\n");
                ret = -ENOMEM;
                goto error;
        }

        panelrev = panel->id[2] & 0x0F;
        dsim_info("%s : Panel rev : %d\n", __func__, panelrev);

        switch (dynamic_lcd_type) {
                case LCD_TYPE_S6E3HA2_WQHD:
                        dsim_info("%s init dimming info for daisy HA2 rev.E panel\n", __func__);
                        diminfo = (void *) dimming_info_HA2_RE;
                        panel->acl_opr_tbl = (unsigned char **) ACL_OPR_TABLE_HA2;
						memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA2, sizeof(aid_dimming_dynamic.vint_dim_offset));
						memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA2, sizeof(aid_dimming_dynamic.elvss_minus_offset));
                        break;
                case LCD_TYPE_S6E3HA3_WQHD:
                        dsim_info("%s init dimming info for daisy HA3 pre panel\n", __func__);
                        diminfo = (void *) dimming_info_HA3;
                        panel->acl_opr_tbl = (unsigned char **) ACL_OPR_TABLE_HA3;
						memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA3, sizeof(aid_dimming_dynamic.vint_dim_offset));
						memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA3, sizeof(aid_dimming_dynamic.elvss_minus_offset));
                        break;
                case LCD_TYPE_S6E3HF3_WQHD:
                        dsim_info("%s init dimming info for daisy HF3 panel\n", __func__);
                        diminfo = (void *) dimming_info_HF3;
                        panel->acl_opr_tbl = (unsigned char **) ACL_OPR_TABLE_HF3;
						memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HF3, sizeof(aid_dimming_dynamic.vint_dim_offset));
						memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HF3, sizeof(aid_dimming_dynamic.elvss_minus_offset));
                        break;
                default:
                        dsim_info("%s init dimming info for daisy (UNKNOWN) HA2 panel\n", __func__);
                        diminfo = (void *) dimming_info_HA2_RE;
                        panel->acl_opr_tbl = (unsigned char **) ACL_OPR_TABLE_HA2;
						memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA2, sizeof(aid_dimming_dynamic.vint_dim_offset));
						memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA2, sizeof(aid_dimming_dynamic.elvss_minus_offset));
                        break;
        }


        panel->dim_data = (void *) dimming;
        panel->dim_info = (void *) diminfo;
        panel->br_tbl = (unsigned int *) br_tbl;
        panel->hbm_inter_br_tbl = (unsigned int *) hbm_interpolation_br_tbl;
        panel->hbm_tbl = (unsigned char **) HBM_TABLE;
        panel->acl_cutoff_tbl = (unsigned char **) ACL_CUTOFF_TABLE;
        panel->hbm_index = MAX_BR_INFO - 1;
        panel->hbm_elvss_comp = S6E3HA2_HBM_ELVSS_COMP;

        for (j = 0; j < CI_MAX; j++) {
                temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
                dimming->t_gamma[V255][j] = (int) center_gamma[V255][j] + temp;
                dimming->mtp[V255][j] = temp;
                pos += 2;
        }

        for (i = V203; i >= V0; i--) {
                for (j = 0; j < CI_MAX; j++) {
                        temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
                        dimming->t_gamma[i][j] = (int) center_gamma[i][j] + temp;
                        dimming->mtp[i][j] = temp;
                        pos++;
                }
        }
        /* for vt */
        temp = (mtp[pos + 1]) << 8 | mtp[pos];

        for (i = 0; i < CI_MAX; i++)
                dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;
#ifdef SMART_DIMMING_DEBUG
        dimm_info("Center Gamma Info : \n");
        for (i = 0; i < VMAX; i++) {
                dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
                          dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
                          dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
        }
#endif
        dimm_info("VT MTP : \n");
        dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
                  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
                  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

        dimm_info("MTP Info : \n");
        for (i = 0; i < VMAX; i++) {
                dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
                          dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
                          dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
        }

        ret = generate_volt_table(dimming);
        if (ret) {
                dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
                goto error;
        }

        for (i = 0; i < MAX_BR_INFO; i++) {
                method = diminfo[i].way;

                if (method == DIMMING_METHOD_FILL_CENTER) {
                        ret = set_gamma_to_center(&diminfo[i]);
                        if (ret) {
                                dsim_err("%s : failed to get center gamma\n", __func__);
                                goto error;
                        }
                }
                else if (method == DIMMING_METHOD_FILL_HBM) {
                        ret = set_gamma_to_hbm(&diminfo[i], hbm);
                        if (ret) {
                                dsim_err("%s : failed to get hbm gamma\n", __func__);
                                goto error;
                        }
                }
        }

        for (i = 0; i < MAX_BR_INFO; i++) {
                method = diminfo[i].way;
                if (method == DIMMING_METHOD_AID) {
                        ret = cal_gamma_from_index(dimming, &diminfo[i]);
                        if (ret) {
                                dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
                                goto error;
                        }
                }
                if (method == DIMMING_METHOD_INTERPOLATION) {
                        ret = interpolation_gamma_to_hbm(diminfo, i);
                        if (ret) {
                                dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
                                goto error;
                        }
                }
        }

        for (i = 0; i < MAX_BR_INFO; i++) {
                memset(string_buf, 0, sizeof(string_buf));
                string_offset = sprintf(string_buf, "gamma[%3d] : ", diminfo[i].br);

                for (j = 0; j < GAMMA_CMD_CNT; j++)
                        string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);

                dsim_info("%s\n", string_buf);
        }

      error:
        return ret;

}


#ifdef CONFIG_LCD_HMT
static const unsigned int hmt_br_tbl[256] = {
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 12, 12,
        13, 13, 14, 14, 14, 15, 15, 16, 16, 16, 17, 17, 17, 17, 17, 19,
        19, 20, 20, 21, 21, 21, 22, 22, 23, 23, 23, 23, 23, 25, 25, 25,
        25, 25, 27, 27, 27, 27, 27, 29, 29, 29, 29, 29, 31, 31, 31, 31,
        31, 33, 33, 33, 33, 35, 35, 35, 35, 35, 37, 37, 37, 37, 37, 39,
        39, 39, 39, 39, 41, 41, 41, 41, 41, 41, 41, 44, 44, 44, 44, 44,
        44, 44, 44, 47, 47, 47, 47, 47, 47, 47, 50, 50, 50, 50, 50, 50,
        50, 53, 53, 53, 53, 53, 53, 53, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 68, 68, 68, 68, 68, 68, 68, 68, 68, 72,
        72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 82, 82, 82, 82, 82, 82, 82, 82,
        82, 82, 82, 82, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87,
        87, 87, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
        93, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 105
};

struct SmtDimInfo hmt_dimming_info_HA2[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 43, .cGma = gma2p15, .rTbl = HA2_HMTrtbl10nit, .cTbl = HA2_HMTctbl10nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 11, .refBr = 48, .cGma = gma2p15, .rTbl = HA2_HMTrtbl11nit, .cTbl = HA2_HMTctbl11nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 12, .refBr = 51, .cGma = gma2p15, .rTbl = HA2_HMTrtbl12nit, .cTbl = HA2_HMTctbl12nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 13, .refBr = 55, .cGma = gma2p15, .rTbl = HA2_HMTrtbl13nit, .cTbl = HA2_HMTctbl13nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 14, .refBr = 60, .cGma = gma2p15, .rTbl = HA2_HMTrtbl14nit, .cTbl = HA2_HMTctbl14nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 15, .refBr = 61, .cGma = gma2p15, .rTbl = HA2_HMTrtbl15nit, .cTbl = HA2_HMTctbl15nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 16, .refBr = 64, .cGma = gma2p15, .rTbl = HA2_HMTrtbl16nit, .cTbl = HA2_HMTctbl16nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 17, .refBr = 68, .cGma = gma2p15, .rTbl = HA2_HMTrtbl17nit, .cTbl = HA2_HMTctbl17nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 19, .refBr = 75, .cGma = gma2p15, .rTbl = HA2_HMTrtbl19nit, .cTbl = HA2_HMTctbl19nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 20, .refBr = 78, .cGma = gma2p15, .rTbl = HA2_HMTrtbl20nit, .cTbl = HA2_HMTctbl20nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 21, .refBr = 80, .cGma = gma2p15, .rTbl = HA2_HMTrtbl21nit, .cTbl = HA2_HMTctbl21nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 22, .refBr = 86, .cGma = gma2p15, .rTbl = HA2_HMTrtbl22nit, .cTbl = HA2_HMTctbl22nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 23, .refBr = 89, .cGma = gma2p15, .rTbl = HA2_HMTrtbl23nit, .cTbl = HA2_HMTctbl23nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 25, .refBr = 96, .cGma = gma2p15, .rTbl = HA2_HMTrtbl25nit, .cTbl = HA2_HMTctbl25nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 27, .refBr = 103, .cGma = gma2p15, .rTbl = HA2_HMTrtbl27nit, .cTbl = HA2_HMTctbl27nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 29, .refBr = 108, .cGma = gma2p15, .rTbl = HA2_HMTrtbl29nit, .cTbl = HA2_HMTctbl29nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 31, .refBr = 114, .cGma = gma2p15, .rTbl = HA2_HMTrtbl31nit, .cTbl = HA2_HMTctbl31nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 33, .refBr = 121, .cGma = gma2p15, .rTbl = HA2_HMTrtbl33nit, .cTbl = HA2_HMTctbl33nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 35, .refBr = 129, .cGma = gma2p15, .rTbl = HA2_HMTrtbl35nit, .cTbl = HA2_HMTctbl35nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 37, .refBr = 137, .cGma = gma2p15, .rTbl = HA2_HMTrtbl37nit, .cTbl = HA2_HMTctbl37nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 39, .refBr = 144, .cGma = gma2p15, .rTbl = HA2_HMTrtbl39nit, .cTbl = HA2_HMTctbl39nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 41, .refBr = 146, .cGma = gma2p15, .rTbl = HA2_HMTrtbl41nit, .cTbl = HA2_HMTctbl41nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 44, .refBr = 155, .cGma = gma2p15, .rTbl = HA2_HMTrtbl44nit, .cTbl = HA2_HMTctbl44nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 47, .refBr = 166, .cGma = gma2p15, .rTbl = HA2_HMTrtbl47nit, .cTbl = HA2_HMTctbl47nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 50, .refBr = 175, .cGma = gma2p15, .rTbl = HA2_HMTrtbl50nit, .cTbl = HA2_HMTctbl50nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 53, .refBr = 186, .cGma = gma2p15, .rTbl = HA2_HMTrtbl53nit, .cTbl = HA2_HMTctbl53nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 56, .refBr = 198, .cGma = gma2p15, .rTbl = HA2_HMTrtbl56nit, .cTbl = HA2_HMTctbl56nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 60, .refBr = 211, .cGma = gma2p15, .rTbl = HA2_HMTrtbl60nit, .cTbl = HA2_HMTctbl60nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 64, .refBr = 223, .cGma = gma2p15, .rTbl = HA2_HMTrtbl64nit, .cTbl = HA2_HMTctbl64nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 68, .refBr = 237, .cGma = gma2p15, .rTbl = HA2_HMTrtbl68nit, .cTbl = HA2_HMTctbl68nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 72, .refBr = 250, .cGma = gma2p15, .rTbl = HA2_HMTrtbl72nit, .cTbl = HA2_HMTctbl72nit, .aid = HA2_HMTaid8001, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 77, .refBr = 189, .cGma = gma2p15, .rTbl = HA2_HMTrtbl77nit, .cTbl = HA2_HMTctbl77nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 82, .refBr = 202, .cGma = gma2p15, .rTbl = HA2_HMTrtbl82nit, .cTbl = HA2_HMTctbl82nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 87, .refBr = 213, .cGma = gma2p15, .rTbl = HA2_HMTrtbl87nit, .cTbl = HA2_HMTctbl87nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 93, .refBr = 226, .cGma = gma2p15, .rTbl = HA2_HMTrtbl93nit, .cTbl = HA2_HMTctbl93nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 99, .refBr = 238, .cGma = gma2p15, .rTbl = HA2_HMTrtbl99nit, .cTbl = HA2_HMTctbl99nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
	{.br = 105, .refBr = 251, .cGma = gma2p15, .rTbl = HA2_HMTrtbl105nit, .cTbl = HA2_HMTctbl105nit, .aid = HA2_HMTaid7003, .elvCaps = HA2_HMTelvCaps, .elv = HA2_HMTelv},
};


struct SmtDimInfo hmt_dimming_info_HA3[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 43, .cGma = gma2p15, .rTbl = HA3_HMTrtbl10nit, .cTbl = HA3_HMTctbl10nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 11, .refBr = 48, .cGma = gma2p15, .rTbl = HA3_HMTrtbl11nit, .cTbl = HA3_HMTctbl11nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 12, .refBr = 51, .cGma = gma2p15, .rTbl = HA3_HMTrtbl12nit, .cTbl = HA3_HMTctbl12nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 13, .refBr = 55, .cGma = gma2p15, .rTbl = HA3_HMTrtbl13nit, .cTbl = HA3_HMTctbl13nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 14, .refBr = 60, .cGma = gma2p15, .rTbl = HA3_HMTrtbl14nit, .cTbl = HA3_HMTctbl14nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 15, .refBr = 61, .cGma = gma2p15, .rTbl = HA3_HMTrtbl15nit, .cTbl = HA3_HMTctbl15nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 16, .refBr = 64, .cGma = gma2p15, .rTbl = HA3_HMTrtbl16nit, .cTbl = HA3_HMTctbl16nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 17, .refBr = 68, .cGma = gma2p15, .rTbl = HA3_HMTrtbl17nit, .cTbl = HA3_HMTctbl17nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 19, .refBr = 75, .cGma = gma2p15, .rTbl = HA3_HMTrtbl19nit, .cTbl = HA3_HMTctbl19nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 20, .refBr = 78, .cGma = gma2p15, .rTbl = HA3_HMTrtbl20nit, .cTbl = HA3_HMTctbl20nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 21, .refBr = 80, .cGma = gma2p15, .rTbl = HA3_HMTrtbl21nit, .cTbl = HA3_HMTctbl21nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 22, .refBr = 86, .cGma = gma2p15, .rTbl = HA3_HMTrtbl22nit, .cTbl = HA3_HMTctbl22nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 23, .refBr = 89, .cGma = gma2p15, .rTbl = HA3_HMTrtbl23nit, .cTbl = HA3_HMTctbl23nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 25, .refBr = 96, .cGma = gma2p15, .rTbl = HA3_HMTrtbl25nit, .cTbl = HA3_HMTctbl25nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 27, .refBr = 103, .cGma = gma2p15, .rTbl = HA3_HMTrtbl27nit, .cTbl = HA3_HMTctbl27nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 29, .refBr = 108, .cGma = gma2p15, .rTbl = HA3_HMTrtbl29nit, .cTbl = HA3_HMTctbl29nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 31, .refBr = 114, .cGma = gma2p15, .rTbl = HA3_HMTrtbl31nit, .cTbl = HA3_HMTctbl31nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 33, .refBr = 121, .cGma = gma2p15, .rTbl = HA3_HMTrtbl33nit, .cTbl = HA3_HMTctbl33nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 35, .refBr = 129, .cGma = gma2p15, .rTbl = HA3_HMTrtbl35nit, .cTbl = HA3_HMTctbl35nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 37, .refBr = 137, .cGma = gma2p15, .rTbl = HA3_HMTrtbl37nit, .cTbl = HA3_HMTctbl37nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 39, .refBr = 144, .cGma = gma2p15, .rTbl = HA3_HMTrtbl39nit, .cTbl = HA3_HMTctbl39nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 41, .refBr = 146, .cGma = gma2p15, .rTbl = HA3_HMTrtbl41nit, .cTbl = HA3_HMTctbl41nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 44, .refBr = 155, .cGma = gma2p15, .rTbl = HA3_HMTrtbl44nit, .cTbl = HA3_HMTctbl44nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 47, .refBr = 166, .cGma = gma2p15, .rTbl = HA3_HMTrtbl47nit, .cTbl = HA3_HMTctbl47nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 50, .refBr = 175, .cGma = gma2p15, .rTbl = HA3_HMTrtbl50nit, .cTbl = HA3_HMTctbl50nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 53, .refBr = 186, .cGma = gma2p15, .rTbl = HA3_HMTrtbl53nit, .cTbl = HA3_HMTctbl53nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 56, .refBr = 198, .cGma = gma2p15, .rTbl = HA3_HMTrtbl56nit, .cTbl = HA3_HMTctbl56nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 60, .refBr = 211, .cGma = gma2p15, .rTbl = HA3_HMTrtbl60nit, .cTbl = HA3_HMTctbl60nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 64, .refBr = 223, .cGma = gma2p15, .rTbl = HA3_HMTrtbl64nit, .cTbl = HA3_HMTctbl64nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 68, .refBr = 237, .cGma = gma2p15, .rTbl = HA3_HMTrtbl68nit, .cTbl = HA3_HMTctbl68nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 72, .refBr = 250, .cGma = gma2p15, .rTbl = HA3_HMTrtbl72nit, .cTbl = HA3_HMTctbl72nit, .aid = HA3_HMTaid8001, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 77, .refBr = 189, .cGma = gma2p15, .rTbl = HA3_HMTrtbl77nit, .cTbl = HA3_HMTctbl77nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 82, .refBr = 202, .cGma = gma2p15, .rTbl = HA3_HMTrtbl82nit, .cTbl = HA3_HMTctbl82nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 87, .refBr = 213, .cGma = gma2p15, .rTbl = HA3_HMTrtbl87nit, .cTbl = HA3_HMTctbl87nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 93, .refBr = 226, .cGma = gma2p15, .rTbl = HA3_HMTrtbl93nit, .cTbl = HA3_HMTctbl93nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 99, .refBr = 238, .cGma = gma2p15, .rTbl = HA3_HMTrtbl99nit, .cTbl = HA3_HMTctbl99nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 105, .refBr = 251, .cGma = gma2p15, .rTbl = HA3_HMTrtbl105nit, .cTbl = HA3_HMTctbl105nit, .aid = HA3_HMTaid7003, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
};

struct SmtDimInfo hmt_dimming_info_HF3[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 43, .cGma = gma2p15, .rTbl = HF3_HMTrtbl10nit, .cTbl = HF3_HMTctbl10nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 11, .refBr = 48, .cGma = gma2p15, .rTbl = HF3_HMTrtbl11nit, .cTbl = HF3_HMTctbl11nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 12, .refBr = 51, .cGma = gma2p15, .rTbl = HF3_HMTrtbl12nit, .cTbl = HF3_HMTctbl12nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 13, .refBr = 55, .cGma = gma2p15, .rTbl = HF3_HMTrtbl13nit, .cTbl = HF3_HMTctbl13nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 14, .refBr = 60, .cGma = gma2p15, .rTbl = HF3_HMTrtbl14nit, .cTbl = HF3_HMTctbl14nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 15, .refBr = 61, .cGma = gma2p15, .rTbl = HF3_HMTrtbl15nit, .cTbl = HF3_HMTctbl15nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 16, .refBr = 64, .cGma = gma2p15, .rTbl = HF3_HMTrtbl16nit, .cTbl = HF3_HMTctbl16nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 17, .refBr = 68, .cGma = gma2p15, .rTbl = HF3_HMTrtbl17nit, .cTbl = HF3_HMTctbl17nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 19, .refBr = 75, .cGma = gma2p15, .rTbl = HF3_HMTrtbl19nit, .cTbl = HF3_HMTctbl19nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 20, .refBr = 78, .cGma = gma2p15, .rTbl = HF3_HMTrtbl20nit, .cTbl = HF3_HMTctbl20nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 21, .refBr = 80, .cGma = gma2p15, .rTbl = HF3_HMTrtbl21nit, .cTbl = HF3_HMTctbl21nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 22, .refBr = 86, .cGma = gma2p15, .rTbl = HF3_HMTrtbl22nit, .cTbl = HF3_HMTctbl22nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 23, .refBr = 89, .cGma = gma2p15, .rTbl = HF3_HMTrtbl23nit, .cTbl = HF3_HMTctbl23nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 25, .refBr = 96, .cGma = gma2p15, .rTbl = HF3_HMTrtbl25nit, .cTbl = HF3_HMTctbl25nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 27, .refBr = 103, .cGma = gma2p15, .rTbl = HF3_HMTrtbl27nit, .cTbl = HF3_HMTctbl27nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 29, .refBr = 108, .cGma = gma2p15, .rTbl = HF3_HMTrtbl29nit, .cTbl = HF3_HMTctbl29nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 31, .refBr = 114, .cGma = gma2p15, .rTbl = HF3_HMTrtbl31nit, .cTbl = HF3_HMTctbl31nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 33, .refBr = 121, .cGma = gma2p15, .rTbl = HF3_HMTrtbl33nit, .cTbl = HF3_HMTctbl33nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 35, .refBr = 129, .cGma = gma2p15, .rTbl = HF3_HMTrtbl35nit, .cTbl = HF3_HMTctbl35nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 37, .refBr = 137, .cGma = gma2p15, .rTbl = HF3_HMTrtbl37nit, .cTbl = HF3_HMTctbl37nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 39, .refBr = 144, .cGma = gma2p15, .rTbl = HF3_HMTrtbl39nit, .cTbl = HF3_HMTctbl39nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 41, .refBr = 146, .cGma = gma2p15, .rTbl = HF3_HMTrtbl41nit, .cTbl = HF3_HMTctbl41nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 44, .refBr = 155, .cGma = gma2p15, .rTbl = HF3_HMTrtbl44nit, .cTbl = HF3_HMTctbl44nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 47, .refBr = 166, .cGma = gma2p15, .rTbl = HF3_HMTrtbl47nit, .cTbl = HF3_HMTctbl47nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 50, .refBr = 175, .cGma = gma2p15, .rTbl = HF3_HMTrtbl50nit, .cTbl = HF3_HMTctbl50nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 53, .refBr = 186, .cGma = gma2p15, .rTbl = HF3_HMTrtbl53nit, .cTbl = HF3_HMTctbl53nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 56, .refBr = 198, .cGma = gma2p15, .rTbl = HF3_HMTrtbl56nit, .cTbl = HF3_HMTctbl56nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 60, .refBr = 211, .cGma = gma2p15, .rTbl = HF3_HMTrtbl60nit, .cTbl = HF3_HMTctbl60nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 64, .refBr = 223, .cGma = gma2p15, .rTbl = HF3_HMTrtbl64nit, .cTbl = HF3_HMTctbl64nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 68, .refBr = 237, .cGma = gma2p15, .rTbl = HF3_HMTrtbl68nit, .cTbl = HF3_HMTctbl68nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 72, .refBr = 250, .cGma = gma2p15, .rTbl = HF3_HMTrtbl72nit, .cTbl = HF3_HMTctbl72nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 77, .refBr = 189, .cGma = gma2p15, .rTbl = HF3_HMTrtbl77nit, .cTbl = HF3_HMTctbl77nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 82, .refBr = 202, .cGma = gma2p15, .rTbl = HF3_HMTrtbl82nit, .cTbl = HF3_HMTctbl82nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 87, .refBr = 213, .cGma = gma2p15, .rTbl = HF3_HMTrtbl87nit, .cTbl = HF3_HMTctbl87nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 93, .refBr = 226, .cGma = gma2p15, .rTbl = HF3_HMTrtbl93nit, .cTbl = HF3_HMTctbl93nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 99, .refBr = 238, .cGma = gma2p15, .rTbl = HF3_HMTrtbl99nit, .cTbl = HF3_HMTctbl99nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 105, .refBr = 251, .cGma = gma2p15, .rTbl = HF3_HMTrtbl105nit, .cTbl = HF3_HMTctbl105nit, .aid = HF3_HMTaid7003, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
};


static int hmt_init_dimming(struct dsim_device *dsim, u8 * mtp)
{
        int     i, j;
        int     pos = 0;
        int     ret = 0;
        short   temp;
        struct dim_data *dimming;
        struct panel_private *panel = &dsim->priv;
        struct SmtDimInfo *diminfo = NULL;

        dimming = (struct dim_data *) kmalloc(sizeof(struct dim_data), GFP_KERNEL);
        if (!dimming) {
                dsim_err("failed to allocate memory for dim data\n");
                ret = -ENOMEM;
                goto error;
        }

        switch (dynamic_lcd_type) {
                case LCD_TYPE_S6E3HA2_WQHD:
                        dsim_info("%s init HMT dimming info for HA2 panel\n", __func__);
                        diminfo = (void *) hmt_dimming_info_HA2;
                        break;
                case LCD_TYPE_S6E3HA3_WQHD:
                        dsim_info("%s init HMT dimming info for HA3 panel\n", __func__);
                        diminfo = (void *) hmt_dimming_info_HA3;
                        break;
                case LCD_TYPE_S6E3HF3_WQHD:
                        dsim_info("%s init HMT dimming info for HF3 panel\n", __func__);
                        diminfo = (void *) hmt_dimming_info_HF3;
                        break;
                default:
                        dsim_info("%s init HMT dimming info for (UNKNOWN) HA2 panel\n", __func__);
                        diminfo = (void *) hmt_dimming_info_HA2;
                        break;
        }

        panel->hmt_dim_data = (void *) dimming;
        panel->hmt_dim_info = (void *) diminfo;

        panel->hmt_br_tbl = (unsigned int *) hmt_br_tbl;

        for (j = 0; j < CI_MAX; j++) {
                temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
                dimming->t_gamma[V255][j] = (int) center_gamma[V255][j] + temp;
                dimming->mtp[V255][j] = temp;
                pos += 2;
        }

        for (i = V203; i >= V0; i--) {
                for (j = 0; j < CI_MAX; j++) {
                        temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
                        dimming->t_gamma[i][j] = (int) center_gamma[i][j] + temp;
                        dimming->mtp[i][j] = temp;
                        pos++;
                }
        }
        /* for vt */
        temp = (mtp[pos + 1]) << 8 | mtp[pos];

        for (i = 0; i < CI_MAX; i++)
                dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;

        ret = generate_volt_table(dimming);
        if (ret) {
                dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
                goto error;
        }

        for (i = 0; i < HMT_MAX_BR_INFO; i++) {
                ret = cal_gamma_from_index(dimming, &diminfo[i]);
                if (ret) {
                        dsim_err("failed to calculate gamma : index : %d\n", i);
                        goto error;
                }
        }
      error:
        return ret;

}

#endif

#endif


/************************************ HA2 *****************************************/


static int s6e3ha2_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
        int     i = 0;
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char buf[S6E3HA2_MTP_DATE_SIZE] = { 0, };
        unsigned char bufForCoordi[S6E3HA2_COORDINATE_LEN] = { 0, };
        unsigned char hbm_gamma[S6E3HA2_HBMGAMMA_LEN + 1] = { 0, };
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, S6E3HA2_ID_REG, S6E3HA2_ID_LEN, dsim->priv.id);
        if (ret != S6E3HA2_ID_LEN) {
                dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
                panel->lcdConnected = PANEL_DISCONNEDTED;
                goto read_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA2_ID_LEN; i++)
                dsim_info("%02x, ", dsim->priv.id[i]);
        dsim_info("\n");

        ret = dsim_read_hl_data(dsim, S6E3HA2_MTP_ADDR, S6E3HA2_MTP_DATE_SIZE, buf);
        if (ret != S6E3HA2_MTP_DATE_SIZE) {
                dsim_err("failed to read mtp, check panel connection\n");
                goto read_fail;
        }
        memcpy(mtp, buf, S6E3HA2_MTP_SIZE);
        memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
        dsim_info("READ MTP SIZE : %d\n", S6E3HA2_MTP_SIZE);
        dsim_info("=========== MTP INFO =========== \n");
        for (i = 0; i < S6E3HA2_MTP_SIZE; i++)
                dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

        // coordinate
        ret = dsim_read_hl_data(dsim, S6E3HA2_COORDINATE_REG, S6E3HA2_COORDINATE_LEN, bufForCoordi);
        if (ret != S6E3HA2_COORDINATE_LEN) {
                dsim_err("fail to read coordinate on command.\n");
                goto read_fail;
        }
        dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
        dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
        dsim_info("READ coordi : ");
        for (i = 0; i < 2; i++)
                dsim_info("%d, ", dsim->priv.coordinate[i]);
        dsim_info("\n");

        // code
        ret = dsim_read_hl_data(dsim, S6E3HA2_CODE_REG, S6E3HA2_CODE_LEN, dsim->priv.code);
        if (ret != S6E3HA2_CODE_LEN) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ code : ");
        for (i = 0; i < S6E3HA2_CODE_LEN; i++)
                dsim_info("%x, ", dsim->priv.code[i]);
        dsim_info("\n");

        // tset
        ret = dsim_read_hl_data(dsim, TSET_REG, TSET_LEN - 1, dsim->priv.tset);
        if (ret < TSET_LEN - 1) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ tset : ");
        for (i = 0; i < TSET_LEN - 1; i++)
                dsim_info("%x, ", dsim->priv.tset[i]);
        dsim_info("\n");


        // elvss
        ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN - 1, dsim->priv.elvss_set);
        if (ret < ELVSS_LEN - 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("READ elvss : ");
        for (i = 0; i < ELVSS_LEN - 1; i++)
                dsim_info("%x \n", dsim->priv.elvss_set[i]);

/* read hbm elvss for hbm interpolation */
        panel->hbm_elvss = dsim->priv.elvss_set[S6E3HA2_HBM_ELVSS_INDEX];

        ret = dsim_read_hl_data(dsim, S6E3HA2_HBMGAMMA_REG, S6E3HA2_HBMGAMMA_LEN + 1, hbm_gamma);
        if (ret != S6E3HA2_HBMGAMMA_LEN + 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("HBM Gamma : ");
        memcpy(hbm, hbm_gamma + 1, S6E3HA2_HBMGAMMA_LEN);

        for (i = 1; i < S6E3HA2_HBMGAMMA_LEN + 1; i++)
                dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto read_exit;
        }
        ret = 0;

      read_exit:
        return 0;

      read_fail:
        return -ENODEV;
}
static int s6e3ha2_wqhd_dump(struct dsim_device *dsim)
{
        int     ret = 0;
        int     i;
        unsigned char id[S6E3HA2_ID_LEN];
        unsigned char rddpm[4];
        unsigned char rddsm[4];
        unsigned char err_buf[4];

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
        if (ret != 3) {
                dsim_err("%s : can't read Panel's EA Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's 0xEA Reg Value ===\n");
        dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
        dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

        ret = dsim_read_hl_data(dsim, S6E3HA2_RDDPM_ADDR, 3, rddpm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDPM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

        if (rddpm[0] & 0x80)
                dsim_info("* Booster Voltage Status : ON\n");
        else
                dsim_info("* Booster Voltage Status : OFF\n");

        if (rddpm[0] & 0x40)
                dsim_info("* Idle Mode : On\n");
        else
                dsim_info("* Idle Mode : OFF\n");

        if (rddpm[0] & 0x20)
                dsim_info("* Partial Mode : On\n");
        else
                dsim_info("* Partial Mode : OFF\n");

        if (rddpm[0] & 0x10)
                dsim_info("* Sleep OUT and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x08)
                dsim_info("* Normal Mode On and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x04)
                dsim_info("* Display On and Working Ok\n");
        else
                dsim_info("* Display Off\n");

        ret = dsim_read_hl_data(dsim, S6E3HA2_RDDSM_ADDR, 3, rddsm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDSM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

        if (rddsm[0] & 0x80)
                dsim_info("* TE On\n");
        else
                dsim_info("* TE OFF\n");

        if (rddsm[0] & 0x02)
                dsim_info("* S_DSI_ERR : Found\n");

        if (rddsm[0] & 0x01)
                dsim_info("* DSI_ERR : Found\n");

        // id
        ret = dsim_read_hl_data(dsim, S6E3HA2_ID_REG, S6E3HA2_ID_LEN, id);
        if (ret != S6E3HA2_ID_LEN) {
                dsim_err("%s : can't read panel id\n", __func__);
                goto dump_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA2_ID_LEN; i++)
                dsim_info("%02x, ", id[i]);
        dsim_info("\n");

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
        }
      dump_exit:
        dsim_info(" - %s\n", __func__);
        return ret;

}
static int s6e3ha2_wqhd_probe(struct dsim_device *dsim)
{
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char mtp[S6E3HA2_MTP_SIZE] = { 0, };
        unsigned char hbm[S6E3HA2_HBMGAMMA_LEN] = { 0, };
        panel->dim_data = (void *) NULL;
        panel->lcdConnected = PANEL_CONNECTED;

        dsim_info(" +  : %s\n", __func__);

        ret = s6e3ha2_read_init_info(dsim, mtp, hbm);
        if (panel->lcdConnected == PANEL_DISCONNEDTED) {
                dsim_err("dsim : %s lcd was not connected\n", __func__);
                goto probe_exit;
        }
		esd_support = 1;

#ifdef CONFIG_PANEL_AID_DIMMING
        ret = init_dimming(dsim, mtp, hbm);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_LCD_HMT
        ret = hmt_init_dimming(dsim, mtp);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
        panel->mdnie_support = 1;
#endif

probe_exit:
        return ret;

}


static int s6e3ha2_wqhd_displayon(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
                goto displayon_err;
        }

      displayon_err:
        return ret;

}

static int s6e3ha2_wqhd_exit(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
                goto exit_err;
        }

        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
                goto exit_err;
        }

        msleep(120);

      exit_err:
        return ret;
}

static int s6e3ha2_wqhd_init(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
                goto init_exit;
        }

        /* 7. Sleep Out(11h) */
        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
                goto init_exit;
        }
        msleep(5);

        /* 9. Interface Setting */

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_SINGLE_DSI_1, ARRAY_SIZE(S6E3HA2_SEQ_SINGLE_DSI_1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_SINGLE_DSI_2, ARRAY_SIZE(S6E3HA2_SEQ_SINGLE_DSI_2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_2\n", __func__);
                goto init_exit;
        }
#ifdef CONFIG_LCD_HMT
        if (dsim->priv.hmt_on != HMT_ON)
#endif
                msleep(120);

        /* Common Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TE_ON, ARRAY_SIZE(S6E3HA2_SEQ_TE_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSP_TE, ARRAY_SIZE(S6E3HA2_SEQ_TSP_TE));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_PENTILE_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
                goto init_exit;
        }

        /* POC setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_POC_SETTING1, ARRAY_SIZE(S6E3HA2_SEQ_POC_SETTING1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_POC_SETTING1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_POC_SETTING2, ARRAY_SIZE(S6E3HA2_SEQ_POC_SETTING2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_POC_SETTING2\n", __func__);
                goto init_exit;
        }

        /* OSC setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING1, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING2, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC2\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING3, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING3));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC3\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
                goto init_exit;
        }

        /* PCD setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_PCD_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_PCD_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_PCD_SETTING\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_ERR_FG_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
                goto init_exit;
        }

#ifndef CONFIG_PANEL_AID_DIMMING
        /* Brightness Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HA2_SEQ_GAMMA_CONDITION_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HA2_SEQ_AOR_CONTROL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ELVSS_SET, ARRAY_SIZE(S6E3HA2_SEQ_ELVSS_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_VINT_SET, ARRAY_SIZE(S6E3HA2_SEQ_VINT_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
                goto init_exit;
        }

        /* ACL Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HA2_SEQ_ACL_OFF));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HA2_SEQ_ACL_OFF_OPR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
                goto init_exit;
        }
        /* elvss */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HA2_SEQ_TSET_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSET, ARRAY_SIZE(S6E3HA2_SEQ_TSET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
                goto init_exit;
        }
#endif
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
                goto init_exit;
        }

      init_exit:
        return ret;
}



struct dsim_panel_ops s6e3ha2_panel_ops = {
        .probe = s6e3ha2_wqhd_probe,
        .displayon = s6e3ha2_wqhd_displayon,
        .exit = s6e3ha2_wqhd_exit,
        .init = s6e3ha2_wqhd_init,
        .dump = s6e3ha2_wqhd_dump,
};

/************************************ HA3 *****************************************/

// convert NEW SPEC to OLD SPEC
static int s6e3ha3_VT_RGB2GRB( unsigned char *VT )
{
	int ret = 0;
	int r, g, b;

	// new SPEC = RG0B
	r = (VT[0] & 0xF0) >>4;
	g = (VT[0] & 0x0F);
	b = (VT[1] & 0x0F);

	// old spec = GR0B
	VT[0] = g<<4 | r;
	VT[1] = b;

	return ret;
}

static int s6e3ha3_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
        int     i = 0;
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char buf[S6E3HA3_MTP_DATE_SIZE] = { 0, };
        unsigned char bufForCoordi[S6E3HA3_COORDINATE_LEN] = { 0, };
        unsigned char hbm_gamma[S6E3HA3_HBMGAMMA_LEN + 1] = { 0, };
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, S6E3HA3_ID_REG, S6E3HA3_ID_LEN, dsim->priv.id);
        if (ret != S6E3HA3_ID_LEN) {
                dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
                panel->lcdConnected = PANEL_DISCONNEDTED;
                goto read_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA3_ID_LEN; i++)
                dsim_info("%02x, ", dsim->priv.id[i]);
        dsim_info("\n");

        ret = dsim_read_hl_data(dsim, S6E3HA3_MTP_ADDR, S6E3HA3_MTP_DATE_SIZE, buf);
        if (ret != S6E3HA3_MTP_DATE_SIZE) {
                dsim_err("failed to read mtp, check panel connection\n");
                goto read_fail;
        }
		s6e3ha3_VT_RGB2GRB( buf + S6E3HA3_MTP_VT_ADDR );
        memcpy(mtp, buf, S6E3HA3_MTP_SIZE);
        memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
        dsim_info("READ MTP SIZE : %d\n", S6E3HA3_MTP_SIZE);
        dsim_info("=========== MTP INFO =========== \n");
        for (i = 0; i < S6E3HA3_MTP_SIZE; i++)
                dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

        // coordinate
        ret = dsim_read_hl_data(dsim, S6E3HA3_COORDINATE_REG, S6E3HA3_COORDINATE_LEN, bufForCoordi);
        if (ret != S6E3HA3_COORDINATE_LEN) {
                dsim_err("fail to read coordinate on command.\n");
                goto read_fail;
        }
        dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
        dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
        dsim_info("READ coordi : ");
        for (i = 0; i < 2; i++)
                dsim_info("%d, ", dsim->priv.coordinate[i]);
        dsim_info("\n");

        // code
        ret = dsim_read_hl_data(dsim, S6E3HA3_CODE_REG, S6E3HA3_CODE_LEN, dsim->priv.code);
        if (ret != S6E3HA3_CODE_LEN) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ code : ");
        for (i = 0; i < S6E3HA3_CODE_LEN; i++)
                dsim_info("%x, ", dsim->priv.code[i]);
        dsim_info("\n");

        // aid

		memcpy(dsim->priv.aid, &S6E3HA3_SEQ_AOR[1], ARRAY_SIZE(S6E3HA3_SEQ_AOR) - 1);
        dsim_info("READ aid : ");
        for (i = 0; i < S6E3HA3_AID_CMD_CNT - 1; i++)
                dsim_info("%x, ", dsim->priv.aid[i]);
        dsim_info("\n");

        // tset
        ret = dsim_read_hl_data(dsim, S6E3HA3_TSET_REG, S6E3HA3_TSET_LEN - 1, dsim->priv.tset);
        if (ret < S6E3HA3_TSET_LEN - 1) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ tset : ");
        for (i = 0; i < S6E3HA3_TSET_LEN - 1; i++)
                dsim_info("%x, ", dsim->priv.tset[i]);
        dsim_info("\n");

        // elvss
        ret = dsim_read_hl_data(dsim, S6E3HA3_ELVSS_REG, S6E3HA3_ELVSS_LEN - 1, dsim->priv.elvss_set);
        if (ret < S6E3HA3_ELVSS_LEN - 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("READ elvss : ");
        for (i = 0; i < S6E3HA3_ELVSS_LEN - 1; i++)
                dsim_info("%x \n", dsim->priv.elvss_set[i]);

/* read hbm elvss for hbm interpolation */
        panel->hbm_elvss = dsim->priv.elvss_set[S6E3HA3_HBM_ELVSS_INDEX];

        ret = dsim_read_hl_data(dsim, S6E3HA3_HBMGAMMA_REG, S6E3HA3_HBMGAMMA_LEN + 1, hbm_gamma);
        if (ret != S6E3HA3_HBMGAMMA_LEN + 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("HBM Gamma : ");
        memcpy(hbm, hbm_gamma + 1, S6E3HA3_HBMGAMMA_LEN);

        for (i = 1; i < S6E3HA3_HBMGAMMA_LEN + 1; i++)
                dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto read_exit;
        }
        ret = 0;

read_exit:
        return 0;

read_fail:
        return -ENODEV;
}
static int s6e3ha3_wqhd_dump(struct dsim_device *dsim)
{
        int     ret = 0;
        int     i;
        unsigned char id[S6E3HA3_ID_LEN];
        unsigned char rddpm[4];
        unsigned char rddsm[4];
        unsigned char err_buf[4];

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
        if (ret != 3) {
                dsim_err("%s : can't read Panel's EA Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's 0xEA Reg Value ===\n");
        dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
        dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

        ret = dsim_read_hl_data(dsim, S6E3HA3_RDDPM_ADDR, 3, rddpm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDPM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

        if (rddpm[0] & 0x80)
                dsim_info("* Booster Voltage Status : ON\n");
        else
                dsim_info("* Booster Voltage Status : OFF\n");

        if (rddpm[0] & 0x40)
                dsim_info("* Idle Mode : On\n");
        else
                dsim_info("* Idle Mode : OFF\n");

        if (rddpm[0] & 0x20)
                dsim_info("* Partial Mode : On\n");
        else
                dsim_info("* Partial Mode : OFF\n");

        if (rddpm[0] & 0x10)
                dsim_info("* Sleep OUT and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x08)
                dsim_info("* Normal Mode On and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x04)
                dsim_info("* Display On and Working Ok\n");
        else
                dsim_info("* Display Off\n");

        ret = dsim_read_hl_data(dsim, S6E3HA3_RDDSM_ADDR, 3, rddsm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDSM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

        if (rddsm[0] & 0x80)
                dsim_info("* TE On\n");
        else
                dsim_info("* TE OFF\n");

        if (rddsm[0] & 0x02)
                dsim_info("* S_DSI_ERR : Found\n");

        if (rddsm[0] & 0x01)
                dsim_info("* DSI_ERR : Found\n");

        // id
        ret = dsim_read_hl_data(dsim, S6E3HA3_ID_REG, S6E3HA3_ID_LEN, id);
        if (ret != S6E3HA3_ID_LEN) {
                dsim_err("%s : can't read panel id\n", __func__);
                goto dump_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA3_ID_LEN; i++)
                dsim_info("%02x, ", id[i]);
        dsim_info("\n");

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
        }
      dump_exit:
        dsim_info(" - %s\n", __func__);
        return ret;

}
static int s6e3ha3_wqhd_probe(struct dsim_device *dsim)
{
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char mtp[S6E3HA3_MTP_SIZE] = { 0, };
        unsigned char hbm[S6E3HA3_HBMGAMMA_LEN] = { 0, };
        panel->dim_data = (void *) NULL;
        panel->lcdConnected = PANEL_CONNECTED;

        dsim_info(" +  : %s\n", __func__);

        ret = s6e3ha3_read_init_info(dsim, mtp, hbm);
        if (panel->lcdConnected == PANEL_DISCONNEDTED) {
                dsim_err("dsim : %s lcd was not connected\n", __func__);
                goto probe_exit;
        }

		esd_support = 1;
		if((panel->id[2] & 0x0F) < 2)	// under rev.B
			esd_support = 0;

#ifdef CONFIG_PANEL_AID_DIMMING
        ret = init_dimming(dsim, mtp, hbm);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_LCD_HMT
        ret = hmt_init_dimming(dsim, mtp);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
        panel->mdnie_support = 1;
#endif

      probe_exit:
        return ret;

}


static int s6e3ha3_wqhd_displayon(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
                goto displayon_err;
        }

      displayon_err:
        return ret;

}

static int s6e3ha3_wqhd_exit(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
                goto exit_err;
        }

        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
                goto exit_err;
        }

        msleep(120);

      exit_err:
        return ret;
}

static int s6e3ha3_wqhd_init(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
                goto init_exit;
        }
        msleep(5);

        /* 7. Sleep Out(11h) */
        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
                goto init_exit;
        }
        msleep(5);

        /* 9. Interface Setting */

		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_FREQ_CALIBRATION, ARRAY_SIZE(S6E3HA3_SEQ_FREQ_CALIBRATION));
		if (ret < 0) {
				dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_MIC\n", __func__);
				goto init_exit;
		}

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_MIC, ARRAY_SIZE(S6E3HA3_SEQ_MIC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_MIC\n", __func__);
                goto init_exit;
        }
		if(((dsim->priv.id[1] & 0x04) >> 2) == LCD_DCDC_MAX77838) {
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DCDC_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_DCDC_GLOBAL));
	        if (ret < 0) {
				dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DCDC_GLOBAL\n", __func__);
	            goto init_exit;
			}
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DCDC, ARRAY_SIZE(S6E3HA3_SEQ_DCDC));
	        if (ret < 0) {
				dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DCDC\n", __func__);
                goto init_exit;
			}
		}
        msleep(120);

        /* Common Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TE_RISING_TIMING, ARRAY_SIZE(S6E3HA3_SEQ_TE_RISING_TIMING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TE_ON, ARRAY_SIZE(S6E3HA3_SEQ_TE_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HA3_SEQ_TSP_TE));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_PCD, ARRAY_SIZE(S6E3HA3_SEQ_PCD));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ERR_FG, ARRAY_SIZE(S6E3HA3_SEQ_ERR_FG));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HA3_SEQ_PENTILE_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

#ifndef CONFIG_PANEL_AID_DIMMING
        /* Brightness Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HA3_SEQ_GAMMA_CONDITION_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_AOR, ARRAY_SIZE(S6E3HA3_SEQ_AOR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_AOR_0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_MPS_ELVSS_SET, ARRAY_SIZE(S6E3HA3_SEQ_MPS_ELVSS_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_MPS_ELVSS_SET\n", __func__);
                goto init_exit;
        }

        /* ACL Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HA3_SEQ_ACL_OFF));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HA3_SEQ_ACL_OFF_OPR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
                goto init_exit;
        }

        /* elvss */
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ELVSS_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_ELVSS_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_ELVSS_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ELVSS, ARRAY_SIZE(S6E3HA3_SEQ_ELVSS));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_ELVSS\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_TSET_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSET, ARRAY_SIZE(S6E3HA3_SEQ_TSET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HA3_SEQ_VINT_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_VINT_SET\n", __func__);
                goto init_exit;
        }
#endif
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
                goto init_exit;
        }

      init_exit:
        return ret;
}



struct dsim_panel_ops s6e3ha3_panel_ops = {
        .probe = s6e3ha3_wqhd_probe,
        .displayon = s6e3ha3_wqhd_displayon,
        .exit = s6e3ha3_wqhd_exit,
        .init = s6e3ha3_wqhd_init,
        .dump = s6e3ha3_wqhd_dump,
};

/******************** HF3 ********************/


static int s6e3hf3_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
        int     i = 0;
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char buf[S6E3HF3_MTP_DATE_SIZE] = { 0, };
        unsigned char bufForCoordi[S6E3HF3_COORDINATE_LEN] = { 0, };
        unsigned char hbm_gamma[S6E3HF3_HBMGAMMA_LEN + 1] = { 0, };
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, S6E3HF3_ID_REG, S6E3HF3_ID_LEN, dsim->priv.id);
        if (ret != S6E3HF3_ID_LEN) {
                dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
                panel->lcdConnected = PANEL_DISCONNEDTED;
                goto read_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HF3_ID_LEN; i++)
                dsim_info("%02x, ", dsim->priv.id[i]);
        dsim_info("\n");

        ret = dsim_read_hl_data(dsim, S6E3HF3_MTP_ADDR, S6E3HF3_MTP_DATE_SIZE, buf);
        if (ret != S6E3HF3_MTP_DATE_SIZE) {
                dsim_err("failed to read mtp, check panel connection\n");
                goto read_fail;
        }
		s6e3ha3_VT_RGB2GRB( buf +S6E3HF3_MTP_VT_ADDR );
        memcpy(mtp, buf, S6E3HF3_MTP_SIZE);
        memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
        dsim_info("READ MTP SIZE : %d\n", S6E3HF3_MTP_SIZE);
        dsim_info("=========== MTP INFO =========== \n");
        for (i = 0; i < S6E3HF3_MTP_SIZE; i++)
                dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

        // coordinate
        ret = dsim_read_hl_data(dsim, S6E3HF3_COORDINATE_REG, S6E3HF3_COORDINATE_LEN, bufForCoordi);
        if (ret != S6E3HF3_COORDINATE_LEN) {
                dsim_err("fail to read coordinate on command.\n");
                goto read_fail;
        }
        dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
        dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
        dsim_info("READ coordi : ");
        for (i = 0; i < 2; i++)
                dsim_info("%d, ", dsim->priv.coordinate[i]);
        dsim_info("\n");

        // code
        ret = dsim_read_hl_data(dsim, S6E3HF3_CODE_REG, S6E3HF3_CODE_LEN, dsim->priv.code);
        if (ret != S6E3HF3_CODE_LEN) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ code : ");
        for (i = 0; i < S6E3HF3_CODE_LEN; i++)
                dsim_info("%x, ", dsim->priv.code[i]);
        dsim_info("\n");

        // aid
   		memcpy(dsim->priv.aid, &S6E3HF3_SEQ_AOR[1], ARRAY_SIZE(S6E3HF3_SEQ_AOR) - 1);
        dsim_info("READ aid : ");
        for (i = 0; i < S6E3HF3_AID_CMD_CNT - 1; i++)
                dsim_info("%x, ", dsim->priv.aid[i]);
        dsim_info("\n");

        // tset
        ret = dsim_read_hl_data(dsim, S6E3HF3_TSET_REG, S6E3HF3_TSET_LEN - 1, dsim->priv.tset);
        if (ret < S6E3HF3_TSET_LEN - 1) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ tset : ");
        for (i = 0; i < S6E3HF3_TSET_LEN - 1; i++)
                dsim_info("%x, ", dsim->priv.tset[i]);
        dsim_info("\n");

        // elvss
        ret = dsim_read_hl_data(dsim, S6E3HF3_ELVSS_REG, S6E3HF3_ELVSS_LEN - 1, dsim->priv.elvss_set);
        if (ret < S6E3HF3_ELVSS_LEN - 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("READ elvss : ");
        for (i = 0; i < S6E3HF3_ELVSS_LEN - 1; i++)
                dsim_info("%x \n", dsim->priv.elvss_set[i]);

/* read hbm elvss for hbm interpolation */
        panel->hbm_elvss = dsim->priv.elvss_set[S6E3HF3_HBM_ELVSS_INDEX];

        ret = dsim_read_hl_data(dsim, S6E3HF3_HBMGAMMA_REG, S6E3HF3_HBMGAMMA_LEN + 1, hbm_gamma);
        if (ret != S6E3HF3_HBMGAMMA_LEN + 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("HBM Gamma : ");
        memcpy(hbm, hbm_gamma + 1, S6E3HF3_HBMGAMMA_LEN);

        for (i = 1; i < S6E3HF3_HBMGAMMA_LEN + 1; i++)
                dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto read_exit;
        }
        ret = 0;

      read_exit:
        return 0;

      read_fail:
        return -ENODEV;
}

static int s6e3hf3_wqhd_dump(struct dsim_device *dsim)
{
        int     ret = 0;
        int     i;
        unsigned char id[S6E3HF3_ID_LEN];
        unsigned char rddpm[4];
        unsigned char rddsm[4];
        unsigned char err_buf[4];

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
        if (ret != 3) {
                dsim_err("%s : can't read Panel's EA Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's 0xEA Reg Value ===\n");
        dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
        dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

        ret = dsim_read_hl_data(dsim, S6E3HF3_RDDPM_ADDR, 3, rddpm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDPM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

        if (rddpm[0] & 0x80)
                dsim_info("* Booster Voltage Status : ON\n");
        else
                dsim_info("* Booster Voltage Status : OFF\n");

        if (rddpm[0] & 0x40)
                dsim_info("* Idle Mode : On\n");
        else
                dsim_info("* Idle Mode : OFF\n");

        if (rddpm[0] & 0x20)
                dsim_info("* Partial Mode : On\n");
        else
                dsim_info("* Partial Mode : OFF\n");

        if (rddpm[0] & 0x10)
                dsim_info("* Sleep OUT and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x08)
                dsim_info("* Normal Mode On and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x04)
                dsim_info("* Display On and Working Ok\n");
        else
                dsim_info("* Display Off\n");

        ret = dsim_read_hl_data(dsim, S6E3HF3_RDDSM_ADDR, 3, rddsm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDSM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

        if (rddsm[0] & 0x80)
                dsim_info("* TE On\n");
        else
                dsim_info("* TE OFF\n");

        if (rddsm[0] & 0x02)
                dsim_info("* S_DSI_ERR : Found\n");

        if (rddsm[0] & 0x01)
                dsim_info("* DSI_ERR : Found\n");

        // id
        ret = dsim_read_hl_data(dsim, S6E3HF3_ID_REG, S6E3HF3_ID_LEN, id);
        if (ret != S6E3HF3_ID_LEN) {
                dsim_err("%s : can't read panel id\n", __func__);
                goto dump_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HF3_ID_LEN; i++)
                dsim_info("%02x, ", id[i]);
        dsim_info("\n");

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
        }
      dump_exit:
        dsim_info(" - %s\n", __func__);
        return ret;

}

static int s6e3hf3_wqhd_probe(struct dsim_device *dsim)
{
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char mtp[S6E3HF3_MTP_SIZE] = { 0, };
        unsigned char hbm[S6E3HF3_HBMGAMMA_LEN] = { 0, };
        panel->dim_data = (void *) NULL;
        panel->lcdConnected = PANEL_CONNECTED;
		dsim->glide_display_size = 80;	// framebuffer of LCD is 1600x2560, display area is 1440x2560, glidesize = (1600-1440)/2

        dsim_info(" +  : %s\n", __func__);

        ret = s6e3hf3_read_init_info(dsim, mtp, hbm);
        if (panel->lcdConnected == PANEL_DISCONNEDTED) {
                dsim_err("dsim : %s lcd was not connected\n", __func__);
                goto probe_exit;
        }

		esd_support = 0;		// TBD

#ifdef CONFIG_PANEL_AID_DIMMING
        ret = init_dimming(dsim, mtp, hbm);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_LCD_HMT
        ret = hmt_init_dimming(dsim, mtp);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
// kyNam_150427_        panel->mdnie_support = 0;
// kyNam_150427_        if (panel->panel_type != LCD_TYPE_S6E3HF3_WQHD)
                panel->mdnie_support = 1;
#endif

      probe_exit:
        return ret;

}


static int s6e3hf3_wqhd_displayon(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
                goto displayon_err;
        }

      displayon_err:
        return ret;

}

static int s6e3hf3_wqhd_exit(struct dsim_device *dsim)
{
        int     ret = 0;


        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
                goto exit_err;
        }

        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
                goto exit_err;
        }

        msleep(120);

      exit_err:
        return ret;
}


static int s6e3hf3_wqhd_init(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
                goto init_exit;
        }

        /* 7. Sleep Out(11h) */
        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
                goto init_exit;
        }
        msleep(5);

        /* 9. Interface Setting */

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MIC, ARRAY_SIZE(S6E3HF3_SEQ_MIC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_CASET_SET, ARRAY_SIZE(S6E3HF3_SEQ_CASET_SET));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC\n", __func__);
                goto init_exit;
        }

#ifdef CONFIG_LCD_HMT
        if (dsim->priv.hmt_on != HMT_ON)
#endif
        msleep(120);

        /* Common Setting */
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_RISING_TIMING, ARRAY_SIZE(S6E3HF3_SEQ_TE_RISING_TIMING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_ON, ARRAY_SIZE(S6E3HF3_SEQ_TE_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF3_SEQ_TSP_TE));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_POC_SET, ARRAY_SIZE(S6E3HF3_SEQ_POC_SET));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_PCD, ARRAY_SIZE(S6E3HF3_SEQ_PCD));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ERR_FG, ARRAY_SIZE(S6E3HF3_SEQ_ERR_FG));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_PENTILE_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
                goto init_exit;
        }

#ifndef CONFIG_PANEL_AID_DIMMING
        /* Brightness Setting */
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_CONDITION_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_AOR, ARRAY_SIZE(S6E3HF3_SEQ_AOR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_AOR_0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MPS_ELVSS_SET, ARRAY_SIZE(S6E3HF3_SEQ_MPS_ELVSS_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_MPS_ELVSS_SET\n", __func__);
                goto init_exit;
        }

        /* ACL Setting */
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF_OPR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
                goto init_exit;
        }

        /* elvss */
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ELVSS_GLOBAL, ARRAY_SIZE(S6E3HF3_SEQ_ELVSS_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_ELVSS_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ELVSS, ARRAY_SIZE(S6E3HF3_SEQ_ELVSS));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_ELVSS\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HF3_SEQ_TSET_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSET, ARRAY_SIZE(S6E3HF3_SEQ_TSET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF3_SEQ_VINT_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_VINT_SET\n", __func__);
                goto init_exit;
        }
#endif
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
                goto init_exit;
        }

      init_exit:
        return ret;
}




struct dsim_panel_ops s6e3hf3_panel_ops = {
        .probe = s6e3hf3_wqhd_probe,
        .displayon = s6e3hf3_wqhd_displayon,
        .exit = s6e3hf3_wqhd_exit,
        .init = s6e3hf3_wqhd_init,
        .dump = s6e3hf3_wqhd_dump,
};



/******************** COMMON ********************/

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
        switch (dynamic_lcd_type) {
                case LCD_TYPE_S6E3HA2_WQHD:
                        return &s6e3ha2_panel_ops;
                case LCD_TYPE_S6E3HA3_WQHD:
                        return &s6e3ha3_panel_ops;
                case LCD_TYPE_S6E3HF3_WQHD:
                        return &s6e3hf3_panel_ops;
                default:
                        return &s6e3ha2_panel_ops;
        }
}


static int __init get_lcd_type(char *arg)
{
        unsigned int lcdtype;

        get_option(&arg, &lcdtype);

        dsim_info("--- Parse LCD TYPE ---\n");
        dsim_info("LCDTYPE : %x\n", lcdtype);

#if 1	// kyNam_150430_ HF3-bare LCD
		if( lcdtype == 0x000010 ) lcdtype = LCD_TYPE_ID_HF3;
#endif
		esd_support = 0;


	    switch (lcdtype & LCD_TYPE_MASK) {
		case LCD_TYPE_ID_HA2:
			dynamic_lcd_type = LCD_TYPE_S6E3HA2_WQHD;
			dsim_info("LCD TYPE : S6E3HA2 (WQHD) : %d\n", dynamic_lcd_type);
			aid_dimming_dynamic.elvss_len = S6E3HA2_ELVSS_LEN;
			aid_dimming_dynamic.elvss_reg = S6E3HA2_ELVSS_REG;
			aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA2_ELVSS_CMD_CNT;
			aid_dimming_dynamic.aid_cmd_cnt = S6E3HA2_AID_CMD_CNT;
			aid_dimming_dynamic.aid_reg_offset = S6E3HA2_AID_REG_OFFSET;
			aid_dimming_dynamic.tset_len = S6E3HA2_TSET_LEN;
			aid_dimming_dynamic.tset_reg = S6E3HA2_TSET_REG;
			aid_dimming_dynamic.tset_minus_offset = S6E3HA2_TSET_MINUS_OFFSET;
			aid_dimming_dynamic.vint_reg2 = S6E3HA2_VINT_REG2;
	 		break;
		case LCD_TYPE_ID_HA3:
			dynamic_lcd_type = LCD_TYPE_S6E3HA3_WQHD;
			dsim_info("LCD TYPE : S6E3HA3 (WQHD) : %d\n", dynamic_lcd_type);
			aid_dimming_dynamic.elvss_len = S6E3HA3_ELVSS_LEN;
			aid_dimming_dynamic.elvss_reg = S6E3HA3_ELVSS_REG;
			aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA3_ELVSS_CMD_CNT;
			aid_dimming_dynamic.aid_cmd_cnt = S6E3HA3_AID_CMD_CNT;
			aid_dimming_dynamic.aid_reg_offset = S6E3HA3_AID_REG_OFFSET;
			aid_dimming_dynamic.tset_len = S6E3HA3_TSET_LEN;
			aid_dimming_dynamic.tset_reg = S6E3HA3_TSET_REG;
			aid_dimming_dynamic.tset_minus_offset = S6E3HA3_TSET_MINUS_OFFSET;
			aid_dimming_dynamic.vint_reg2 = S6E3HA3_VINT_REG2;
			break;
		case LCD_TYPE_ID_HF3:
			dynamic_lcd_type = LCD_TYPE_S6E3HF3_WQHD;
			dsim_info("LCD TYPE : S6E3HF3 (WQHD) : %d\n", dynamic_lcd_type);
			aid_dimming_dynamic.elvss_len = S6E3HF3_ELVSS_LEN;
			aid_dimming_dynamic.elvss_reg = S6E3HF3_ELVSS_REG;
			aid_dimming_dynamic.elvss_cmd_cnt = S6E3HF3_ELVSS_CMD_CNT;
			aid_dimming_dynamic.aid_cmd_cnt = S6E3HF3_AID_CMD_CNT;
			aid_dimming_dynamic.aid_reg_offset = S6E3HF3_AID_REG_OFFSET;
			aid_dimming_dynamic.tset_len = S6E3HF3_TSET_LEN;
			aid_dimming_dynamic.tset_reg = S6E3HF3_TSET_REG;
			aid_dimming_dynamic.tset_minus_offset = S6E3HF3_TSET_MINUS_OFFSET;
			aid_dimming_dynamic.vint_reg2 = S6E3HF3_VINT_REG2;
			break;
		default:
			dynamic_lcd_type = LCD_TYPE_S6E3HA2_WQHD;
			dsim_info("LCD TYPE : [UNKNOWN] -> S6E3HA2 (WQHD) : %d\n", dynamic_lcd_type);
			aid_dimming_dynamic.elvss_len = S6E3HA2_ELVSS_LEN;
			aid_dimming_dynamic.elvss_reg = S6E3HA2_ELVSS_REG;
			aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA2_ELVSS_CMD_CNT;
			aid_dimming_dynamic.aid_cmd_cnt = S6E3HA2_AID_CMD_CNT;
			aid_dimming_dynamic.aid_reg_offset = S6E3HA2_AID_REG_OFFSET;
			aid_dimming_dynamic.tset_len = S6E3HA2_TSET_LEN;
			aid_dimming_dynamic.tset_reg = S6E3HA2_TSET_REG;
			aid_dimming_dynamic.tset_minus_offset = S6E3HA2_TSET_MINUS_OFFSET;
			aid_dimming_dynamic.vint_reg2 = S6E3HA2_VINT_REG2;
			break;
		}
	return 0;
}
early_param("lcdtype", get_lcd_type);

