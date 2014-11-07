/*
 * linux/drivers/video/odin/ dss/cabc.c
 *
 * Copyright (C) 2012 LG Electronics
 * Author: suhwa.kim <suhwa.kim@lge.com>
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
 * this program.  If not, see <http://www.gnu.org/licenses/>
 */

#define DRV_NAME 	"odindss-cabc"

//#define DEBUG

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
#include <linux/export.h>
#endif
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>

#include <linux/ion.h>
#include <linux/odin_iommu.h>
#include <video/odindss.h>
#include <linux/mfd/max77819-wled.h>
#include <linux/pwm.h>
#include <linux/ctype.h>
#include "../dss.h"
#include "cabc.h"
#include "cabc_reg_def.h"
#include "cabc_union_reg.h"
#include "cabc-sysfs.h"

// HISTOGRAM LUT data(64 ea x 23 BIT)
#define CABC_SIZE_HIST_LUT (64*4)
// GAIN LUT DATA(256 ea x 12 BIT)
#define CABC_SIZE_GAIN_LUT (256*2)

struct ion_allocation_data alloc_data_histlut;
struct ion_allocation_data alloc_data_gainlut;
struct ion_client *client;

static CABC_CONFIG *cabc_config;
struct pwm_device *cabc_pwm_dev;

static bool odin_cabc_onoff = false;
static bool odin_cabc_log = false;
static bool odin_cabc_log2 = false;

static struct {
	struct platform_device *pdev;
	struct resource		*iomem[ODIN_CABC_MAX_RES];
	void __iomem    		*base[ODIN_CABC_MAX_RES];
	int irq;
}cabc_dev;

static inline void cabc_write_reg(u32 addr, u32 val)
{
    __raw_writel(val, cabc_dev.base[ODIN_CABC_RES] + addr);
}

static inline u32 cabc_read_reg(u32 addr)
{
    return __raw_readl(cabc_dev.base[ODIN_CABC_RES] + addr);
}

void odin_clear_lut(const unsigned int lutSize, unsigned int *adrs)
{
    unsigned int index;

    for (index=0;index<lutSize;index++)
    {
        *(adrs+index)=0;
    }

}

void odin_copy_lut(const unsigned int lutSize,
    unsigned int *adrs, unsigned int *cpyadrs)
{
    unsigned int size = lutSize;
    unsigned int i;
    DSSDBG(" adrs = %x \n", adrs);
     for (i = 0; i < size; i++)
    {
    	*(cpyadrs+size-1) = *(adrs+size-1);
        DSSDBG("\t-->Lut %03d read1 Val : 0x%08X\n", i, *((adrs+i)));
    	size--;
    }
}


void odin_out_cabc_lut(const unsigned int lutSize, unsigned int *adrs)
{
    unsigned int i;

    for (i = 0; i < lutSize; i++)

    {
        DSSDBG("\t-->Lut %d Value : 0x%08X\n", i, *(adrs+i));
    }
}



void odin_cmp_cabc_hist_lut(const unsigned int lutSize, unsigned int *adrs,
														unsigned int *cmpadrs)
{
    unsigned int i, ret;
    unsigned int cmpcnt = 0;

   for (i = 0; i < lutSize; i++)
   {
       if (*(adrs+i) != *(cmpadrs+i))
       {
           cmpcnt++;
           DSSDBG("addr : 0x%08X   index : %d ", (adrs+i), i);
           DSSDBG("differ : 0x%08X\n", *(adrs+i) ^ *(cmpadrs+i));
       }
   }

   ret  = memcmp(adrs, cmpadrs, lutSize*4);
   if (ret) DSSERR("error\n");
   else DSSDBG("pass\n");

}

void odin_cmp_cabc_gain_lut(const unsigned int lutSize, unsigned int *adrs,
			    unsigned int *cmpadrs)
{
    unsigned int i, ret;
    unsigned int cmpcnt = 0;

   for (i = 0; i < lutSize; i++)
   {
       if (*(adrs+i) != *(cmpadrs+i))
       {
           cmpcnt++;
           DSSDBG("addr : 0x%08X   index : %d ", (adrs+i), i);
           DSSDBG("differ : 0x%08X\n", *(adrs+i) ^ *(cmpadrs+i));
       }
   }

   ret  = memcmp(adrs, cmpadrs, lutSize*4);
   if (ret) DSSERR("error\n");
   else DSSDBG("pass\n");

}

void _odin_cabc_enable(int cabc_ch)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
	val = CABC_REG_MOD(val, CABC_ENABLE, CABC_EN);
	cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
	DSSDBG("cabc enable~!!\n");
}

void _odin_cabc_disable(int cabc_ch)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
	val = CABC_REG_MOD(val, CABC_DISABLE, CABC_EN);
	cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
	DSSDBG("cabc disable~!!\n");
}

void odin_cabc_enable(int cabc_en)
{
    if (cabc_en)
         _odin_cabc_enable(ODIN_DSS_CHANNEL_LCD0);
    else
        _odin_cabc_disable(ODIN_DSS_CHANNEL_LCD0);
}
static void _odin_set_cabc_size(int cabc_ch, int width, int height)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_SIZE_CTRL);
	val = CABC_REG_MOD(val, width, IMAGE_WIDTH_SIZE);
	val = CABC_REG_MOD(val, height, IMAGE_HEIGHT_SIZE);
	cabc_write_reg(ADDR_CABC1_SIZE_CTRL, val);

}

void odin_set_cabc_size(int cabc_ch, int width, int height)
{
    _odin_set_cabc_size(cabc_ch, width, height);
}

static void _odin_set_cabc_csc_coef (int cabc_ch, int r_coef,
					int g_coef, int b_coef)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_CSC_CTRL);
	val = CABC_REG_MOD(val, r_coef, R_COEF);
	val = CABC_REG_MOD(val, g_coef, G_COEF);
	val = CABC_REG_MOD(val, b_coef, B_COEF);
	cabc_write_reg(ADDR_CABC1_CSC_CTRL, val);
}

static void _odin_set_cabc_interruptmask(int cabc_ch, CABC_INTRMASK intrmask)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
	val = CABC_REG_MOD(val, intrmask, INTERRUPT_MASK);
	cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
}
#if 0
static void _odin_set_cabc_top_ctrl(int cabc_ch, int hist_step,
						int global_gain)
{

	u32 val;
	val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
	val = CABC_REG_MOD(val, hist_step, HIST_STEP_MODE);
	val = CABC_REG_MOD(val, 1024 >> hist_step, CABC_GAIN_RANGE);
	val = CABC_REG_MOD(val, global_gain, CABC_GLOBAL_GAIN);
	cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);

}
#endif
void _odin_set_cabc_gain_lut_ctrl(int cabc_ch, unsigned int lutaddr,
						unsigned int wdata)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
	val = CABC_REG_MOD(val, CABC_LUT_WRITE, CABC_LUT_WEN);
	val = CABC_REG_MOD(val, lutaddr, CABC_LUT_ADRS);
	val = CABC_REG_MOD(val, wdata, CABC_LUT_WDATA);
	cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);
}

unsigned int _odin_get_cabc_gain_lut_data(int cabc_ch, unsigned int lutaddr)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
	val = CABC_REG_MOD(val, CABC_LUT_READ, CABC_LUT_WEN);
	val = CABC_REG_MOD(val, lutaddr, CABC_LUT_ADRS);
	cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);

	val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_DATA);
	val = CABC_REG_GET(val, CABC_LUT_RDATA);

	return val;
}

unsigned int _odin_get_cabc_hist_lut_data(int cabc_ch, unsigned int histaddr)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_HIST_LUT_ADRS);
	val = CABC_REG_MOD(val, histaddr, HIST_LUT_ADRS);
	cabc_write_reg(ADDR_CABC1_HIST_LUT_ADRS, val);

	val = cabc_read_reg(ADDR_CABC1_HIST_LUT_RDATA);
	val = CABC_REG_GET(val, HIST_LUT_RDATA);

	return val;
}

unsigned int _odin_get_cabc_apl(int cabc_ch, CABC_APL factor)
{
	u32 val;
	val = cabc_read_reg(ADDR_CABC1_APL);
       if (factor == CABC_APL_Y)
       {
            val = CABC_REG_GET(val, APL_Y);
       }

       else  if (factor == CABC_APL_R)
       {
            val = CABC_REG_GET(val, APL_R);
       }

        else  if (factor == CABC_APL_G)
       {
            val = CABC_REG_GET(val, APL_G);
       }

         else  if (factor == CABC_APL_B)
       {
            val = CABC_REG_GET(val, APL_B);
       }


    return val;
}
void odin_out_cabc_apl(int cabc_ch)
{
    DSSINFO("\tY APL Value : 0x%02X\n",
		_odin_get_cabc_apl(cabc_ch, CABC_APL_Y));
    DSSINFO("\tY APL Value : 0x%02X\n",
		_odin_get_cabc_apl(cabc_ch, CABC_APL_R));
    DSSINFO("\tY APL Value : 0x%02X\n",
		_odin_get_cabc_apl(cabc_ch, CABC_APL_G));
    DSSINFO("\tY APL Value : 0x%02X\n",
		_odin_get_cabc_apl(cabc_ch, CABC_APL_B));
}

unsigned int _odin_get_cabc_minmax_yvalue(int cabc_ch, CABC_YVALUE factor)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_MAX_MIN);
    if (factor == CABC_YVALUE_MIN)
        val = CABC_REG_GET(val, MIN_Y);

    else if (factor == CABC_YVALUE_MAX)
        val = CABC_REG_GET(val, MAX_Y);

    return val;
}
unsigned int _odin_get_cabc_totalpixelcnt(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_TOTAL_PIXEL_CNT);
    val = CABC_REG_GET(val, TOTAL_PIXEL_CNT);
    return val;
}


unsigned int _odin_get_cabc_hist_top5(int cabc_ch, CABC_HIST_TOP num)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_HIST_TOP5);
    switch (num)
    {
        case CABC_HIST_TOP1:
            val = CABC_REG_GET(val, HIST_TOP1);
            break;
        case CABC_HIST_TOP2:
            val = CABC_REG_GET(val, HIST_TOP2);
            break;
        case CABC_HIST_TOP3:
            val = CABC_REG_GET(val, HIST_TOP3);
            break;
        case CABC_HIST_TOP4:
            val = CABC_REG_GET(val, HIST_TOP4);
            break;
        case CABC_HIST_TOP5:
            val = CABC_REG_GET(val, HIST_TOP5);
            break;
		default:
			break;
    }
    return val;
}

void _odin_set_cabc_dma_ctrl(int cabc_ch, unsigned int addr)
{
    u32 val;
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_BADDR, addr);

    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, CABC_GAINLUT_DMAMODE, GAIN_LUT_OPERATION_MODE);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);

    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, CABC_DMASTART, GAIN_LUT_DAM_START);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);
}

void _odin_set_cabc_gain_lut_wen(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
    val = CABC_REG_MOD(val, CABC_LUT_WRITE, CABC_LUT_WEN);
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);
}

void _odin_get_cabc_gain_lut_wen(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
    val = CABC_REG_MOD(val, CABC_LUT_READ, CABC_LUT_WEN);
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);
}

void _odin_set_cabc_gain_lut_wdata(int cabc_ch, unsigned int data)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
    val = CABC_REG_MOD(val, data, CABC_LUT_WDATA);
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);
}

unsigned int _odin_get_cabc_gain_lut_rdata(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_DATA);
    val = CABC_REG_GET(val, CABC_LUT_RDATA);
    return val;
}

void _odin_set_cabc_gain_lut_adrs(int cabc_ch, unsigned int index)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
    val = CABC_REG_MOD(val, index, CABC_LUT_ADRS);
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);
}

void _odin_set_cabc_gain_lut(int cabc_ch,unsigned int index,
						unsigned int data)
{
    unsigned int write_data = 0;
     u32 val;
    write_data = (data<<16) | (index<<1);
    val = cabc_read_reg(ADDR_CABC1_GAIN_LUT_CTRL);
    /*val = CABC_REG_MOD(val, writeData, CABC_LUT_ADRS);*/
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_CTRL, val);

}

void _odin_set_cabc_gain_lut_write(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, CABC_DMASTART, GAIN_LUT_DAM_START);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);
}

void _odin_set_cabc_global_gain(int cabc_ch, unsigned int globalgain)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
    val = CABC_REG_MOD(val, globalgain, CABC_GLOBAL_GAIN);
    cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
}

void _odin_set_cabc_gain_lut_range(int cabc_ch, CABC_GAINRANGE range)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
    val = CABC_REG_MOD(val, range, CABC_GAIN_RANGE);
    cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
}

void _odin_set_cabc_gain_lut_mode(int cabc_ch, CABC_GAINLUTOPMODE mode)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, mode, GAIN_LUT_OPERATION_MODE);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);
}

void _odin_set_cabc_gain_lut_baseaddr(int cabc_ch, unsigned int addr)
{
    cabc_write_reg(ADDR_CABC1_GAIN_LUT_BADDR, addr);
}

void _odin_set_cabc_hist_lut_mode(int cabc_ch, CABC_HISTLUTOPMODE mode)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, mode, HIST_LUT_OPERATION_MODE);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);
}

void _odin_set_cabc_hist_lut_step(int cabc_ch, CABC_HISTSTEP step)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
    val = CABC_REG_MOD(val, step, HIST_STEP_MODE);
    cabc_write_reg(ADDR_CABC1_TOP_CTRL, val);
}

unsigned int _odin_get_cabc_hist_lut_step(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
    val = CABC_REG_GET(val, HIST_STEP_MODE);
    return val;
}

void _odin_set_cabc_hist_lut_baseaddr(int cabc_ch, unsigned int addr)
{
    cabc_write_reg(ADDR_CABC1_HIST_LUT_BADDR, addr);
}

void _odin_set_cabc_hist_lut_read(int cabc_ch)
{
    u32 val;
    val = cabc_read_reg(ADDR_CABC1_DMA_CTRL);
    val = CABC_REG_MOD(val, CABC_DMASTART, HIST_LUT_DMA_START);
    cabc_write_reg(ADDR_CABC1_DMA_CTRL, val);
}

unsigned int _odin_get_cabc_hist_lut_read_apb (int cabc_ch,
					unsigned int index)
{
    u32 val;
	val = cabc_read_reg(ADDR_CABC1_HIST_LUT_ADRS);
	val = CABC_REG_MOD(val, index, HIST_LUT_ADRS);
	cabc_write_reg(ADDR_CABC1_HIST_LUT_BADDR, val);

	val = cabc_read_reg(ADDR_CABC1_HIST_LUT_RDATA);
	val = CABC_REG_GET(val, HIST_LUT_RDATA);

	return val;
}
int odin_get_cabc_hist_lut(int cabc_ch, unsigned int *adrs,
                                         int mode, int step)
{
    unsigned int i;
    unsigned int index, lutsize;
    unsigned int *histut0 = NULL;
    unsigned int *histut1 = NULL;
	int ret = 0;

    if (step == CABC_HISTSTEP_32)
    {
        lutsize = 32;
        DSSDBG(" Hist Lut 32 Step\n");
    }
    else
    {
        lutsize = 64;
        DSSDBG(" Hist Lut 64 Step\n");
    }

    histut0 = kzalloc(lutsize*4, GFP_KERNEL);
	if(!histut0) {
		ret = -ENOMEM;
		goto err_alloc;
	}
    histut1 = kzalloc(lutsize*4, GFP_KERNEL);
	if(!histut1) {
		ret = -ENOMEM;
		goto err_alloc;
	}

    _odin_set_cabc_hist_lut_step(cabc_ch, step);
    _odin_set_cabc_hist_lut_mode(cabc_ch, mode);

    if (mode == CABC_HISTLUT_AUTOMODE)
    {
        for (i = 0; i < 2; i++)
        {
            /* interrupt setting */

            if (i)
            {
                odin_copy_lut(lutsize, adrs, histut0);
                cabc_config->hist_lut = adrs;
            }
            else
            {
                odin_copy_lut(lutsize, adrs, histut1);
                cabc_config->hist_lut = adrs;
            }
        }

        DSSDBG("Hist LUT When generate CABC interrupt\n");
    }
    else if (mode == CABC_HISTLUT_DMAMODE)
    {
        DSSDBG("Hist LUT Before generate Hist DMA Start signal\n");
        for (i = 0; i < 2; i++)
        {
            _odin_set_cabc_hist_lut_read(cabc_ch);

            if (i)
            {
                odin_copy_lut(lutsize, adrs, histut0);
                cabc_config->hist_lut = adrs;

            }
            else
            {
                odin_copy_lut(lutsize, adrs, histut1);
                cabc_config->hist_lut = adrs;
            }
        }
    }
    else if (mode == CABC_HISTLUT_APBMODE)
    {
        DSSDBG("Hist LUT Before using APB interface\n");
        for (i = 0; i < 2; i++)
        {
            for (index  = 0; index < lutsize; index++)
            {
                *(adrs+index)=
                    _odin_get_cabc_hist_lut_read_apb(cabc_ch, index);

                msleep(0.5);

            }

            if (i)
            {
                odin_copy_lut(lutsize, adrs, histut0);
                cabc_config->hist_lut = adrs;
            }
            else
            {
                odin_copy_lut(lutsize, adrs, histut1);
                cabc_config->hist_lut = adrs;
                DSSDBG(" adrs in APB = %x \n", cabc_config->hist_lut);
            }
        }
    }

    odin_cmp_cabc_hist_lut(lutsize, histut0, histut1);

    kfree(histut0);
    kfree(histut1);

    return ret;

err_alloc:
	if(!histut1) {
		if(histut0)
			kfree(histut0);
	}
	return ret;
}
int  odin_set_cabc_gain_lut(int cabc_ch, unsigned int *adrs,
                     CABC_GAINLUTOPMODE mode, CABC_GAINRANGE range)
{
    unsigned int index;
    unsigned int gainlev, gain;
    unsigned int lutsize = CABC_GAIN_LUT_SIZE;
    unsigned int *gainlut;
	int ret = 0;

    gainlut = kzalloc(lutsize*4, GFP_KERNEL);
	if(!gainlut) {
		ret = -ENOMEM;
		goto err_alloc;
	}

    for (index = 0; index < lutsize; index++)
    {
        *(adrs+index)=0;
    }

    _odin_set_cabc_gain_lut_range(cabc_ch, range);
    _odin_set_cabc_global_gain(cabc_ch, 0x400 >> range);
    gainlev = (range==0)?1024:(range==1)?512:256;
    DSSDBG("x1 gain Range(Level) : %d\n", gainlev);


    gain = (range==0)?100:(range==1)?50:25;
    for (index=0; index < lutsize; index++)
    {
        *(adrs+index)= (((index*2+1) * gain) % 0xFFF)
            <<16 | ((index*2 * gain) % 0xFFF);
    }
    /* setting gain lut mode : dma or apb */
    _odin_set_cabc_gain_lut_mode(cabc_ch, mode);

    /* writing gain lut */
    if (mode == CABC_GAINLUT_DMAMODE)
    {
        _odin_cabc_disable(cabc_ch);
        _odin_set_cabc_gain_lut_wen(cabc_ch);
        _odin_set_cabc_gain_lut_write(cabc_ch);
    }
    else if (mode == CABC_GAINLUT_APBMODE)
    {
        DSSDBG("Write Gain Lut with APB Interface\n");
        /* gain lut write enable : 0. read : 1 */
        _odin_set_cabc_gain_lut_wen(cabc_ch);
        _odin_cabc_disable(cabc_ch);

         for (index = 0; index < lutsize; index++)
        {
            #if 1
            _odin_set_cabc_gain_lut(cabc_ch, index*2,
                                            *(adrs+index) & 0xFFF);
            _odin_set_cabc_gain_lut(cabc_ch, index*2 + 1,
                (*(adrs+index)>>16) & 0xFFF);

            #else
            #endif

        }
    }
    else if (mode == 11)
    {
        DSSDBG("Write Gain Lut with APB Interface\n");

        _odin_set_cabc_gain_lut_wen(cabc_ch);
        _odin_set_cabc_gain_lut_adrs(cabc_ch, 0);
        _odin_cabc_disable(cabc_ch);

        for (index = 0; index < lutsize; index++)
        {
            _odin_set_cabc_gain_lut_adrs(cabc_ch, index*2);
            _odin_set_cabc_gain_lut_wdata(cabc_ch,
                                                        *(adrs+index) & 0xFFF);
            _odin_set_cabc_gain_lut_adrs(cabc_ch, index*2 + 1);
            _odin_set_cabc_gain_lut_wdata(cabc_ch,
                                              (*(adrs+index)>>16) & 0xFFF);
        }

    }

    DSSDBG("Read Gain Lut with APB Interface\n");
    _odin_set_cabc_gain_lut_mode(cabc_ch,
                                                CABC_GAINLUT_APBMODE);
    _odin_set_cabc_gain_lut_wen(cabc_ch);
    _odin_set_cabc_global_gain(cabc_ch, 0x400>>range);
    _odin_cabc_disable(cabc_ch);

    for (index = 0; index < lutsize; index++)
    {
        #if 1
        *(gainlut+index) = 0;
        _odin_set_cabc_gain_lut_adrs(cabc_ch, index*2);
        *(gainlut+index) |= _odin_get_cabc_gain_lut_rdata(cabc_ch) & 0xFFF;
        _odin_set_cabc_gain_lut_adrs(cabc_ch, index*2 + 1);
        *(gainlut+index) |= (_odin_get_cabc_gain_lut_rdata(cabc_ch)
            & 0xFFF) << 16;
        #else
        #endif
    }

    odin_cmp_cabc_gain_lut(lutsize, adrs, gainlut);
    kfree(gainlut);

err_alloc:
	return ret;
}

void odin_check_cabc_global_gain_val(int cabc_ch)
{
    int i;
    unsigned int range;
    unsigned int gainlev;

    /*range 0 : 1024 , 1 : 512, 2 : 256*/
   for (range = 0; range < CABC_GAIN_RANGE_MAX; range++)
   {
        _odin_set_cabc_gain_lut_adrs(cabc_ch, 0);
        _odin_cabc_disable(cabc_ch);
        _odin_set_cabc_gain_lut_range(cabc_ch, range);
        _odin_set_cabc_global_gain(cabc_ch, 0x400>>range);
        gainlev = (range==0)?1024:(range==1)?512:256;
        DSSDBG("x1 gain Range(Level) : %d\n", gainlev);

        DSSDBG("become brighter than image color\n");

        for (i = gainlev; i < 0xFFF; i++)
        {
            DSSDBG("gain Range(Level) : %d\n", i);
            _odin_set_cabc_global_gain(cabc_ch, i);
            mdelay(10);
        }

        DSSDBG("become darker than image color\n");
        for (i = gainlev; i > 0; i--)
        {
            DSSDBG("gain Range(Level) : %d\n", i);
            _odin_set_cabc_global_gain(cabc_ch, i);
            mdelay(10);
        }

   }
   _odin_set_cabc_gain_lut_range(cabc_ch, range);
   _odin_set_cabc_global_gain(cabc_ch, 0x400>>range);
}

int odin_check_rgb_coef_csc_ctrl(int cabc_ch, unsigned int *hist_lut)
{
    unsigned int i;
    unsigned int pat = 0;
    int ret = 0;

    /*set_ovl_patter_data*/
    for (i = 0; i < 3; i++)
    {
        pat = 0xFF<< (8*i);
        _odin_set_cabc_csc_coef(cabc_ch, pat&0xff,
            (pat>>8)&0xff, (pat>>16)&0xff);
        ret = odin_get_cabc_hist_lut(cabc_ch, hist_lut,
                                CABC_HISTLUT_AUTOMODE, CABC_HISTSTEP_32);
		if(ret < 0) {
			goto err;
		}
    }
    _odin_set_cabc_csc_coef(cabc_ch, 0x132, 0x259, 0x75);

err:
return ret;

}

void odin_check_hist_lut_step(int cabc_ch,
                                        unsigned int *hist_lut,
                                        unsigned int totPixelCnt)
{
    unsigned int i, x;
    unsigned int maxpixeldat = 256;
    unsigned int checkcnt;

    unsigned int histlutstepfixval = 2;
    unsigned int histlutstep64fixval = 62;

    for (i=0;i<maxpixeldat;i++)
    {
        _odin_set_cabc_hist_lut_baseaddr(cabc_ch, (unsigned int)hist_lut);
        _odin_set_cabc_hist_lut_mode(cabc_ch, CABC_HISTLUT_AUTOMODE);
        _odin_set_cabc_interruptmask(cabc_ch, CABC_INTRMASK_DISABLE);
        _odin_set_cabc_hist_lut_step(cabc_ch, CABC_HISTSTEP_32);

/*
        // about 1 frame delay
        //hTIMER_Wait(500);

        // 32 step table
        // [0] - [0] - 0 - 1 - 2 - 3 - … - … - 28 - 29 - 30 - 31 - [0] - [0]*/
        checkcnt=0;
        for (x=0;x<32+histlutstepfixval;x++)
        {
            if (*(hist_lut+x)!=0)
            {
                if (totPixelCnt != *(hist_lut+x))
                    DSSERR("error. %d of 32step hist lut(Dat %03d /8 = %d)\n",
                    x-histlutstepfixval,i,i/8);
                else
                    if (x<histlutstepfixval)
                        DSSERR("error. Not used Table\n");
                    else
                        DSSDBG(
                        "pass. %d of 32step hist lut(Dat %03d /8 = %d)\n",
                        x-histlutstepfixval,i,i/8);

                checkcnt++;
            }
        }

        if (checkcnt != 1)
        {
            DSSERR("error. %d times\n",checkcnt);
        }
        checkcnt = 0;

        odin_out_cabc_apl(cabc_ch);
    }

    for (i=0;i<maxpixeldat;i++)
    {
        _odin_set_cabc_hist_lut_baseaddr(cabc_ch, (unsigned int)hist_lut);
        _odin_set_cabc_hist_lut_mode(cabc_ch, CABC_HISTLUT_AUTOMODE);
        _odin_set_cabc_interruptmask(cabc_ch, CABC_INTRMASK_DISABLE);
        _odin_set_cabc_hist_lut_step(cabc_ch, CABC_HISTSTEP_32);

        /* about 1 frame delay */
        /* hTIMER_Wait(500); */
        /* 64 step  table  */
        /* 62 - 63 - 0 - 1 - 2 - 3 - … - … - 60 - 61 */
        for (x=0;x<64;x++)
        {
            if (*(hist_lut+x)!=0)
            {
                if (totPixelCnt != *(hist_lut+x))
                {
                    if (x<histlutstepfixval)
                        DSSERR(
                        "error. %d of 64step hist lut(Dat %03d /4 = %d)\n",
                        x+histlutstep64fixval,i,i/4);
                    else
                        DSSERR(
                        "error. %d of 64step hist lut(Dat %03d /4 = %d)\n",
                        x-histlutstepfixval,i,i/4);
                }
                else
                {
                    if (x<histlutstepfixval)
                        DSSDBG(
                        "pass. %d of 64step hist lut(Dat %03d /4 = %d)\n",
                        x+histlutstep64fixval,i,i/4);
                    else
                        DSSDBG(
                        "pass. %d of 64step hist lut(Dat %03d /4 = %d)\n",
                        x-histlutstepfixval,i,i/4);
                }

                checkcnt++;
            }
        }

        if (checkcnt != 1)
        {
            DSSERR("error. %d times\n",checkcnt);
        }
        checkcnt = 0;

        odin_out_cabc_apl(cabc_ch);
    }
    /* hDU_Set_Ovl_Pattern_Data_Disble(ovlch); */
}

static unsigned int frm_cnt = 0;
static int clipping_val[1][2] = {{0}};

static int b_factor_64_lut_gamma_22[64] = {
      0,   0,   0,   0,   0,   1,   1,   2,   3,   4,
      5,   6,   7,   9,  10,  12,  13,  15,  17,  19,
     21,  24,  26,  29,  32,  35,  38,  41,  44,  48,
     51,  55,  59,  63,  67,  71,  76,  80,  85,  90,
     95, 100, 106, 111, 117, 123, 129, 135, 141, 148,
    154, 161, 168, 175, 182, 190, 197, 205, 213, 221,
    229, 237, 246, 255,
};

static int b_factor_64_lut_gamma_19[64] = {
    0,   0,   0,   1,   2,   2,   3,   4,   6,   7,
    8,   10,  12,  14,  16,  18,  20,  22,  25,  27,
    30,  33,  36,  39,  42,  46,  49,  53,  56,  60,
    64,  68,  72,  76,  81,  85,  90,  94,  99,  104,
    109, 114, 119, 125, 130, 136, 141, 147, 153, 159,
    165, 171, 178, 184, 191, 197, 204, 211, 218, 225,
    232, 240, 247, 255
};

static int b_factor_64_lut_gamma_17[64] = {
    0,   0,   1,   2,   3,   4,   5,   7,   9,   10,
    12,  14,  16,  19,  21,  24,  26,  29,  32,  35,
    38,  41,  44,  48,  51,  55,  58,  62,  66,  70,
    74,  78,  82,  87,  91,  95,  100, 105, 109, 114,
    119, 124, 129, 134, 140, 145, 150, 156, 161, 167,
    173, 179, 185, 191, 197, 203, 209, 215, 222, 228,
    235, 241, 248, 255
};

static int b_factor_32_lut_gamma_22[32] = {
    0,   0,   1,   2,   4,   6,   9,   12,  15,  19,
    24,  29,  35,  41,  48,  55,  63,  71,  80,  90,
    100, 111, 123, 135, 148, 161, 175, 190, 205, 221,
    237, 255
};

static int b_factor_32_lut_gamma_19[32] = {
    0,   1,   2,   4,   7,   10,  14,  18,  22,  27,
    33,  39,  46,  53,  60,  68,  76,  85,  94,  104,
    114, 125, 136, 147, 159, 171, 184, 197, 211, 225,
    240, 255
};

static int b_factor_32_lut_gamma_17[32] = {
    0,   2,   4,   7,   10,  14,  19,  24,  29,  35,
    41,  48,  55,  62,  70,  78,  87,  95,  105, 114,
    124, 134, 145, 156, 167, 179, 191, 203, 215, 228,
    241, 255
};

int odin_gamma_lut(int clipping_val, int step, CABC_GAMMA gamma)
{
    int ret_b_factor = 0;

    if (step == CABC_HISTSTEP_64)
    {
        if (gamma == CABC_GAMMA_22)
        {
            ret_b_factor = b_factor_64_lut_gamma_22[clipping_val-1];
            DSSDBG("ret_b_factor = %d\n",ret_b_factor);
        }
        else if (gamma == CABC_GAMMA_19)
        {
            ret_b_factor = b_factor_64_lut_gamma_19[clipping_val-1];
        }
        else
        {
            ret_b_factor = b_factor_64_lut_gamma_17[clipping_val-1];
        }
    }
    else
    {
        if (gamma == CABC_GAMMA_22)
        {
            ret_b_factor = b_factor_32_lut_gamma_22[clipping_val-1];
        }
        else if (gamma == CABC_GAMMA_19)
        {
            ret_b_factor = b_factor_32_lut_gamma_19[clipping_val-1];
        }
        else
        {
            ret_b_factor = b_factor_32_lut_gamma_17[clipping_val-1];
        }
    }

    return ret_b_factor;

}

unsigned int get_r_hist(unsigned int hist_step, unsigned int idx)
{
	unsigned int ret = 0;

	if(idx == 0)
		pr_info("i don't know");

	if(hist_step == 64) {
		if(idx == 64)
			ret = cabc_config->hist_lut[1];
		else if(idx == 63)
			ret = cabc_config->hist_lut[0];
		else
			ret = cabc_config->hist_lut[idx+1];
	} else if( hist_step == 32)
			ret =cabc_config->hist_lut[idx+1];

	return ret;
}

unsigned int get_map_hist(unsigned int hist_step,
		unsigned int hist_tot_cnt, unsigned int cdf_hist)
{
	unsigned int ret = 0;
	unsigned int nor_hist;

	nor_hist = (unsigned int) ((cdf_hist * 100)/hist_tot_cnt);

	ret = (int) ((nor_hist * (hist_step) + 50)/100);

	if (ret == 0)
		ret = ret + 1;

	return ret;
}

int odin_gain_cabc(unsigned int backlight_value)
{
    int i,j;
    unsigned int clipping_th= 0;
    unsigned int hist_tot_cnt        = 0;
    unsigned int clip_vals           = 0;
    unsigned int r_hist              = 0;
	unsigned int cdf_hist, cdf_hist_p = 0;
    unsigned int map_hist            = 0;
    unsigned int hist_eq[65]         = {0};
    unsigned int hist_eq_cdf, hist_eq_cdf_p = 0;

    unsigned int hist_step           = 0;
    unsigned int rgb_gain = 0;
    unsigned int back_factor = 0;

    /******************************/
    /* CABC Algorithm*/
    /******************************/
    clipping_val[0][0] = clipping_val[0][1];

    hist_tot_cnt = cabc_config->size.w * cabc_config->size.h;
    clipping_th = hist_tot_cnt * cabc_config->i_cdf_thr / 100;

    if (cabc_config->hist_step == CABC_HISTSTEP_64)
        hist_step = 64;
    else
        hist_step = 32;

    if (frm_cnt > 1)
    {
        for (i=1; i<hist_step; i++)
        {
			r_hist = get_r_hist(hist_step, i);

			cdf_hist = r_hist + cdf_hist_p;
			cdf_hist_p = cdf_hist;
			map_hist = get_map_hist(hist_step,
						hist_tot_cnt, cdf_hist);

            hist_eq[map_hist] = r_hist + hist_eq[map_hist];
        }

        for (j=1; j<hist_step+1; j++)
        {
            if (j == 1)
                hist_eq_cdf = hist_eq[1];
            else
                hist_eq_cdf = hist_eq[j] + hist_eq_cdf_p;

            hist_eq_cdf_p = hist_eq_cdf;


            if (hist_eq_cdf > clipping_th)
                clip_vals = j + cabc_config->i_ctrl_clip_range;
            else
                clip_vals = 0;

            if (clip_vals != 0)
            {
                clipping_val[0][1] = clip_vals;
                break;
            }

        }
        if (frm_cnt == 2)
        {
            back_factor
                = 1 * backlight_value;
            rgb_gain
                = 1 * (0x400>>cabc_config->range);
        }
        else
        {
            back_factor
                = odin_gamma_lut(clipping_val[0][1],
                        cabc_config->hist_step,
                        cabc_config->i_gamma);

            back_factor = (back_factor * backlight_value) / 0xff;

            if (clipping_val[0][0] == 0) clipping_val[0][0]=1;
            rgb_gain = ((0x400>>cabc_config->range) * hist_step) /
            		 clipping_val[0][0];

            if (rgb_gain >= 0xFFF)
                rgb_gain = 0xFFF;
        }
    }
    else
    {
        back_factor
            = 1 * backlight_value;
        rgb_gain
            = 1 * (0x400>>cabc_config->range);

    }

    if (clipping_val[0][0] < 10)
    {
        back_factor = 1 * backlight_value;
        rgb_gain = 1 * (0x400>>cabc_config->range);
    }

    if (frm_cnt != 0xffffffff)
        frm_cnt++;
    else
        frm_cnt = 4;

    if (cabc_config->i_mode_change)
    {
        frm_cnt = 0;
        clipping_val[0][0] = 0;
        clipping_val[0][1] = 0;
        cabc_config->i_mode_change = 0;
    }

    _odin_set_cabc_global_gain(0, rgb_gain);
    return back_factor;
}

int odin_gain_cabc_quality(unsigned int backlight_value)
{
    unsigned int clipping_th= 0;
    unsigned int hist_tot_cnt = 0;
    unsigned int clip_vals    = 0;
    unsigned int r_hist       = 0;
	unsigned int cdf_hist, cdf_hist_p = 0;
    unsigned int map_hist     = 0;
    unsigned int hist_eq[65]  = {0};
    unsigned int hist_eq_cdf, hist_eq_cdf_p = 0;

    unsigned int hist_step    = 0;
    unsigned int rgb_gain     = 0;
    unsigned int back_factor  = 0;

    unsigned int i,j,a,b,c,d,x_in = 0;
    u32 reg_val1, reg_val2;

    cabc_config->gain_lut_en = 1;

    /******************************/
    /* CABC Algorithm*/
    /******************************/

    clipping_val[0][0] = clipping_val[0][1];
    hist_tot_cnt = cabc_config->size.w * cabc_config->size.h;
    clipping_th = hist_tot_cnt * cabc_config->i_cdf_thr / 100;

    if (cabc_config->hist_step == CABC_HISTSTEP_64)
        hist_step = 64;
    else
        hist_step = 32;

    if (frm_cnt > 1)
    {
        for (i=1; i<hist_step; i++)
        {
			r_hist = get_r_hist(hist_step, i);

			cdf_hist = r_hist + cdf_hist_p;
			cdf_hist_p = cdf_hist;
			map_hist = get_map_hist(hist_step,
						hist_tot_cnt, cdf_hist);

            hist_eq[map_hist] = r_hist + hist_eq[map_hist];
        }

        for (j=1; j<hist_step+1; j++)
        {
            if (j == 1)
                hist_eq_cdf = hist_eq[1];
            else
                hist_eq_cdf = hist_eq[j] + hist_eq_cdf_p;

			hist_eq_cdf_p = hist_eq_cdf;

            if (hist_eq_cdf > clipping_th)
                clip_vals = j + cabc_config->i_ctrl_clip_range;
            else
                clip_vals = 0;

			if (clip_vals != 0)
			{
				clipping_val[0][1] = clip_vals;
				break;
			}
        }

        if (frm_cnt == 2)
        {
            back_factor
                = 1 * backlight_value;
            rgb_gain
                = 1 * (0x400>>cabc_config->range);
        }
        else
        {
            back_factor
                = odin_gamma_lut(clipping_val[0][1],
                        cabc_config->hist_step,
                        cabc_config->i_gamma);
            back_factor = (back_factor * backlight_value) / 0xff;
            if (clipping_val[0][0] == 0) clipping_val[0][0]=1;
            rgb_gain = ((0x400>>cabc_config->range) * hist_step) /
            		 clipping_val[0][0];

            if (rgb_gain >= 0xFFF)
                rgb_gain = 0xFFF;
        }
    }
    else
    {
        back_factor
            = 1 * backlight_value;
        rgb_gain
            = 1 * (0x400>>cabc_config->range);
    }

    if (clipping_val[0][0] < 10)
    {
        back_factor = 1 * backlight_value;
        rgb_gain = 1 * (0x400>>cabc_config->range);
    }

    cabc_config->o_back_factor = back_factor;
    cabc_config->o_rgb_gain = rgb_gain;

    if (cabc_config->gain_lut_en == 1)
    {
        /*
           y = ax, y = bx+c
           a = 1/gu,  b = (0xFF-i_ctrl_saturation)/(0xFF-0),
           c = i_ctrl_saturation

           b = b * 100.
           cross point : (x_in, y_in)
           x_in = c /(a-b)
           x_in 이후의 기울기 : a' = (b*x + c)/x
           */
        c = cabc_config->o_back_factor + cabc_config->i_ctrl_saturation;
        a = cabc_config->o_rgb_gain * 100 / (0x400>>cabc_config->range);
        b = (0xFF - c) * 100 / 0xFF;

        if ((a-b) == 0)
        {
            d = 1;
        }
        else
            d = (a-b);

        x_in = c * 100 / d;
        if (x_in == 0) x_in = 2;

        for (i=0; i<x_in; i++)
        {
            if (i%2 == 0)
                cabc_config->gain_lut[i/2] = cabc_config->o_rgb_gain;
            else
                cabc_config->gain_lut[i/2] |= (cabc_config->o_rgb_gain<<16);
        }

        for (i=x_in; i< 256; i++)
        {
            if (i == 0)
            {
#if 1
                cabc_config->gain_lut[i/2] = cabc_config->o_rgb_gain;
#else
                if (i%2 == 0)
                    cabc_config->gain_lut[i/2] = cabc_config->o_rgb_gain;
                else
                    cabc_config->gain_lut[i/2] |= (cabc_config->o_rgb_gain<<16);
#endif
            }
            else
            {
                if (i%2 == 0)
                    cabc_config->gain_lut[i/2]
                        = (((b * i / 100) + c)*(0x400>>cabc_config->range) / i);
                else
                    cabc_config->gain_lut[i/2]
                        |= (((b * i / 100) + c)*
                             (0x400>>cabc_config->range) / i) << 16;
            }
        }

        cabc_config->o_rgb_gain = 0x400>>cabc_config->range;
    }
    else
    {
        for (i=0; i<128; i++)
        {
            cabc_config->gain_lut[i]
                = (cabc_config->o_rgb_gain<<16) | cabc_config->o_rgb_gain;
        }
    }

    rgb_gain = cabc_config->o_rgb_gain;

    if (frm_cnt != 0xffffffff)
        frm_cnt++;
    else
        frm_cnt = 4;

    if (cabc_config->i_mode_change)
    {
        frm_cnt = 0;
        clipping_val[0][0] = 0;
        clipping_val[0][1] = 0;
        cabc_config->i_mode_change = 0;
    }

    _odin_set_cabc_global_gain(0, rgb_gain);
    _odin_cabc_disable(ODIN_DSS_CHANNEL_LCD0);
    _odin_set_cabc_gain_lut_wen(ODIN_DSS_CHANNEL_LCD0);
    _odin_set_cabc_gain_lut_write(ODIN_DSS_CHANNEL_LCD0);

    if(odin_cabc_log2) {
        pr_info("%s: gain_lut bl_factor: %x ,frm_cnt=%d \n",__func__, back_factor,frm_cnt);
        for(i=0 ; i < 128 ; i++) {
            pr_info("%x \n", cabc_config->gain_lut[i] & 0xff );
            pr_info("%x \n", cabc_config->gain_lut[i] >> 16 );
        }
        reg_val1 = cabc_read_reg(ADDR_CABC1_SIZE_CTRL);
        reg_val2 = cabc_read_reg(ADDR_CABC1_TOP_CTRL);
        pr_info("%s: reg_val1 = %x, reg_val2 = %x \n",__func__, reg_val1, reg_val2);
    }

    return back_factor;
}

void odin_set_cabc(int cabc_ch, CABC_CONFIG *cabc_config)
{
    _odin_set_cabc_size(cabc_ch, cabc_config->size.w, cabc_config->size.h);
    _odin_set_cabc_hist_lut_step(cabc_ch, cabc_config->hist_step);
    _odin_set_cabc_hist_lut_mode(cabc_ch, cabc_config->hist_mode);
    _odin_set_cabc_gain_lut_range(cabc_ch, cabc_config->range);
    _odin_set_cabc_global_gain(cabc_ch, cabc_config->range);
}
static int i_cdf_thr = 95;
static int i_ctrl_saturation = 0;


int odin_start_cabc(int cabc_ch)
{
    int result = 0;
    cabc_config->i_mode_change = 1;
    cabc_config->size.w = ODIN_CABC_MAX_WIDTH;
    cabc_config->size.h = ODIN_CABC_MAX_HEIGHT;
    cabc_config->hist_step = CABC_HISTSTEP_64;
    cabc_config->hist_mode = CABC_HISTLUT_AUTOMODE;
    cabc_config->range = CABC_GAIN_1024;
    cabc_config->gain_mode = CABC_GAINLUT_DMAMODE;
    cabc_config->en = true;
    cabc_config->i_cdf_thr = i_cdf_thr; /* TUNNING1 : ~ 80 ~ 100  sat low, bl high */
	cabc_config->i_ctrl_saturation = i_ctrl_saturation; /* TUNNING2:  - : sat low, rgb gain high  or +: sat high, rg val low */
    cabc_config->i_gamma = CABC_GAMMA_22;
    cabc_config->i_ctrl_clip_range = 0;
    odin_set_cabc(cabc_ch, cabc_config);
    return result;
}

static int odin_cabc_open(int cabc_ch)
{
    void *ul_kva_hist, *ul_kva_gain;
	unsigned long ul_iova_gain, ul_iova_hist;
	int ret = 0;
    DSSINFO("odin_cabc_open \n");
    odin_crg_dss_clk_ena(CRG_CORE_CLK,
            (GSCL_CLK | CABC_CLK | OVERLAY_CLK), true);
    client = odin_ion_client_create( "cabc" );

	if (IS_ERR_OR_NULL(client)) {
		pr_err("odin_ion_client_create failed. errno:%d\n", (int)client);
		ret = -ENOMEM;
		goto err_odin_ion_client_create;
	}

    alloc_data_histlut.handle = ion_alloc(client, CABC_SIZE_HIST_LUT, 0x1000,
            (1<<ODIN_ION_SYSTEM_HEAP1), 0,
            ODIN_SUBSYS_DSS );
	if (IS_ERR_OR_NULL(alloc_data_histlut.handle))
	{
		pr_err("ion_alloc failed. errno:%d size:%d\n", (int)alloc_data_histlut.handle, CABC_SIZE_HIST_LUT);
		goto err_ion_alloc1;
	}
    alloc_data_gainlut.handle = ion_alloc(client, CABC_SIZE_GAIN_LUT, 0x1000,
            (1<<ODIN_ION_SYSTEM_HEAP1), 0,
            ODIN_SUBSYS_DSS );
	if (IS_ERR_OR_NULL(alloc_data_gainlut.handle))
	{
		pr_err("ion_alloc failed. errno:%d size:%d\n", (int)alloc_data_gainlut.handle, CABC_SIZE_GAIN_LUT);
		goto err_ion_alloc2;
	}

    if ( IS_ERR(alloc_data_histlut.handle) ){
        /* add your error handling code */
        DSSERR("ion alloc error~~!!!!\n");
    }
    ul_iova_hist = odin_ion_get_iova_of_buffer
        (alloc_data_histlut.handle, ODIN_SUBSYS_DSS);
	ul_kva_hist = ion_map_kernel( client, alloc_data_histlut.handle );

    if ( IS_ERR(alloc_data_gainlut.handle) ){
        /* add your error handling code */
        DSSERR("ion alloc error~~!!!!\n");
    }
    ul_iova_gain = odin_ion_get_iova_of_buffer
        (alloc_data_gainlut.handle, ODIN_SUBSYS_DSS);
	ul_kva_gain = ion_map_kernel( client, alloc_data_gainlut.handle );

    _odin_set_cabc_gain_lut_baseaddr(cabc_ch, ul_iova_gain);
    _odin_set_cabc_hist_lut_baseaddr(cabc_ch, ul_iova_hist);
    _odin_set_cabc_interruptmask(ODIN_DSS_CHANNEL_LCD0, CABC_INTRMASK_DISABLE);
    cabc_config = kzalloc(sizeof(CABC_CONFIG), GFP_KERNEL);
	if(!cabc_config) {
		ret = -ENOMEM;
		goto err_alloc;
	}
    cabc_config->hist_lut = ul_kva_hist;
    cabc_config->gain_lut = ul_kva_gain;
    odin_start_cabc(cabc_ch);

	return 0;

err_ion_alloc2:
	ion_free(client, alloc_data_histlut.handle);
err_ion_alloc1:
    odin_ion_client_destroy(client);
err_odin_ion_client_create:
	client = NULL;
	alloc_data_histlut.handle = NULL;
	alloc_data_gainlut.handle = NULL;

err_alloc:

	return ret;
}

static int odin_cabc_close(int cabc_ch)
{
    int result = 0;
    DSSINFO("odin_cabc_close \n");
    _odin_set_cabc_global_gain(cabc_ch, 0x400);
    odin_cabc_enable(false);
    ion_unmap_kernel(client, alloc_data_histlut.handle);
    ion_unmap_kernel(client, alloc_data_gainlut.handle);
    odin_ion_client_destroy(client);
    kfree(cabc_config);
    _odin_set_cabc_hist_lut_mode(ODIN_DSS_CHANNEL_LCD0, 2);
    odin_crg_dss_clk_ena(CRG_CORE_CLK, CABC_CLK, false);

    return result;
}

static int odin_cabc_gain_ctrl(int cabc_ch,
                                        unsigned int backlight_value)
{
    unsigned int back_factor = 0;
    back_factor = odin_gain_cabc_quality(backlight_value);
    /*back_factor = odin_gain_cabc(backlight_value);*/

	return back_factor;
}

static int odin_cabc_bl_ctrl(enum cabc_opr cmd, void *data, void *data2)
{
	int duty_ns;
	int ret=0, r, cur_bl;

	switch(cmd) {
	case CABC_OPEN:

		break;

	case CABC_GAIN_CTRL:
		  r = *(int *)data;
		  cur_bl = *(int *)data2;
		duty_ns = (cabc_pwm_dev->period * r )/cur_bl;
		if(r) {
			pwm_config(cabc_pwm_dev, duty_ns, cabc_pwm_dev->period);
			pwm_enable(cabc_pwm_dev);
		} else {
			pwm_config(cabc_pwm_dev, 0, cabc_pwm_dev->period);
			pwm_disable(cabc_pwm_dev);
		}

		break;

	case CABC_CLOSE:
		pwm_disable(cabc_pwm_dev);
		break;

	}
	return ret;
}


int odin_cabc_cmd(int ch, enum cabc_opr cmd)
{
	static unsigned int bl_val_before_cabc =0;
	int ret=0, r;

	bl_val_before_cabc = bl_get_brightness();

	switch(cmd) {
	case CABC_OPEN:
		ret = odin_cabc_open(ch);
		if(odin_cabc_log)
			pr_info("%s: CABC_OPEN : bl_val_before_cabc = %d", __func__, bl_val_before_cabc);
		break;

	case CABC_GAIN_CTRL:
		r = odin_cabc_gain_ctrl(ch, bl_val_before_cabc);
		odin_cabc_bl_ctrl(CABC_GAIN_CTRL, &r, &bl_val_before_cabc);
		if(odin_cabc_log)
			pr_info("%s: CABC_GAIN_CTRL: back_factor= %d, bl_val_before_cabc=%d  \n",__func__, r, bl_val_before_cabc);
		break;

	case CABC_CLOSE:
		odin_cabc_close(ch);
		odin_cabc_bl_ctrl(CABC_CLOSE, NULL, NULL);
		if(odin_cabc_log)
			pr_info("%s: CABC_CLOSE : bl_val_before_cabc = %d", __func__, bl_val_before_cabc);
		break;
	}

	return ret;
}
#if 0
static void odin_cabc_ctrl(int cabc_ch, CABC_TESTFSM stat)
{
    u32 val, val2, val3, i;
    unsigned int gain_lut;
    unsigned int hist_lut;
    int index, resultd, x;
    int totpixelcnt = 0;
    unsigned int lutsize = 32;

    struct ion_client *client = odin_ion_client_create("cabc_ion");
	struct ion_handle *handle;
	unsigned long ul_iova;
	void *ul_kva;

    handle = ion_alloc(client, CABC_SIZE_GAIN_LUT, 0x1000,
    					(1<<ODIN_ION_SYSTEM_HEAP1), 0,
						ODIN_SUBSYS_DSS );

	if ( IS_ERR(handle) ){
		/* add your error handling code */
	}
    ul_iova = odin_ion_get_iova_of_buffer(handle, ODIN_SUBSYS_DSS);
    DSSDBG("ul_iova : 0x%x\n", ul_iova);

	/* when you use this function, call unmap too */
	ul_kva = ion_map_kernel(client, handle);
    DSSDBG("ul_kva : 0x%x\n", ul_kva);

    gain_lut = ul_iova;
    /* setting lut base address */
    _odin_set_cabc_gain_lut_baseaddr(cabc_ch, gain_lut);
    _odin_set_cabc_hist_lut_baseaddr(cabc_ch, ul_iova);

    gain_lut = (unsigned int)ul_kva;

    cabc_config = kzalloc(sizeof(CABC_CONFIG), GFP_KERNEL);

    /*intterupt control~~!*/

    switch (stat)
    {
    case READ_HIST_LUT_DMA_INT:
        break;
    case READ_HIST_LUT_DMA_START:
        odin_get_cabc_hist_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_HISTLUT_DMAMODE, CABC_HISTSTEP_64);
        odin_get_cabc_hist_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_HISTLUT_DMAMODE, CABC_HISTSTEP_32);
        break;
    case READ_HIST_LUT_APB:
        odin_get_cabc_hist_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_HISTLUT_APBMODE, CABC_HISTSTEP_64);
        break;

    case WRITE_GAIN_LUT_DMA:
        odin_set_cabc_gain_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_GAINLUT_DMAMODE, CABC_GAIN_1024);
        odin_set_cabc_gain_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_GAINLUT_DMAMODE, CABC_GAIN_512);
        odin_set_cabc_gain_lut(cabc_ch, (unsigned int *)gain_lut,
            CABC_GAINLUT_DMAMODE, CABC_GAIN_256);
        break;
    case WRITE_GAIN_LUT_APB:
        odin_set_cabc_gain_lut(cabc_ch, gain_lut,
            CABC_GAINLUT_APBMODE, CABC_GAIN_1024);
        odin_set_cabc_gain_lut(cabc_ch, gain_lut,
            CABC_GAINLUT_APBMODE, CABC_GAIN_512);
        odin_set_cabc_gain_lut(cabc_ch, gain_lut,
            CABC_GAINLUT_APBMODE, CABC_GAIN_256);
        break;
    case CHECK_GLOBAL_GAIN_VAL:
        odin_check_cabc_global_gain_val(cabc_ch);
        break;
    case CSC_CTRL:
        odin_check_rgb_coef_csc_ctrl(cabc_ch, gain_lut);
        break;
    case CHECK_HIST_LUT_STEP:
        odin_check_hist_lut_step(cabc_ch, gain_lut, totpixelcnt = 10);
        break;
    case CHECK_APL:
        DSSDBG("APL Value : Check to Hist Lut Step Case %d\n",
            CHECK_HIST_LUT_STEP);
        break;
    case CHECK_MIN_MAX_VAL:
        DSSDBG("Max Y Val : %d\n",
            _odin_get_cabc_minmax_yvalue(cabc_ch, CABC_YVALUE_MAX));
        DSSDBG("Min Y Val : %d\n",
            _odin_get_cabc_minmax_yvalue(cabc_ch, CABC_YVALUE_MIN));
        break;
    case CHECK_TOT_PIXEL_CNT:
        DSSDBG("total pixel counter = %d\n ",
            _odin_get_cabc_totalpixelcnt(cabc_ch));
        if (totpixelcnt != _odin_get_cabc_totalpixelcnt(cabc_ch))
            DSSERR("error. Check total pixel counter.\n");
        else
            DSSDBG("pass. Check total pixel counter.\n");
        break;
    case CHECK_HIST_TOP5:
         for (x = 0; x < CABC_HIST_TOPMAX; x++)
        {
            DSSDBG("Hist Lut Cnt Top%d Val : %d\n", x,
                _odin_get_cabc_hist_top5(cabc_ch, x));
        }

        DSSDBG("Hist Lut Cnt Top5 using S/W function\n");
        break;
    }

    odin_ion_client_destroy(client);
    _odin_set_cabc_hist_lut_mode(ODIN_DSS_CHANNEL_LCD0, 2);
}
void odin_cabc_initialize(int cabc_ch, CABC_INFO *pcabc)
{

	_odin_set_cabc_size(cabc_ch, ODIN_CABC_MAX_WIDTH,
        ODIN_CABC_MAX_HEIGHT);
	_odin_set_cabc_csc_coef(cabc_ch, &pcabc->coef.r_coef,
				&pcabc->coef.g_coef, &pcabc->coef.b_coef);
	_odin_set_cabc_top_ctrl
        (cabc_ch, &pcabc->hist_step, &pcabc->global_gain);
	DSSDBG("cabc init~!!\n");
}
static ssize_t show_cabc_m_onoff( struct device *dev,
				  struct device_attribute *attr,
				  char *buf )
{

	if ( odin_cabc_onoff == false )
		pr_info( "cabc disable~!\n"   );
	else if ( odin_cabc_onoff == true )
		pr_info( "cabc enable~!\n"   );

	return 0;

}

static ssize_t store_cabc_m_onoff( struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count )
{
    CABC_INFO info;
    CSC_COEF coef;

    if ( (*(buf+0)=='o') && (*(buf+1)=='n') )
    {
        odin_cabc_onoff = true;
        coef.r_coef = 0x300;
        coef.g_coef = 0x300;
        coef.b_coef = 0x70;
        info.channel = ODIN_DSS_CHANNEL_LCD0;
        info.coef = coef;
        info.global_gain = 0x150;
        info.height = ODIN_CABC_MAX_HEIGHT;
        info.width = ODIN_CABC_MAX_WIDTH;
        info.hist_lut_addr = NULL;
        info.gain_lut_addr = NULL;
        info.hist_step = CABC_HISTSTEP_64;

        _odin_cabc_enable(ODIN_DSS_CHANNEL_LCD0);
        odin_cabc_initialize(ODIN_DSS_CHANNEL_LCD0, &info);
        pr_info( "cabc turn ON\n" );
    }

    else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') )
    {
        odin_cabc_onoff = false;
        _odin_cabc_disable(ODIN_DSS_CHANNEL_LCD0);
        pr_info( "cabc OFF\n" );
    }
    else if (*(buf+0)=='1')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 11);
    }
    else if (*(buf+0)=='2')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 12);
    }
    else if (*(buf+0)=='3')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 3);
    }
    else if (*(buf+0)=='4')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 14);
    }
    else if (*(buf+0)=='5')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 5);
    }
    else if (*(buf+0)=='6')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 6);
    }
    else if (*(buf+0)=='7')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 7);
    }
    else if (*(buf+0)=='8')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 8);
    }
    else if (*(buf+0)=='9')
    {
        odin_cabc_ctrl(ODIN_DSS_CHANNEL_LCD0, 9);
    }
    else
    {
        pr_info( "what?" );
    }

    return count;

}

static DEVICE_ATTR( cabc_m_onoff, 0666,
		    show_cabc_m_onoff,
		    store_cabc_m_onoff );

#endif
void cabc_free_memory_region(void)
{
    int size;
    int i;

    for (i = 0; i < ODIN_CABC_MAX_RES; i++)
    {
        if (cabc_dev.base[i])
        {
            iounmap(cabc_dev.base[i]);
            cabc_dev.base[i] = NULL;
        }
        if (cabc_dev.iomem[i])
        {
            size = cabc_dev.iomem[i]->end -  cabc_dev.iomem[i]->start + 1;
            release_mem_region( cabc_dev.iomem[i]->start, size);
            cabc_dev.iomem[i] = NULL;
        }
    }
}

static ssize_t show_cabc_onoff( struct device *dev,
				struct device_attribute *attr,
				char *buf )
{
	if ( odin_cabc_onoff == false )
		pr_info( "cabc disable~!\n"   );
	else if ( odin_cabc_onoff == true )
		pr_info( "cabc enable~!\n"   );

	return 0;
}

static ssize_t store_cabc_onoff( struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count )
{
    if ( (*(buf+0)=='o') && (*(buf+1)=='n') )
    {
        odin_cabc_onoff = true;
        pr_info( "cabc turn ON\n" );
    }

    else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') )
    {
        odin_cabc_onoff = false;
        pr_info( "cabc OFF\n" );
    }
    else
    {
        pr_info( "what?" );
    }

    return count;

}

static DEVICE_ATTR( cabc_onoff, 0644,
		    show_cabc_onoff,
		    store_cabc_onoff );

static ssize_t show_cabc_log2( struct device *dev,
			       struct device_attribute *attr,
			       char *buf )
{
	if ( odin_cabc_log2 == false )
		pr_info( "cabc log2 off~!\n"   );
	else if ( odin_cabc_log2 == true )
		pr_info( "cabc log2 on~!\n"   );

	return 0;
}

static ssize_t store_cabc_log2( struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count )
{
    if ( (*(buf+0)=='o') && (*(buf+1)=='n') )
    {
        odin_cabc_log2 = true;
        pr_info( "cabc log2 turn ON\n" );
    }

    else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') )
    {
        odin_cabc_log2 = false;
        pr_info( "cabc log2 turn OFF\n" );
    }
    else
    {
        pr_info( "what?" );
    }

    return count;

}

static DEVICE_ATTR( cabc_log2, 0664,
		    show_cabc_log2,
		    store_cabc_log2 );


static ssize_t show_cabc_log( struct device *dev,
			      struct device_attribute *attr,
			      char *buf )
{
	if ( odin_cabc_log == false )
		pr_info( "cabc log off~!\n"   );
	else if ( odin_cabc_log == true )
		pr_info( "cabc log on~!\n"   );

	return 0;
}

static ssize_t store_cabc_log( struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count )
{
    if ( (*(buf+0)=='o') && (*(buf+1)=='n') )
    {
        odin_cabc_log = true;
        pr_info( "cabc log turn ON\n" );
    }

    else if ( (*buf=='o') && (*(buf+1)=='f') && (*(buf+2)=='f') )
    {
        odin_cabc_log = false;
        pr_info( "cabc log turn OFF\n" );
    }
    else
    {
        pr_info( "what?" );
    }

    return count;

}

static DEVICE_ATTR( cabc_log, 0664,
		    show_cabc_log,
		    store_cabc_log );

static ssize_t show_cabc_tun1( struct device *dev,
			struct device_attribute *attr, char *buf )
{
	pr_info( "%s: i_cdf_thr = %d  \n",__func__, i_cdf_thr);
	return 0;
}

static ssize_t store_cabc_tun1( struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count )
{	char *endp;
    unsigned int cdf;
	cdf = simple_strtoul(buf, &endp, 10);
	if(isspace(*endp))
		endp++;
	if(endp - buf != count)
		return -EINVAL;
	i_cdf_thr = cdf;
	pr_info( "%s: i_cdf_thr = %d  \n",__func__, i_cdf_thr);
	return count;
}

static DEVICE_ATTR( cabc_tun1, 0664, show_cabc_tun1, store_cabc_tun1 );

static ssize_t show_cabc_tun2( struct device *dev,
			struct device_attribute *attr, char *buf )
{
	pr_info( "%s: i_ctrl_saturation = %d  \n",__func__, i_ctrl_saturation);
	return 0;
}

static ssize_t store_cabc_tun2( struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count )
{
	char *endp;
	int sat;
	sat = simple_strtol(buf, &endp, 10);	if(isspace(*endp))
		endp++;
	if(endp - buf != count)
		return -EINVAL;
	i_ctrl_saturation = sat;
	pr_info( "%s: i_ctrl_saturation = %d  \n",__func__, i_ctrl_saturation);
	return count;
}

static DEVICE_ATTR( cabc_tun2, 0664, show_cabc_tun2, store_cabc_tun2 );

bool odin_cabc_is_on(void)
{
	bool ret;

	if(odin_cabc_onoff)
		ret = true;
	else
		ret = false;

	return ret;
}
int odin_cabc_device_create_files(struct device *dev)
{
	int ret;

	ret = device_create_file( dev, &dev_attr_cabc_onoff );
	if ( ret != 0 )
		goto err;

	ret = device_create_file( dev, &dev_attr_cabc_log );
	if ( ret != 0 )
		goto err;

	ret = device_create_file( dev, &dev_attr_cabc_log2 );
	if ( ret != 0 )
		goto err;

	ret = device_create_file( dev, &dev_attr_cabc_tun1 );
	if ( ret != 0 )
		goto err;

	ret = device_create_file( dev, &dev_attr_cabc_tun2 );

err:
	return ret;
}
#ifdef CONFIG_OF
extern struct device odin_device_parent;
#endif
int odin_cabc_probe(struct platform_device *pdev)
{
    int ret = 0;
    int i;

    /* sysfs */
    struct device *dev;

    DSSINFO("odin_cabchw_probe.\n");

#ifdef CONFIG_OF
    pdev->dev.parent = &odin_device_parent;

#endif
    for (i = 0; i < ODIN_CABC_MAX_RES; i++)
    {

#ifdef CONFIG_OF
        cabc_dev.base[i] = of_iomap(pdev->dev.of_node, i);
        if (cabc_dev.base[i]  == 0)
        {
            DSSERR("can't ioremap CABC\n");
            ret = -ENOMEM;
            goto free_region;
        }


#else
        cabc_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
        if (!cabc_mem)
        {
            DSSERR("can't get IORESOURCE_MEM CABC\n");
            ret = -EINVAL;
            goto free_region;
        }

        size = cabc_mem->end - cabc_mem->start;
        cabc_dev.iomem[i] = request_mem_region(cabc_mem->start,
        		size, pdev->name);
        if (cabc_dev.iomem[i] == NULL)
        {
            DSSERR("failed to request memory region\n");
            ret = -ENOENT;
            goto free_region;
        }
        cabc_dev.base[i] = ioremap(cabc_mem->start, size);
        if (cabc_dev.base[i]  == 0)
        {
            DSSERR("can't ioremap CABC\n");
            ret = -ENOMEM;
            goto free_region;
        }

#endif
    }

    _odin_set_cabc_hist_lut_mode(ODIN_DSS_CHANNEL_LCD0, 2);

	cabc_pwm_dev = pwm_get(&pdev->dev, "odin-pwm");

    platform_set_drvdata(pdev, &cabc_dev);

    /* sysfs */
	dev = cabc_device_register(&pdev->dev,"ap_cabc");
    if ( IS_ERR( dev ) )
    {
        return PTR_ERR( dev );
    }

	ret = odin_cabc_device_create_files(dev);
	if(ret != 0)
		dev_err(&pdev->dev, "Error creating sysfs files \n");

    return 0;

free_region:
	cabc_free_memory_region();
	return ret;
}

static int odin_cabc_remove(struct platform_device *pdev)
{
	cabc_device_unregister(&pdev->dev);
	return 0;
}
#ifdef CONFIG_OF
static struct of_device_id odindss_cabc_match[] = {
	{
		.compatible = "odindss-cabc",
	},
	{},
};
#endif

static struct platform_driver odin_cabc_driver = {
	.probe          = odin_cabc_probe,
	.remove         = odin_cabc_remove,
	.driver         = {
	.name   = DRV_NAME,
	.owner  = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = of_match_ptr(odindss_cabc_match),
#endif
	},
};

int odin_cabc_init_platform_driver(void)
{
	return platform_driver_register(&odin_cabc_driver);
}

void odin_cabc_uninit_platform_driver(void)
{
	return platform_driver_unregister(&odin_cabc_driver);
}
