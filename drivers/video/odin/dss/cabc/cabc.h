/*
 * linux/drivers/video/odin/dss/cabc.h
 *
 * Copyright (C) 2012 LGE
 * Author: suhwa.kim
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CABC_H__
#define __CABC_H__

#include <video/odindss.h>

/*****************CABC REG control***************/
#define CABC_REG_MASK(idx) \
	(((1 << ((DU_##idx##_START) - (DU_##idx##_END) + 1)) - 1) \
	   << (DU_##idx##_END))
#define CABC_REG_VAL(val, idx) ((val) << (DU_##idx##_END))
#define CABC_REG_GET(val, idx) \
	(((val) & CABC_REG_MASK(idx)) >> (DU_##idx##_END))
#define CABC_REG_MOD(orig, val, idx) \
    	(((orig) & ~CABC_REG_MASK(idx)) | CABC_REG_VAL(val, idx))

#define ODIN_CABC_MAX_WIDTH	1080
#define ODIN_CABC_MAX_HEIGHT	1920


#define ODIN_CABC_MAX_RES		(1)
#define ODIN_CABC_RES		(0)


#define CABC_CHANNEL_OFFSET(a)		((a == 1) ? 0x38 : 0)
#define CABC_ENABLE (1)
#define CABC_DISABLE (0)

#define MAXGAINLUT (256)
#define MAXHISTLUT (64)

#define CABC_GAIN_LUT_SIZE 128

typedef struct{
    unsigned int w;
    unsigned int h;
}CABC_SIZE;


typedef enum
{
    CABC_GAIN_1024 = 0,
    CABC_GAIN_512,
    CABC_GAIN_256,
    CABC_GAIN_RANGE_MAX,
}CABC_GAINRANGE;

typedef enum
{
    CABC_HISTSTEP_32 = 0,
    CABC_HISTSTEP_64,
}CABC_HISTSTEP;

typedef enum
{
    CABC_INTRMASK_DISABLE = 0,
    CABC_INTRMASK_ENABLE,
}CABC_INTRMASK;

typedef enum
{
    CABC_LUT_WRITE = 0,
    CABC_LUT_READ,
}CABC_LUTCTRL;

typedef enum
{
    CABC_APL_Y = 0,
    CABC_APL_R,
    CABC_APL_G,
    CABC_APL_B,
    CABC_MAX,
}CABC_APL;

typedef enum
{
    CABC_YVALUE_MAX = 0,
    CABC_YVALUE_MIN,
}CABC_YVALUE;

typedef enum
{
    CABC_HIST_TOP1 = 0,
    CABC_HIST_TOP2,
    CABC_HIST_TOP3,
    CABC_HIST_TOP4,
    CABC_HIST_TOP5,
    CABC_HIST_TOPMAX,
}CABC_HIST_TOP;

typedef enum
{
    CABC_HISTLUTDMA = 0,
    CABC_GAINLUTDMA,
}CABC_SELDMA;

typedef enum
{
    CABC_DMASTART = 1,
}CABC_DMACTRL;

typedef enum
{
    CABC_GAINLUT_DMAMODE = 0,
    CABC_GAINLUT_APBMODE,
}CABC_GAINLUTOPMODE;

typedef enum
{
    CABC_HISTLUT_AUTOMODE = 0,
    CABC_HISTLUT_DMAMODE,
    CABC_HISTLUT_APBMODE,
}CABC_HISTLUTOPMODE;

typedef enum
{
    CABC_GAMMA_22 = 0,
    CABC_GAMMA_19,
    CABC_GAMMA_17,
}CABC_GAMMA;


typedef struct {
    unsigned int en;
    int *hist_lut;

    CABC_HISTLUTOPMODE hist_mode;
    CABC_HISTSTEP hist_step;
    unsigned int i_hist_top[5];

    int *gain_lut;
    unsigned int global_gain;
    CABC_GAINLUTOPMODE gain_mode;
    CABC_GAINRANGE range;
    unsigned int gain_lut_en;

    CABC_SIZE size;

    int i_cdf_thr;              /* 0 ~ 1 -> 0 ~ 100 : percent */
    int i_ctrl_clip_range;
    CABC_GAMMA i_gamma;              /* 22(gamma) / 19, 17 */
    unsigned int i_mode_change;

    unsigned int o_back_factor;        /* 0 ~ 1 -> 0 ~ 0xFF */
    unsigned int o_rgb_gain;           /* 0x100 / 0x200 / 0x400 ~ 0xFFF */
    unsigned int i_ctrl_saturation;

}CABC_CONFIG;


typedef enum
{
    CABC_TEST_EXIT = 0,
    READ_HIST_LUT_DMA_INT = 1,
    READ_HIST_LUT_DMA_START,
    READ_HIST_LUT_APB,
    WRITE_GAIN_LUT_DMA,
    WRITE_GAIN_LUT_APB,
    CHECK_GLOBAL_GAIN_VAL,
    CSC_CTRL,
    CHECK_HIST_LUT_STEP,
    CHECK_APL,
    CHECK_MIN_MAX_VAL,
    CHECK_TOT_PIXEL_CNT,
    CHECK_HIST_TOP5,
    CABC_TEST_MAX,
    CABC_TEST_ALGORITHM,
    CABC_TEST_SEL,
}CABC_TESTFSM;

typedef struct {
	int r_coef;
	int g_coef;
	int b_coef;
}CSC_COEF;

typedef struct {
	int channel;
	int width;
	int height;
	int hist_step;
	int global_gain;

	CSC_COEF coef;
	u8 *hist_lut_addr;
	u8 *gain_lut_addr;
}CABC_INFO;

typedef struct {
      int cabc_ch;
      int num;
}CABC_INIT;




enum cabc_opr {
	CABC_OPEN,
	CABC_GAIN_CTRL,
	CABC_CLOSE,
};

void odin_set_cabc_size(int cabc_ch, int width, int height);
int odin_cabc_cmd(int ch, enum cabc_opr cmd);
bool odin_cabc_is_on(void);


#define ODIN_CABC_MAGIC		'C'

/* IOCTLS */
#define ODIN_CABCIOC_CABC_VSYNC	\
                              _IOW(ODIN_CABC_MAGIC, 100,  CABC_CONFIG)
#define ODIN_CABCIOC_CABC_SET \
                             _IOW(ODIN_CABC_MAGIC, 101,  CABC_INIT)
#define ODIN_CABCIOC_CABC_ENA 	\
                             _IOW(ODIN_CABC_MAGIC, 102,  CABC_INFO)



#endif	/* __CABC_H__*/


