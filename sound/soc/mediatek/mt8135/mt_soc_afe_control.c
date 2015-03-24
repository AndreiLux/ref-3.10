/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "AudDrv_Common.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_afe_connection.h"
#include "mt_soc_hdmi_def.h"
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <mach/mt_gpio_def.h>
#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
#include <mach/mtk_wcn_cmb_stub.h>
#endif

#define CONFIG_MTK_DEEP_IDLE
#ifdef CONFIG_MTK_DEEP_IDLE
#include <mach/mt_clkmgr.h>
#include <mach/mt_idle.h>
#endif

static DEFINE_SPINLOCK(afe_control_lock);

/*
 *    global variable control
 */
/* address for ioremap audio hardware register */
void *AFE_BASE_ADDRESS;
void *AFE_SRAM_ADDRESS;

/* static  variable */
static struct AudioMrgIf *mAudioMrg;
static struct AudioDigitalDAIBT *AudioDaiBt;
static struct AudioDigtalI2S *AudioAdcI2S;
static const bool AudioAdcI2SStatus;
static struct AudioDigtalI2S *m2ndI2S;

static struct AudioIrqMcuMode *mAudioMcuMode[Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE] = { NULL };
static struct AudioMemIFAttribute *mAudioMEMIF[Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK] = { NULL };

static bool mAudioInit;
static bool mAudioAfeBaseAddressInit;

static struct AFE_MEM_CONTROL_T AFE_dL1_Control_context;
struct AFE_MEM_CONTROL_T AFE_VUL_Control_context;
struct AFE_MEM_CONTROL_T AFE_DAI_Control_context;
struct AFE_MEM_CONTROL_T AFE_AWB_Control_context;

static struct snd_pcm_substream *pSubSteam[Soc_Aud_Digital_Block_MEM_MOD_DAI + 1] = { NULL };

struct AudAfe_Suspend_Reg Suspend_reg;
static bool AudDrvSuspendStatus;
static struct mt_audio_custom_gpio_data audio_gpio_data;

/* mutex lock */
static DEFINE_MUTEX(afe_control_mutex);
/* static DEFINE_SPINLOCK(auddrv_irqstatus_lock); */

static const uint16_t kSideToneCoefficientTable16k[] = {
	0x049C, 0x09E8, 0x09E0, 0x089C,
	0xFF54, 0xF488, 0xEAFC, 0xEBAC,
	0xfA40, 0x17AC, 0x3D1C, 0x6028,
	0x7538
};

static const uint16_t kSideToneCoefficientTable32k[] = {
	0xff58, 0x0063, 0x0086, 0x00bf,
	0x0100, 0x013d, 0x0169, 0x0178,
	0x0160, 0x011c, 0x00aa, 0x0011,
	0xff5d, 0xfea1, 0xfdf6, 0xfd75,
	0xfd39, 0xfd5a, 0xfde8, 0xfeea,
	0x005f, 0x0237, 0x0458, 0x069f,
	0x08e2, 0x0af7, 0x0cb2, 0x0df0,
	0x0e96
};

/*
 *    function implementation
 */
static irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id);
static void Auddrv_DL_Interrupt_Handler(void);
static void Auddrv_UL_Interrupt_Handler(void);	/* irq2 ISR handler */
static void Auddrv_Handle_Mem_context(struct AFE_MEM_CONTROL_T *Mem_Block);

/*****************************************************************************
 * FUNCTION
 *  InitAfeControl ,ResetAfeControl
 *
 * DESCRIPTION
 *  afe init function
 *
 *****************************************************************************
 */

bool InitAfeControl(void)
{
	int i = 0;
	pr_info("InitAfeControl\n");
	/* first time to init , reg init. */
	Auddrv_Reg_map();
	mutex_lock(&afe_control_mutex);
	if (!mAudioInit) {
		mAudioInit = true;
		mAudioMrg = kzalloc(sizeof(struct AudioMrgIf), GFP_KERNEL);
		AudioDaiBt = kzalloc(sizeof(struct AudioDigitalDAIBT), GFP_KERNEL);
		AudioAdcI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);
		m2ndI2S = kzalloc(sizeof(struct AudioDigtalI2S), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++)
			mAudioMcuMode[i] = kzalloc(sizeof(struct AudioIrqMcuMode), GFP_KERNEL);

		for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
			mAudioMEMIF[i] = kzalloc(sizeof(struct AudioMemIFAttribute), GFP_KERNEL);

		AFE_dL1_Control_context.MemIfNum = Soc_Aud_Digital_Block_MEM_DL1;
		AFE_VUL_Control_context.MemIfNum = Soc_Aud_Digital_Block_MEM_VUL;
		AFE_DAI_Control_context.MemIfNum = Soc_Aud_Digital_Block_MEM_DAI;
		AFE_AWB_Control_context.MemIfNum = Soc_Aud_Digital_Block_MEM_AWB;
	}
	mutex_unlock(&afe_control_mutex);
	return true;
}

bool ResetAfeControl(void)
{
	int i;
	pr_info("ResetAfeControl\n");

	mutex_lock(&afe_control_mutex);
	mAudioInit = false;
	memset((void *)(mAudioMrg), 0, sizeof(struct AudioMrgIf));
	memset((void *)(AudioDaiBt), 0, sizeof(struct AudioDigitalDAIBT));

	for (i = 0; i < Soc_Aud_IRQ_MCU_MODE_NUM_OF_IRQ_MODE; i++)
		memset((void *)(mAudioMcuMode[i]), 0, sizeof(struct AudioIrqMcuMode));

	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++)
		memset((void *)(mAudioMEMIF[i]), 0, sizeof(struct AudioMemIFAttribute));

	mutex_unlock(&afe_control_mutex);
	return true;
}

/*****************************************************************************
 * FUNCTION
 *  Register_aud_irq
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */

int Register_Aud_Irq(void *dev)
{
	const int ret = request_irq(MT8135_AFE_MCU_IRQ_LINE, AudDrv_IRQ_handler,
			      IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
	if (unlikely(ret < 0))
		pr_err("Register_Aud_Irq %d", ret);

	return ret;
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_IRQ_handler / AudDrv_magic_tasklet
 *
 * DESCRIPTION
 *  IRQ handler
 *
 *****************************************************************************
 */
irqreturn_t AudDrv_IRQ_handler(int irq, void *dev_id)
{
	const uint32_t u4RegValue = (Afe_Get_Reg(AFE_IRQ_STATUS) & IRQ_STATUS_BIT);
	/* pr_info("AudDrvAudDrv_IRQ_handler AFE_IRQ_STATUS = %x\n", u4RegValue); */
	/* here is error handle , for interrupt is trigger but not status , clear all interrupt with bit 6 */

	if (u4RegValue & INTERRUPT_IRQ1_MCU)
		Auddrv_DL_Interrupt_Handler();

	if (u4RegValue & INTERRUPT_IRQ2_MCU)
		Auddrv_UL_Interrupt_Handler();

	if (u4RegValue & INTERRUPT_HDMI_IRQ)
		Auddrv_HDMI_Interrupt_Handler();

	if (unlikely(u4RegValue == 0)) {
		pr_info("%s u4RegValue == 0\n", __func__);
		AudDrv_Clk_On();
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 6, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 1, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 2, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 3, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 4, 0xff);
		Afe_Set_Reg(AFE_IRQ_CLR, 1 << 5, 0xff);

		AudDrv_Clk_Off();
		goto AudDrv_IRQ_handler_exit;
	}

	/* clear irq */
	Afe_Set_Reg(AFE_IRQ_CLR, u4RegValue, 0xff);

AudDrv_IRQ_handler_exit:
	return IRQ_HANDLED;
}

void Auddrv_DL_SetPlatformInfo(enum Soc_Aud_Digital_Block digital_block,
			       struct snd_pcm_substream *substream)
{
	if (likely((int)digital_block < ARRAY_SIZE(pSubSteam)))
		pSubSteam[digital_block] = substream;
	else
		pr_err("%s unexpected digital_block = %d\n", __func__, digital_block);
}

void Auddrv_DL_ResetPlatformInfo(enum Soc_Aud_Digital_Block digital_block)
{
	if (likely((int)digital_block < ARRAY_SIZE(pSubSteam)))
		pSubSteam[digital_block] = NULL;
	else
		pr_err("%s unexpected digital_block = %d\n", __func__, digital_block);
}

void Auddrv_ResetBuffer(enum Soc_Aud_Digital_Block digital_block)
{
	struct AFE_BLOCK_T *Afe_Block;

	switch (digital_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		Afe_Block = &(AFE_dL1_Control_context.rBlock);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL:
		Afe_Block = &(AFE_VUL_Control_context.rBlock);
		break;
	case Soc_Aud_Digital_Block_MEM_DAI:
		Afe_Block = &(AFE_DAI_Control_context.rBlock);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB:
		Afe_Block = &(AFE_AWB_Control_context.rBlock);
		break;
	default:
		AUDIO_ASSERT(true);
		return;
	}

	if (unlikely(!Afe_Block || !Afe_Block->pucVirtBufAddr)) {
		pr_err("%s cannot reset buffer, digital_block= %d\n", __func__, digital_block);
		return;
	}

	memset(Afe_Block->pucVirtBufAddr, 0, Afe_Block->u4BufferSize);
	Afe_Block->u4DMAReadIdx = 0;
	Afe_Block->u4WriteIdx = 0;
	Afe_Block->u4DataRemained = 0;
}


int Auddrv_UpdateHwPtr(enum Soc_Aud_Digital_Block digital_block, struct snd_pcm_substream *substream)
{
	struct AFE_BLOCK_T *Afe_Block;
	int rc = 0;

	switch (digital_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		Afe_Block = &(AFE_dL1_Control_context.rBlock);
		rc = bytes_to_frames(substream->runtime, Afe_Block->u4DMAReadIdx);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL:
		Afe_Block = &(AFE_VUL_Control_context.rBlock);
		rc = bytes_to_frames(substream->runtime, Afe_Block->u4WriteIdx);
		break;
	case Soc_Aud_Digital_Block_MEM_DAI:
		Afe_Block = &(AFE_DAI_Control_context.rBlock);
		rc = bytes_to_frames(substream->runtime, Afe_Block->u4WriteIdx);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB:
		Afe_Block = &(AFE_AWB_Control_context.rBlock);
		rc = bytes_to_frames(substream->runtime, Afe_Block->u4WriteIdx);
		break;
	default:
		AUDIO_ASSERT(true);
		break;
	}

	return rc;
}

/*****************************************************************************
 * FUNCTION
 *  AudDrv_Allocate_DL1_Buffer / AudDrv_Free_DL1_Buffer
 *
 * DESCRIPTION
 *  allocate DL1 Buffer
 *

******************************************************************************/
int AudDrv_Allocate_DL1_Buffer(uint32_t Afe_Buf_Length)
{
#ifdef AUDIO_MEMORY_SRAM
	uint32_t u4PhyAddr = 0;
#endif
	struct AFE_BLOCK_T *pblock = &AFE_dL1_Control_context.rBlock;
	pblock->u4BufferSize = Afe_Buf_Length;

	pr_debug("AudDrv %s length = %d\n", __func__, Afe_Buf_Length);
	if (unlikely(Afe_Buf_Length > AUDDRV_DL1_MAX_BUFFER_LENGTH))
		return -1;

	/* allocate memory */
	if (pblock->pucPhysBufAddr == 0) {
#ifdef AUDIO_MEMORY_SRAM
		/* todo , there should be a sram manager to allocate memory for low  power.powervr_device */
		u4PhyAddr = AFE_INTERNAL_SRAM_PHY_BASE;
		pblock->pucPhysBufAddr = u4PhyAddr;

#ifdef AUDIO_MEM_IOREMAP
		pr_info("AudDrv %s length AUDIO_MEM_IOREMAP  = %d\n", __func__, Afe_Buf_Length);
		pblock->pucVirtBufAddr = (kal_uint8 *) AFE_SRAM_ADDRESS;
#else
		pblock->pucVirtBufAddr = AFE_INTERNAL_SRAM_VIR_BASE;
#endif /* AUDIO_MEM_IOREMAP */

#else /* AUDIO_MEMORY_SRAM */
		pr_info("AudDrv %s use dram\n", __func__);
		pblock->pucVirtBufAddr = dma_alloc_coherent(0, pblock->u4BufferSize,
					&pblock->pucPhysBufAddr, GFP_KERNEL);
#endif
	}

	pr_debug("AudDrv %s pucVirtBufAddr = %p\n", __func__, pblock->pucVirtBufAddr);

	/* check 32 bytes align */
	if (unlikely((pblock->pucPhysBufAddr & 0x1f) != 0))
		pr_warn("[Auddrv] %s is not aligned (0x%x)\n", __func__, pblock->pucPhysBufAddr);

	pblock->u4SampleNumMask = 0x001f;	/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	/* set sram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END, pblock->pucPhysBufAddr + (Afe_Buf_Length - 1), 0xffffffff);

	return 0;
}

static void Auddrv_DL_Interrupt_Handler(void)
{
	/* irq1 ISR handler */
	kal_int32 Afe_consumed_bytes;
	kal_int32 HW_memory_index;
	kal_int32 HW_Cur_ReadIdx = 0;
	struct AFE_BLOCK_T * const Afe_Block = &(AFE_dL1_Control_context.rBlock);

	PRINTK_AUDDEEPDRV("%s\n", __func__);
	HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);

	if (HW_Cur_ReadIdx == 0) {
		pr_notice("%s HW_Cur_ReadIdx ==0\n", __func__);
		HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
	}

	HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

	/*
	   pr_notice("%s HW_Cur_ReadIdx=0x%x HW_memory_index = 0x%x Afe_Block->pucPhysBufAddr = 0x%x\n",
	   __func__, HW_Cur_ReadIdx, HW_memory_index, Afe_Block->pucPhysBufAddr);
	 */

	/* get hw consume bytes */
	if (HW_memory_index > Afe_Block->u4DMAReadIdx) {
		Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
	} else {
		Afe_consumed_bytes =
		    Afe_Block->u4BufferSize + HW_memory_index - Afe_Block->u4DMAReadIdx;
	}

	if (unlikely((Afe_consumed_bytes & 0x03) != 0))
		pr_warn("%s DMA address is not aligned 32 bytes\n", __func__);

	Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
	Afe_Block->u4DMAReadIdx &= Afe_Block->u4BufferSize - 1;

	PRINTK_AUDDEEPDRV("%s before snd_pcm_period_elapsed\n", __func__);
	snd_pcm_period_elapsed(pSubSteam[Soc_Aud_Digital_Block_MEM_DL1]);
	PRINTK_AUDDEEPDRV("%s after snd_pcm_period_elapsed\n", __func__);
}

static void Auddrv_UL_Interrupt_Handler(void)
{
	/* irq2 ISR handler */
	const uint32_t Afe_Dac_Con0 = Afe_Get_Reg(AFE_DAC_CON0);
	struct AFE_MEM_CONTROL_T *Mem_Block = NULL;

	if (Afe_Dac_Con0 & 0x8) {
		/* handle VUL Context */
		Mem_Block = &AFE_VUL_Control_context;
		Auddrv_Handle_Mem_context(Mem_Block);
	}
	if (Afe_Dac_Con0 & 0x10) {
		/* handle DAI Context */
		Mem_Block = &AFE_DAI_Control_context;
		Auddrv_Handle_Mem_context(Mem_Block);
	}
	if (Afe_Dac_Con0 & 0x40) {
		/* handle AWB Context */
		Mem_Block = &AFE_AWB_Control_context;
		Auddrv_Handle_Mem_context(Mem_Block);
	}
}

static bool CheckSize(uint32_t size)
{
	if ((size) == 0) {
		pr_warn(" Mt_soc_afe_contorl CheckSize size = 0\n");
		return true;
	}
	return false;
}

static void Auddrv_Handle_Mem_context(struct AFE_MEM_CONTROL_T *Mem_Block)
{
	uint32_t HW_Cur_ReadIdx = 0;
	kal_int32 Hw_Get_bytes = 0;
	struct AFE_BLOCK_T *mBlock = NULL;

	if (unlikely(!Mem_Block))
		return;

	switch (Mem_Block->MemIfNum) {
	case Soc_Aud_Digital_Block_MEM_VUL:
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_VUL_CUR);
		break;
	case Soc_Aud_Digital_Block_MEM_DAI:
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DAI_CUR);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB:
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_AWB_CUR);
		break;
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_MOD_PCM_CUR);
		break;
	}

	mBlock = &Mem_Block->rBlock;

	if (unlikely(CheckSize(HW_Cur_ReadIdx)))
		return;

	if (unlikely(mBlock->pucVirtBufAddr == NULL))
		return;

	/* HW already fill in */
	Hw_Get_bytes = (HW_Cur_ReadIdx - mBlock->pucPhysBufAddr) - mBlock->u4WriteIdx;

	if (Hw_Get_bytes < 0)
		Hw_Get_bytes += mBlock->u4BufferSize;

	PRINTK_AUDDEEPDRV("%s u4WriteIdx:%x Hw_Get_bytes:%x HW_Cur_ReadIdx:%x\n",
			__func__, mBlock->u4WriteIdx, Hw_Get_bytes, HW_Cur_ReadIdx);

	/*
	   pr_notice("%s Hw_Get_bytes:%x HW_Cur_ReadIdx:%x u4DMAReadIdx:%x u4WriteIdx:0x%x\n",
	   __func__, Hw_Get_bytes, HW_Cur_ReadIdx, mBlock->u4DMAReadIdx, mBlock->u4WriteIdx);
	   pr_notice("%s pucPhysBufAddr:%x MemIfNum = %d pSubSteam[MemIfNum] = %p\n",
	   __func__, mBlock->pucPhysBufAddr, Mem_Block->MemIfNum, pSubSteam[Mem_Block->MemIfNum]); */

	mBlock->u4WriteIdx += Hw_Get_bytes;
	mBlock->u4WriteIdx &= mBlock->u4BufferSize - 1;

	snd_pcm_period_elapsed(pSubSteam[Mem_Block->MemIfNum]);
}

/*****************************************************************************
 * FUNCTION
 *  Auddrv_Reg_map
 *
 * DESCRIPTION
 * Auddrv_Reg_map
 *
 *****************************************************************************
 */

void Auddrv_Reg_map(void)
{
	if (!mAudioAfeBaseAddressInit) {
		AFE_SRAM_ADDRESS = ioremap_nocache(AFE_INTERNAL_SRAM_PHY_BASE, 0x10000);
		AFE_BASE_ADDRESS = ioremap_nocache(AUDIO_HW_PHYSICAL_BASE, 0x10000);
		mAudioAfeBaseAddressInit = true;
		pr_info("AFE_BASE_ADDRESS = %p AFE_SRAM_ADDRESS = %p\n",
			AFE_BASE_ADDRESS, AFE_SRAM_ADDRESS);
	}
}

void InitAudioGpioData(struct mt_audio_custom_gpio_data *gpio_data)
{
	if (gpio_data)
		memcpy(&audio_gpio_data, gpio_data, sizeof(struct mt_audio_custom_gpio_data));
}

static bool CheckMemIfEnable(void)
{
	int i;
	for (i = 0; i < Soc_Aud_Digital_Block_NUM_OF_DIGITAL_BLOCK; i++) {
		if ((mAudioMEMIF[i]->mState) == true)
			return true;
	}
	return false;
}

static bool CheckUpLinkMemIfEnable(void)
{
	int i;
	for (i = Soc_Aud_Digital_Block_MEM_VUL; i < Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE; i++) {
		if ((mAudioMEMIF[i]->mState) == true)
			return true;
	}
	return false;
}

/*****************************************************************************
 * FUNCTION
 *  EnableAfe
 *
 * DESCRIPTION
 * EnableAfe
 *
 *****************************************************************************
 */
void EnableAfe(bool bEnable)
{
	unsigned long flags;
	bool MemEnable;

	spin_lock_irqsave(&afe_control_lock, flags);
	MemEnable = CheckMemIfEnable();

	if (!bEnable && !MemEnable)
		Afe_Set_Reg(AFE_DAC_CON0, 0x0, 0x0);
	else if (bEnable && MemEnable)
		Afe_Set_Reg(AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe_control_lock, flags);
}

static uint32_t SampleRateTransform(uint32_t SampleRate)
{
	switch (SampleRate) {
	case 8000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_8K;
	case 11025:
		return Soc_Aud_I2S_SAMPLERATE_I2S_11K;
	case 12000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_12K;
	case 16000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_16K;
	case 22050:
		return Soc_Aud_I2S_SAMPLERATE_I2S_22K;
	case 24000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_24K;
	case 32000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_32K;
	case 44100:
		return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
	case 48000:
		return Soc_Aud_I2S_SAMPLERATE_I2S_48K;
	default:
		break;
	}
	return Soc_Aud_I2S_SAMPLERATE_I2S_44K;
}

bool SetSampleRate(uint32_t Aud_block, uint32_t SampleRate)
{
	pr_debug("%s Aud_block = %d Aud_block = %d\n", __func__, Aud_block, Aud_block);
	SampleRate = SampleRateTransform(SampleRate);

	switch (Aud_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate, 0x0000000f);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DL2:
		{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 4, 0x000000f0);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_I2S:
		{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 8, 0x00000f00);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_AWB:
		{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 12, 0x0000f000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:
		{
			Afe_Set_Reg(AFE_DAC_CON1, SampleRate << 16, 0x000f0000);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_DAI:
		{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				Afe_Set_Reg(AFE_DAC_CON1, 0 << 20, 1 << 20);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				Afe_Set_Reg(AFE_DAC_CON1, 1 << 20, 1 << 20);
			else
				return false;
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		{
			if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
				Afe_Set_Reg(AFE_DAC_CON1, 0 << 30, 1 << 30);
			else if (SampleRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
				Afe_Set_Reg(AFE_DAC_CON1, 1 << 30, 1 << 30);
			else
				return false;
			break;
		}
		return true;
	}
	return true;
}

bool SetChannels(uint32_t Memory_Interface, uint32_t channel)
{
	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_AWB:
		{
			Afe_Set_Reg(AFE_DAC_CON1, channel << 24, 1 << 24);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_VUL:
		{
			Afe_Set_Reg(AFE_DAC_CON1, channel << 27, 1 << 27);
			break;
		}
	default:
		pr_info("SetChannels  Memory_Interface = %d, channel = %d\n",
			Memory_Interface, channel);
		return false;
	}
	return true;
}

bool Set2ndI2SOut(uint32_t samplerate)
{
	uint32_t u32AudioI2S = 0;
	pr_info("Set2ndI2SOut\n");

	/* set 2nd samplerate to AFE_ADC_CON1 */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S, samplerate);
	u32AudioI2S |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	u32AudioI2S |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	u32AudioI2S |= (Soc_Aud_I2S_SRC_MASTER_MODE << 2);	/* default set to master mode */
	u32AudioI2S |= (Soc_Aud_I2S_WLEN_WLEN_16BITS << 1);
	Afe_Set_Reg(AFE_I2S_CON, u32AudioI2S, MASK_ALL);
	return true;
}

bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate)
{
	return true;
}

bool SetI2SAdcIn(struct AudioDigtalI2S *DigtalI2S)
{
	memcpy((void *)AudioAdcI2S, (void *)DigtalI2S, sizeof(struct AudioDigtalI2S));

	if (!AudioAdcI2SStatus) {
		uint32_t eSamplingRate = SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE);
		uint32_t dVoiceModeSelect = 0;
		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1);	/* Using Internal ADC */
		if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_8K)
			dVoiceModeSelect = 0;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_16K)
			dVoiceModeSelect = 1;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_32K)
			dVoiceModeSelect = 2;
		else if (eSamplingRate == Soc_Aud_I2S_SAMPLERATE_I2S_48K)
			dVoiceModeSelect = 3;

		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0,
			    ((dVoiceModeSelect << 2) | dVoiceModeSelect) << 17, 0x001E0000);
		/* Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF); // up8x txif sat on */
		Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, ((dVoiceModeSelect < 3) ? 1 : 3) << 10,
			    0x00000C00);

	} else {
		uint32_t Audio_I2S_Adc = 0;
		Afe_Set_Reg(AFE_ADDA_TOP_CON0, 1, 0x1);	/* Using External ADC */
		Audio_I2S_Adc |= (AudioAdcI2S->mLR_SWAP << 31);
		Audio_I2S_Adc |= (AudioAdcI2S->mBuffer_Update_word << 24);
		Audio_I2S_Adc |= (AudioAdcI2S->mINV_LRCK << 23);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit_test << 22);
		Audio_I2S_Adc |= (AudioAdcI2S->mFpga_bit << 21);
		Audio_I2S_Adc |= (AudioAdcI2S->mloopback << 20);
		Audio_I2S_Adc |= (SampleRateTransform(AudioAdcI2S->mI2S_SAMPLERATE) << 8);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_FMT << 3);
		Audio_I2S_Adc |= (AudioAdcI2S->mI2S_WLEN << 1);
		pr_info("%s Audio_I2S_Adc = 0x%x", __func__, Audio_I2S_Adc);
		Afe_Set_Reg(AFE_I2S_CON2, Audio_I2S_Adc, MASK_ALL);
	}
	return true;
}

bool EnableSideGenHw(uint32_t connection, bool direction, bool Enable)
{
	pr_info("+%s(), connection = %d, direction = %d, Enable= %d\n",
		__func__, connection, direction, Enable);
	if (Enable && direction) {
		switch (connection) {
		case Soc_Aud_InterConnectionInput_I00:
		case Soc_Aud_InterConnectionInput_I01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x04662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x14662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I03:
		case Soc_Aud_InterConnectionInput_I04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x24662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I05:
		case Soc_Aud_InterConnectionInput_I06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x34662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I07:
		case Soc_Aud_InterConnectionInput_I08:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x44662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I09:
		case Soc_Aud_InterConnectionInput_I10:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x54662662, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionInput_I11:
		case Soc_Aud_InterConnectionInput_I12:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x64662662, 0xffffffff);
			break;
		default:
			break;
		}
	} else if (Enable) {
		switch (connection) {
		case Soc_Aud_InterConnectionOutput_O00:
		case Soc_Aud_InterConnectionOutput_O01:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x746c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O02:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x846c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O03:
		case Soc_Aud_InterConnectionOutput_O04:
			Afe_Set_Reg(AFE_SGEN_CON0, 0x946c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O05:
		case Soc_Aud_InterConnectionOutput_O06:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xa46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O07:
		case Soc_Aud_InterConnectionOutput_O08:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xb46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O09:
		case Soc_Aud_InterConnectionOutput_O10:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xc46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O11:
			Afe_Set_Reg(AFE_SGEN_CON0, 0xd46c26c2, 0xffffffff);
			break;
		case Soc_Aud_InterConnectionOutput_O12:
			/* MD connect BT Verify (8K SamplingRate) */
			if (Soc_Aud_I2S_SAMPLERATE_I2S_8K ==
			    mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate) {
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe40e80e8, 0xffffffff);
			} else if (Soc_Aud_I2S_SAMPLERATE_I2S_16K ==
				   mAudioMEMIF[Soc_Aud_Digital_Block_MEM_MOD_DAI]->mSampleRate) {
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe40f00f0, 0xffffffff);
			} else {
				Afe_Set_Reg(AFE_SGEN_CON0, 0xe46c26c2, 0xffffffff);	/* Default */
			}
			break;
		default:
			break;
		}
	} else {
		/* don't set [31:28] as 0 when disable sinetone HW, because it will repalce i00/i01 input
		   with sine gen output. */
		/* Set 0xf is correct way to disconnect sinetone HW to any I/O. */
		Afe_Set_Reg(AFE_SGEN_CON0, 0xf0000000, 0xffffffff);
	}
	return true;
}

bool SetI2SAdcEnable(bool bEnable)
{
	Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, bEnable ? 1 : 0, 0x01);
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState = bEnable;
	if (bEnable == true) {
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0001, 0x0001);
	} else if (mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState == false &&
		   mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState == false) {
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0000, 0x0001);
	}
	return true;
}

bool Set2ndI2SEnable(bool bEnable)
{
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	return true;
}

void CleanPreDistortion(void)
{
	pr_debug("%s\n", __func__);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
}

bool SetDLSrc2(uint32_t SampleRate)
{
	uint32_t AfeAddaDLSrc2Con0, AfeAddaDLSrc2Con1;

	if (likely(SampleRate == 44100))
		AfeAddaDLSrc2Con0 = 7;
	else if (SampleRate == 8000)
		AfeAddaDLSrc2Con0 = 0;
	else if (SampleRate == 11025)
		AfeAddaDLSrc2Con0 = 1;
	else if (SampleRate == 12000)
		AfeAddaDLSrc2Con0 = 2;
	else if (SampleRate == 16000)
		AfeAddaDLSrc2Con0 = 3;
	else if (SampleRate == 22050)
		AfeAddaDLSrc2Con0 = 4;
	else if (SampleRate == 24000)
		AfeAddaDLSrc2Con0 = 5;
	else if (SampleRate == 32000)
		AfeAddaDLSrc2Con0 = 6;
	else if (SampleRate == 48000)
		AfeAddaDLSrc2Con0 = 8;
	else
		AfeAddaDLSrc2Con0 = 7;	/* Default 44100 */

	/* ASSERT(0); */
	if (AfeAddaDLSrc2Con0 == 0 || AfeAddaDLSrc2Con0 == 3) {	/* 8k or 16k voice mode */
		AfeAddaDLSrc2Con0 =
		    (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11) | (0x01 << 5);
	} else {
		AfeAddaDLSrc2Con0 = (AfeAddaDLSrc2Con0 << 28) | (0x03 << 24) | (0x03 << 11);
	}
	/* SA suggest apply -0.3db to audio/speech path */
	AfeAddaDLSrc2Con0 = AfeAddaDLSrc2Con0 | (0x01 << 1);	/* 2013.02.22 for voice mode degrade 0.3 db */
	AfeAddaDLSrc2Con1 = 0xf74f0000;

	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, AfeAddaDLSrc2Con0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, AfeAddaDLSrc2Con1, MASK_ALL);
	return true;
}

bool SetI2SDacOut(uint32_t SampleRate)
{
	uint32_t Audio_I2S_Dac = 0;
	pr_debug("SetI2SDacOut\n");
	CleanPreDistortion();
	SetDLSrc2(SampleRate);

	Audio_I2S_Dac |= (Soc_Aud_LR_SWAP_NO_SWAP << 31);
	Audio_I2S_Dac |= (SampleRateTransform(SampleRate) << 8);
	Audio_I2S_Dac |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	Audio_I2S_Dac |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	Audio_I2S_Dac |= (Soc_Aud_I2S_WLEN_WLEN_16BITS << 1);
	Afe_Set_Reg(AFE_I2S_CON1, Audio_I2S_Dac, MASK_ALL);
	return true;
}

bool SetHdmiPathEnable(bool bEnable)
{
	mAudioMEMIF[Soc_Aud_Digital_Block_HDMI]->mState = bEnable;
	return true;
}

bool SetHwDigitalGainMode(uint32_t GainType, uint32_t SampleRate, uint32_t SamplePerStep)
{
	uint32_t value = 0;
	pr_info("SetHwDigitalGainMode GainType = %d, SampleRate = %d, SamplePerStep= %d\n",
		  GainType, SampleRate, SamplePerStep);
	value = SamplePerStep << 8 | SampleRate << 4;
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		return false;
	}
	return true;
}

bool SetHwDigitalGainEnable(int GainType, bool Enable)
{
	pr_info("+%s(), GainType = %d, Enable = %d\n", __func__, GainType, Enable);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			Afe_Set_Reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);
		}
		Afe_Set_Reg(AFE_GAIN1_CON0, Enable, 0x1);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		if (Enable) {
			/* Let current gain be 0 to ramp up */
			Afe_Set_Reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);
		}
		Afe_Set_Reg(AFE_GAIN2_CON0, Enable, 0x1);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetHwDigitalGain(uint32_t Gain, int GainType)
{
	pr_info("+%s(), Gain = 0x%x, gain type = %d\n", __func__, Gain, GainType);
	switch (GainType) {
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON1, Gain, 0xffffffff);
		break;
	case Soc_Aud_Hw_Digital_Gain_HW_DIGITAL_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON1, Gain, 0xffffffff);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool SetModemPcmConfig(int modem_index, struct AudioDigitalPCM p_modem_pcm_attribute)
{
	uint32_t reg_pcm2_intf_con = 0;
	pr_info("+%s()\n", __func__);

	if (modem_index == MODEM_1) {
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x1) << 13;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x1) << 12;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mSingelMicSel & 0x1) << 7;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmWordLength & 0x1) << 4;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x1) << 3;
		reg_pcm2_intf_con |= (p_modem_pcm_attribute.mPcmFormat & 0x3) << 1;
		pr_info("%s(), PCM2_INTF_CON(0x%x) = 0x%x\n", __func__, PCM2_INTF_CON,
			  reg_pcm2_intf_con);
		Afe_Set_Reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		/* config ASRC for modem 2 */
		if (p_modem_pcm_attribute.mPcmModeWidebandSel == Soc_Aud_PCM_MODE_PCM_MODE_8K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00001964, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00001964, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x00000CB2, MASK_ALL);
		} else if (p_modem_pcm_attribute.mPcmModeWidebandSel ==
			   Soc_Aud_PCM_MODE_PCM_MODE_16K) {
			Afe_Set_Reg(AFE_ASRC_CON1, 0x00000cb2, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON2, 0x00400000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON3, 0x00400000, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON4, 0x00000cb2, MASK_ALL);
			Afe_Set_Reg(AFE_ASRC_CON7, 0x00000659, MASK_ALL);
		}

		{
			uint32_t reg_pcm_intf_con1 = 0;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mTxLchRepeatSel & 0x01) << 19;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mVbt16kModeSel & 0x01) << 18;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mExtModemSel & 0x01) << 17;
			reg_pcm_intf_con1 |=
			    (p_modem_pcm_attribute.mExtendBckSyncLength & 0x1F) << 9;
			reg_pcm_intf_con1 |=
			    (p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01) << 8;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSingelMicSel & 0x01) << 7;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mAsyncFifoSel & 0x01) << 6;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mSlaveModeSel & 0x01) << 5;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmWordLength & 0x01) << 4;
			reg_pcm_intf_con1 |=
			    (p_modem_pcm_attribute.mPcmModeWidebandSel & 0x01) << 3;
			reg_pcm_intf_con1 |= (p_modem_pcm_attribute.mPcmFormat & 0x03) << 1;

			pr_info("%s(), PCM_INTF_CON1(0x%x) = 0x%x", __func__, PCM_INTF_CON1,
				  reg_pcm_intf_con1);
			Afe_Set_Reg(PCM_INTF_CON1, reg_pcm_intf_con1, MASK_ALL);
		}
	}
	return true;
}

bool SetModemPcmEnable(int modem_index, bool modem_pcm_on)
{
	pr_info("+%s(), modem_index = %d, modem_pcm_on = %d\n", __func__, modem_index,
		  modem_pcm_on);

	if (modem_index == MODEM_1) {	/* MODEM_1 use PCM2_INTF_CON (0x53C) !!! */
		Afe_Set_Reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_1_O]->mState = modem_pcm_on;
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		if (modem_pcm_on == true) {	/* turn on ASRC before Modem PCM on */
			Afe_Set_Reg(AFE_ASRC_CON6, 0x0001183F, MASK_ALL);	/* pre ver. 0x0001188F */
			Afe_Set_Reg(AFE_ASRC_CON0, 0x06003031, 0xFFFFFFBF);
			Afe_Set_Reg(PCM_INTF_CON1, 0x1, 0x1);
		} else if (modem_pcm_on == false) {	/* turn off ASRC after Modem PCM off */
			Afe_Set_Reg(PCM_INTF_CON1, 0x0, 0x1);
			Afe_Set_Reg(AFE_ASRC_CON6, 0x00000000, MASK_ALL);
			{
				uint32_t dNeedDisableASM = (Afe_Get_Reg(AFE_ASRC_CON0) & 0x0040) ? 1 : 0;
				Afe_Set_Reg(AFE_ASRC_CON0, 0, (1 << 4 | 1 << 5 | dNeedDisableASM));
			}
			Afe_Set_Reg(AFE_ASRC_CON0, 0x0, 0x1);
		}
		mAudioMEMIF[Soc_Aud_Digital_Block_MODEM_PCM_2_O]->mState = modem_pcm_on;
	} else {
		pr_warn("%s(), no such modem_index: %d!!", __func__, modem_index);
		return false;
	}
	return true;
}

bool EnableSideToneFilter(bool stf_on)
{
	/* MD max support 16K sampling rate */
	const uint8_t kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable16k) / sizeof(uint16_t);
	pr_info("+%s(), stf_on = %d", __func__, stf_on);

	if (stf_on == false) {
		/* bypass STF result & disable */
		const bool bypass_stf_on = true;
		uint32_t reg_value = (bypass_stf_on << 31) | (stf_on << 8);
		Afe_Set_Reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
		pr_info("%s(), AFE_SIDETONE_CON1[0x%x] = 0x%x", __func__, AFE_SIDETONE_CON1,
			  reg_value);

		/* set side tone gain = 0 */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		pr_info("%s(), AFE_SIDETONE_GAIN[0x%x] = 0x%x", __func__, AFE_SIDETONE_GAIN, 0);
	} else {
		const bool bypass_stf_on = false;
		uint32_t write_reg_value =
		    (bypass_stf_on << 31) | (stf_on << 8) | kSideToneHalfTapNum;

		/* set side tone gain */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		pr_info("%s(), AFE_SIDETONE_GAIN[0x%x] = 0x%x", __func__, AFE_SIDETONE_GAIN, 0);

		/* using STF result & enable & set half tap num */
		Afe_Set_Reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
		pr_info("%s(), AFE_SIDETONE_CON1[0x%x] = 0x%x", __func__, AFE_SIDETONE_CON1,
			  write_reg_value);

		/* set side tone coefficient */
		{
			const bool enable_read_write = true;	/* enable read/write side tone coefficient */
			const bool read_write_sel = true;	/* for write case */
			const bool sel_ch2 = false;	/* using uplink ch1 as STF input */

			uint32_t read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
			size_t coef_addr = 0;
			for (coef_addr = 0; coef_addr < kSideToneHalfTapNum; coef_addr++) {
				int try_cnt = 0;
				bool old_write_ready = (read_reg_value >> 29) & 0x1;
				write_reg_value = enable_read_write << 25 |
				    read_write_sel << 24 |
				    sel_ch2 << 23 |
				    coef_addr << 16 | kSideToneCoefficientTable16k[coef_addr];
				Afe_Set_Reg(AFE_SIDETONE_CON0, write_reg_value, 0x39FFFFF);
				pr_info("%s(), AFE_SIDETONE_CON0[0x%x] = 0x%x", __func__,
					  AFE_SIDETONE_CON0, write_reg_value);

				/* wait until flag write_ready changed (means write done) */
				for (try_cnt = 0; try_cnt < 10; try_cnt++) {	/* max try 10 times */
					bool new_write_ready = 0;
					/* TODO: implement alternative way to delay execution instead of sleep */
					/* msleep(3); */
					read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
					new_write_ready = (read_reg_value >> 29) & 0x1;
					if (new_write_ready != old_write_ready) {	/* flip => ok */
						break;
					} else {
						BUG_ON(new_write_ready != old_write_ready);
						return false;
					}
				}
			}
		}
	}

	pr_info("-%s(), stf_on = %d", __func__, stf_on);
	return true;
}

bool SetMemoryPathEnable(uint32_t Aud_block, bool bEnable)
{
	if (unlikely(Aud_block >= Soc_Aud_Digital_Block_NUM_OF_MEM_INTERFACE))
		return true;

	if (bEnable) {
		mAudioMEMIF[Aud_block]->mState = true;
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1), 1 << (Aud_block + 1));
	} else {
#ifdef SIDEGEN_ENABLE
		Afe_Set_Reg(AFE_SGEN_CON0, 0x0, 0xffffffff);
#endif
		Afe_Set_Reg(AFE_DAC_CON0, bEnable << (Aud_block + 1), 1 << (Aud_block + 1));
		mAudioMEMIF[Aud_block]->mState = false;
	}
	return true;
}

bool SetI2SDacEnable(bool bEnable)
{
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState = bEnable;

	if (bEnable) {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
		/* Afe_Set_Reg(FPGA_CFG1, 0, 0x10);    //For FPGA Pin the same with DAC */
	} else {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, bEnable, 0x01);
		Afe_Set_Reg(AFE_I2S_CON1, bEnable, 0x1);

		if (!mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState &&
		    !mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_ADC]->mState) {
			Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, bEnable, 0x0001);
		}
		/* Afe_Set_Reg(FPGA_CFG1, 1 << 4, 0x10);    //For FPGA Pin the same with DAC */
	}
	return true;
}

bool GetI2SDacEnable(void)
{
	return mAudioMEMIF[Soc_Aud_Digital_Block_I2S_OUT_DAC]->mState;
}

bool SetConnection(uint32_t ConnectionState, uint32_t Input, uint32_t Output)
{
	return SetConnectionState(ConnectionState, Input, Output);
}

bool SetIrqEnable(uint32_t Irqmode, bool bEnable)
{
	pr_debug("%s(), Irqmode = %d, bEnable = %d\n", __func__, Irqmode, bEnable);

	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		if (unlikely(!bEnable && CheckUpLinkMemIfEnable())) {
			/* IRQ2 is in used */
			pr_info("skip disable IRQ2, AFE_DAC_CON0 = 0x%x\n",
				  Afe_Get_Reg(AFE_DAC_CON0));
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << Irqmode), (1 << Irqmode));
			mAudioMcuMode[Irqmode]->mStatus = bEnable;
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (bEnable << 12), (1 << 12));
			mAudioMcuMode[Irqmode]->mStatus = bEnable;
			break;
		}

	default:
		pr_warn("%s unexpected Irqmode = %d\n", __func__, Irqmode);
		break;
	}
	return true;
}

bool SetIrqMcuSampleRate(uint32_t Irqmode, uint32_t SampleRate)
{
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (SampleRateTransform(SampleRate) << 4),
				    0x000000f0);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_MCU_CON, (SampleRateTransform(SampleRate) << 8),
				    0x00000f00);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
		{
			/* mAudioMcuMode[Irqmode].mSampleRate = SampleRate; */
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
	default:
		return false;
	}
	return true;
}

bool SetIrqMcuCounter(uint32_t Irqmode, uint32_t Counter)
{
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_CNT1, Counter, 0xffffffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_CNT2, Counter, 0xffffffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
		{
			Afe_Set_Reg(AFE_IRQ_CNT5, Counter, 0xffffffff);
			break;
		}
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
	default:
		return false;
	}
	return true;
}

bool GetIrqStatus(uint32_t Irqmode, struct AudioIrqMcuMode *Mcumode)
{
	switch (Irqmode) {
	case Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE:
	case Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE:
	case Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE:
	case Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE:
		memcpy((void *)Mcumode, (const void *)mAudioMcuMode[Irqmode],
		       sizeof(struct AudioIrqMcuMode));
		break;
	default:
		return false;
	}
	return true;
}

bool SetMemDuplicateWrite(uint32_t InterfaceType, int dupwrite)
{
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DAI:
		{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 29, 1 << 29);
			break;
		}
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		{
			Afe_Set_Reg(AFE_DAC_CON1, dupwrite << 31, 1 << 31);
			break;
		}
	default:
		return false;
	}
	return true;
}

bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode)
{
	/* 8135 not support */
	return false;
}

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S)
{
	/* 8135 not support */
	return false;
}

bool Set2ndI2SInEnable(bool bEnable)
{
	/* 8135 not support */
	return false;

#if 0
	pr_notice("Set2ndI2SInEnable bEnable = %d", bEnable);
	m2ndI2S->mI2S_EN = bEnable;
	Afe_Set_Reg(AFE_I2S_CON, bEnable, 0x1);
	mAudioMEMIF[Soc_Aud_Digital_Block_I2S_IN_2]->mState = bEnable;
	return true;
#endif
}

void SetDeepIdleEnableForAfe(bool enable)
{
#ifdef CONFIG_MTK_DEEP_IDLE
	if (enable)
		enable_dpidle_by_bit(MT_CG_AUDIO_PDN_AFE);
	else
		disable_dpidle_by_bit(MT_CG_AUDIO_PDN_AFE);
#endif
}

void SetDeepIdleEnableForHdmi(bool enable)
{
#ifdef CONFIG_MTK_DEEP_IDLE
	if (enable)
		enable_dpidle_by_bit(MT_CG_AUDIO_PDN_HDMI_CK);
	else
		disable_dpidle_by_bit(MT_CG_AUDIO_PDN_HDMI_CK);
#endif
}

/* TODO: GPIO should move to machine? */
void ResetFmChipMrgIf(void)
{
	struct mt_audio_custom_gpio_data *agd = &audio_gpio_data;

	int i, j;
	PRINTK_AUDDRV("!! AudDrv, +RESET FM Chip MergeIF\n");

	mt_pin_set_mode_gpio(agd->combo_i2s_ck);	/* clk */
	mt_pin_set_mode_gpio(agd->combo_i2s_ws);	/* sync */
	mt_pin_set_mode_gpio(agd->combo_i2s_dat);
	mt_pin_set_mode_gpio(agd->pcm_daiout);

	gpio_direction_output(agd->combo_i2s_ck, 0);	/* output */
	gpio_direction_output(agd->combo_i2s_ws, 0);	/* output */
	gpio_direction_input(agd->combo_i2s_dat);	/* input */
	gpio_direction_output(agd->pcm_daiout, 0);	/* output */

	ndelay(170);		/* delay 170ns */
	for (j = 0; j < 3; j++) {
		/* -- gen sync */
		gpio_set_value(agd->combo_i2s_ws, 1);	/* sync */
		ndelay(170);	/* delay 170ns */

		gpio_set_value(agd->combo_i2s_ck, 1);	/* clock */
		ndelay(170);	/* delay 170ns */
		gpio_set_value(agd->combo_i2s_ck, 0);	/* clock */
		ndelay(170);	/* delay 170ns */
		gpio_set_value(agd->combo_i2s_ws, 0);	/* sync */
		ndelay(170);	/* delay 170ns */

		/* -- send 56 clock to change state */
		for (i = 0; i < 56; i++) {
			gpio_set_value(agd->combo_i2s_ck, 1); /* clock */
			ndelay(170);	/* delay 170ns */
			gpio_set_value(agd->combo_i2s_ck, 0); /* clock */
			ndelay(170);	/* delay 170ns */
		}
	}

	/* -- gen sync */
	gpio_set_value(agd->combo_i2s_ws, 1);	/* sync */
	ndelay(170);		/* delay 170ns */
	gpio_set_value(agd->combo_i2s_ck, 1);	/* clock */
	ndelay(170);		/* delay 170ns */
	gpio_set_value(agd->combo_i2s_ck, 0);	/* clock */
	ndelay(170);		/* delay 170ns */
	gpio_set_value(agd->combo_i2s_ws, 0);	/* sync */
	PRINTK_AUDDRV("!! AudDrv, -RESET FM Chip MergeIF\n");
	return;

}

void SetDAIBTAttribute(void)
{
	uint32_t Audio_BT_DAI = 0;
	/* fix me , ned to base on actual situation */
	AudioDaiBt->mUSE_MRGIF_INPUT = Soc_Aud_BT_DAI_INPUT_FROM_MGRIF;
	/* korochecn fixed value (WCNChipController::GetInstance()->BTChipSamplingRate()) ? (Mode16K) : (Mode8K); */
	AudioDaiBt->mDAI_BT_MODE = Soc_Aud_DATBT_MODE_Mode8K;
	AudioDaiBt->mDAI_DEL = Soc_Aud_DAI_DEL_HighWord;
	/* korochecn fixed value WCNChipController::GetInstance()->BTChipSyncLength(); */
	AudioDaiBt->mBT_LEN = 0;
	AudioDaiBt->mDATA_RDY = true;
	/* korochecn fixed value WCNChipController::GetInstance()->BTChipSyncFormat(); */
	AudioDaiBt->mBT_SYNC = Soc_Aud_BTSYNC_Short_Sync;
	AudioDaiBt->mBT_ON = true;
	AudioDaiBt->mDAIBT_ON = false;

	Audio_BT_DAI |= (AudioDaiBt->mUSE_MRGIF_INPUT << 12);
	Audio_BT_DAI |= (AudioDaiBt->mDAI_BT_MODE << 9);
	Audio_BT_DAI |= (AudioDaiBt->mDAI_DEL << 8);
	Audio_BT_DAI |= (AudioDaiBt->mBT_LEN << 4);
	Audio_BT_DAI |= (AudioDaiBt->mDATA_RDY << 3);
	Audio_BT_DAI |= (AudioDaiBt->mBT_SYNC << 2);
	Audio_BT_DAI |= (AudioDaiBt->mBT_ON << 1);
	Afe_Set_Reg(AFE_DAIBT_CON0, Audio_BT_DAI, AFE_MASK_ALL);
}

void SetDAIBTEnable(bool bEnable)
{
	PRINTK_AUDDRV("SetDAIBTEnable = %d\n", bEnable);

	if (bEnable == true) {
		/* if (WCNChipController::GetInstance()->IsMergeInterfaceSupported() == true) */
		/* korocheck: always use MRGIF */
		{
			if (mAudioMrg->MrgIf_En == false) {
				ResetFmChipMrgIf();	/* send reset sequence to 6628 mergeinterface by GPIO */
				udelay(100);	/* delay between GPIO reset and merge interface enable */

				PRINTK_AUDDRV("MrgIf_En=0, SetDAIBTEnable AUDDRV_SET_BT_FM_GPIO");
				mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_3);	/* PCM_ON, Digital FM */
				/* set merge intf I2S mode to 44.1k(programming guide) */
				Afe_Set_Reg(AFE_MRGIF_CON, 0x900000, 0xf00000);
				/* Should turn on merge interface first then turn on DAI */
				Afe_Set_Reg(AFE_MRGIF_CON, 0x10000, 0x10000);
				Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);

				mAudioMrg->MrgIf_En = true;
			} else {
				/* if merge interface already turn on, take care of it if DAIBT sampleRate is different
				   (8K->16K or 16K->8K) */
				PRINTK_AUDDRV("MrgIf_En=1, SetDAIBTEnable AUDDRV_SET_BT_FM_GPIO");
				mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_3);	/* PCM_ON, Digital FM */
			}
		}
	}

	AudioDaiBt->mDAIBT_ON = bEnable;
	Afe_Set_Reg(AFE_DAIBT_CON0, bEnable, 0x1);
	mAudioMEMIF[Soc_Aud_Digital_Block_DAI_BT]->mState = bEnable;
	if (bEnable == false) {
		/* if (WCNChipController::GetInstance()->IsMergeInterfaceSupported() == true) */
		/* korocheck: always use MRGIF */
		{
			/* When try to disable DAIBT, should turn off merge interface after turn off DAI */
			if (mAudioMrg->MrgIf_En == true) {
				if (mAudioMrg->Mergeif_I2S_Enable == false) {
					/* Turn off merge interface if I2S is not used */
					/* must delay at least 1/32000 = 31.25us to avoid mrgif clk is shutdown before
					   the sync sequence(sent after AFE_DAIBT_CON0 bit0 is OFF) is complete */
					udelay(50);
					PRINTK_AUDDRV("SetDAIBTEnable disable mrgif clk after sync sequence done");
					Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);
					Afe_Set_Reg(AFE_MRGIF_CON, 0x00000, 0x10000);
					mAudioMrg->MrgIf_En = false;
				}
			}
		}

		PRINTK_AUDDRV("SetDAIBTEnable AUDDRV_SET_FM_I2S_GPIO");
		mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X) CMB_STUB_AIF_2);	/* PCM_OFF, Digital FM */
	}

}

void DoAfeSuspend(void)
{
	if (!AudDrvSuspendStatus) {
		AudDrv_Store_reg_AFE(&Suspend_reg);
		AudDrv_Suspend_Clk_Off();
		AudDrvSuspendStatus = true;
	}
}
EXPORT_SYMBOL_GPL(DoAfeSuspend);

void DoAfeResume(void)
{
	if (AudDrvSuspendStatus) {
		AudDrv_Suspend_Clk_On();
		AudDrv_Recover_reg_AFE(&Suspend_reg);
		AudDrvSuspendStatus = false;
		Auddrv_ResetBuffer(Soc_Aud_Digital_Block_MEM_DL1);
	}
}
EXPORT_SYMBOL_GPL(DoAfeResume);

void AudDrv_Recover_reg_AFE(struct AudAfe_Suspend_Reg *pBackup_reg)
{
	pr_notice("+%s\n", __func__);

	if (!pBackup_reg) {
		pr_warn(" AudDrv pBackup_reg is null\n");
		return;
	}

	AudDrv_Clk_On();
	/* Digital register setting */
	Afe_Set_Reg(AUDIO_TOP_CON3, pBackup_reg->Suspend_AUDIO_TOP_CON3, MASK_ALL);
	Afe_Set_Reg(AFE_DAC_CON0, pBackup_reg->Suspend_AFE_DAC_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_DAC_CON1, pBackup_reg->Suspend_AFE_DAC_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_I2S_CON, pBackup_reg->Suspend_AFE_I2S_CON, MASK_ALL);
	Afe_Set_Reg(AFE_DAIBT_CON0, pBackup_reg->Suspend_AFE_DAIBT_CON0, MASK_ALL);

	Afe_Set_Reg(AFE_CONN0, pBackup_reg->Suspend_AFE_CONN0, MASK_ALL);
	Afe_Set_Reg(AFE_CONN1, pBackup_reg->Suspend_AFE_CONN1, MASK_ALL);
	Afe_Set_Reg(AFE_CONN2, pBackup_reg->Suspend_AFE_CONN2, MASK_ALL);
	Afe_Set_Reg(AFE_CONN3, pBackup_reg->Suspend_AFE_CONN3, MASK_ALL);
	Afe_Set_Reg(AFE_CONN4, pBackup_reg->Suspend_AFE_CONN4, MASK_ALL);

	Afe_Set_Reg(AFE_I2S_CON1, pBackup_reg->Suspend_AFE_I2S_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_I2S_CON2, pBackup_reg->Suspend_AFE_I2S_CON2, MASK_ALL);
	Afe_Set_Reg(AFE_MRGIF_CON, pBackup_reg->Suspend_AFE_MRGIF_CON, MASK_ALL);

	Afe_Set_Reg(AFE_DL1_BASE, pBackup_reg->Suspend_AFE_DL1_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_DL1_CUR, pBackup_reg->Suspend_AFE_DL1_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_DL1_END, pBackup_reg->Suspend_AFE_DL1_END, MASK_ALL);
	Afe_Set_Reg(AFE_DL2_BASE, pBackup_reg->Suspend_AFE_DL2_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_DL2_CUR, pBackup_reg->Suspend_AFE_DL2_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_DL2_END, pBackup_reg->Suspend_AFE_DL2_END, MASK_ALL);
	Afe_Set_Reg(AFE_AWB_BASE, pBackup_reg->Suspend_AFE_AWB_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_AWB_CUR, pBackup_reg->Suspend_AFE_AWB_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_AWB_END, pBackup_reg->Suspend_AFE_AWB_END, MASK_ALL);
	Afe_Set_Reg(AFE_VUL_BASE, pBackup_reg->Suspend_AFE_VUL_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_VUL_CUR, pBackup_reg->Suspend_AFE_VUL_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_VUL_END, pBackup_reg->Suspend_AFE_VUL_END, MASK_ALL);
	Afe_Set_Reg(AFE_DAI_BASE, pBackup_reg->Suspend_AFE_DAI_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_DAI_CUR, pBackup_reg->Suspend_AFE_DAI_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_DAI_END, pBackup_reg->Suspend_AFE_DAI_END, MASK_ALL);

	Afe_Set_Reg(AFE_IRQ_CON, pBackup_reg->Suspend_AFE_IRQ_CON, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MON0, pBackup_reg->Suspend_AFE_MEMIF_MON0, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MON1, pBackup_reg->Suspend_AFE_MEMIF_MON1, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MON2, pBackup_reg->Suspend_AFE_MEMIF_MON2, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MON3, pBackup_reg->Suspend_AFE_MEMIF_MON3, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MON4, pBackup_reg->Suspend_AFE_MEMIF_MON4, MASK_ALL);

	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, pBackup_reg->Suspend_AFE_ADDA_DL_SRC2_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, pBackup_reg->Suspend_AFE_ADDA_DL_SRC2_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, pBackup_reg->Suspend_AFE_ADDA_UL_SRC_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_UL_SRC_CON1, pBackup_reg->Suspend_AFE_ADDA_UL_SRC_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_TOP_CON0, pBackup_reg->Suspend_AFE_ADDA_TOP_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, pBackup_reg->Suspend_AFE_ADDA_UL_DL_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_NEWIF_CFG0, pBackup_reg->Suspend_AFE_ADDA_NEWIF_CFG0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_NEWIF_CFG1, pBackup_reg->Suspend_AFE_ADDA_NEWIF_CFG1, MASK_ALL);

	Afe_Set_Reg(AFE_FOC_CON, pBackup_reg->Suspend_AFE_FOC_CON, MASK_ALL);
	Afe_Set_Reg(AFE_FOC_CON1, pBackup_reg->Suspend_AFE_FOC_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_FOC_CON2, pBackup_reg->Suspend_AFE_FOC_CON2, MASK_ALL);
	Afe_Set_Reg(AFE_FOC_CON3, pBackup_reg->Suspend_AFE_FOC_CON3, MASK_ALL);
	Afe_Set_Reg(AFE_FOC_CON4, pBackup_reg->Suspend_AFE_FOC_CON4, MASK_ALL);
	Afe_Set_Reg(AFE_FOC_CON5, pBackup_reg->Suspend_AFE_FOC_CON5, MASK_ALL);

	Afe_Set_Reg(AFE_MON_STEP, pBackup_reg->Suspend_AFE_MON_STEP, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_DEBUG, pBackup_reg->Suspend_AFE_SIDETONE_DEBUG, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_MON, pBackup_reg->Suspend_AFE_SIDETONE_MON, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_CON0, pBackup_reg->Suspend_AFE_SIDETONE_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_COEFF, pBackup_reg->Suspend_AFE_SIDETONE_COEFF, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_CON1, pBackup_reg->Suspend_AFE_SIDETONE_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_SIDETONE_GAIN, pBackup_reg->Suspend_AFE_SIDETONE_GAIN, MASK_ALL);
	Afe_Set_Reg(AFE_SGEN_CON0, pBackup_reg->Suspend_AFE_SGEN_CON0, MASK_ALL);


	Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, pBackup_reg->Suspend_AFE_ADDA_PREDIS_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, pBackup_reg->Suspend_AFE_ADDA_PREDIS_CON1, MASK_ALL);

	Afe_Set_Reg(AFE_MRGIF_MON0, pBackup_reg->Suspend_AFE_MRGIF_MON0, MASK_ALL);
	Afe_Set_Reg(AFE_MRGIF_MON1, pBackup_reg->Suspend_AFE_MRGIF_MON1, MASK_ALL);
	Afe_Set_Reg(AFE_MRGIF_MON2, pBackup_reg->Suspend_AFE_MRGIF_MON2, MASK_ALL);

	Afe_Set_Reg(AFE_HDMI_OUT_CON0, pBackup_reg->Suspend_AFE_HDMI_OUT_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_HDMI_OUT_BASE, pBackup_reg->Suspend_AFE_HDMI_OUT_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_HDMI_OUT_CUR, pBackup_reg->Suspend_AFE_HDMI_OUT_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_HDMI_OUT_END, pBackup_reg->Suspend_AFE_HDMI_OUT_END, MASK_ALL);
	Afe_Set_Reg(AFE_SPDIF_OUT_CON0, pBackup_reg->Suspend_AFE_SPDIF_OUT_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_SPDIF_BASE, pBackup_reg->Suspend_AFE_SPDIF_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_SPDIF_CUR, pBackup_reg->Suspend_AFE_SPDIF_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_SPDIF_END, pBackup_reg->Suspend_AFE_SPDIF_END, MASK_ALL);
	Afe_Set_Reg(AFE_HDMI_CONN0, pBackup_reg->Suspend_AFE_HDMI_CONN0, MASK_ALL);
	Afe_Set_Reg(AFE_8CH_I2S_OUT_CON, pBackup_reg->Suspend_AFE_8CH_I2S_OUT_CON, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_CFG, pBackup_reg->Suspend_AFE_IEC_CFG, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_NSNUM, pBackup_reg->Suspend_AFE_IEC_NSNUM, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_BURST_INFO, pBackup_reg->Suspend_AFE_IEC_BURST_INFO, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_BURST_LEN, pBackup_reg->Suspend_AFE_IEC_BURST_LEN, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_NSADR, pBackup_reg->Suspend_AFE_IEC_NSADR, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_CHL_STAT0, pBackup_reg->Suspend_AFE_IEC_CHL_STAT0, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_CHL_STAT1, pBackup_reg->Suspend_AFE_IEC_CHL_STAT1, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_CHR_STAT0, pBackup_reg->Suspend_AFE_IEC_CHR_STAT0, MASK_ALL);
	Afe_Set_Reg(AFE_IEC_CHR_STAT1, pBackup_reg->Suspend_AFE_IEC_CHR_STAT1, MASK_ALL);

	Afe_Set_Reg(AFE_MOD_PCM_BASE, pBackup_reg->Suspend_AFE_MOD_PCM_BASE, MASK_ALL);
	Afe_Set_Reg(AFE_MOD_PCM_END, pBackup_reg->Suspend_AFE_MOD_PCM_END, MASK_ALL);
	Afe_Set_Reg(AFE_MOD_PCM_CUR, pBackup_reg->Suspend_AFE_MOD_PCM_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_MCU_CON, pBackup_reg->Suspend_AFE_IRQ_MCU_CON, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_STATUS, pBackup_reg->Suspend_AFE_IRQ_STATUS, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_CLR, pBackup_reg->Suspend_AFE_IRQ_CLR, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_CNT1, pBackup_reg->Suspend_AFE_IRQ_CNT1, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_CNT2, pBackup_reg->Suspend_AFE_IRQ_CNT2, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_MON2, pBackup_reg->Suspend_AFE_IRQ_MON2, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ_CNT5, pBackup_reg->Suspend_AFE_IRQ_CNT5, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ1_CNT_MON, pBackup_reg->Suspend_AFE_IRQ1_CNT_MON, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ2_CNT_MON, pBackup_reg->Suspend_AFE_IRQ2_CNT_MON, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ1_EN_CNT_MON, pBackup_reg->Suspend_AFE_IRQ1_EN_CNT_MON, MASK_ALL);
	Afe_Set_Reg(AFE_IRQ5_EN_CNT_MON, pBackup_reg->Suspend_AFE_IRQ5_EN_CNT_MON, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MINLEN, pBackup_reg->Suspend_AFE_MEMIF_MINLEN, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_MAXLEN, pBackup_reg->Suspend_AFE_MEMIF_MAXLEN, MASK_ALL);
	Afe_Set_Reg(AFE_MEMIF_PBUF_SIZE, pBackup_reg->Suspend_AFE_MEMIF_PBUF_SIZE, MASK_ALL);

	Afe_Set_Reg(AFE_GAIN1_CON0, pBackup_reg->Suspend_AFE_GAIN1_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN1_CON1, pBackup_reg->Suspend_AFE_GAIN1_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN1_CON2, pBackup_reg->Suspend_AFE_GAIN1_CON2, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN1_CON3, pBackup_reg->Suspend_AFE_GAIN1_CON3, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN1_CONN, pBackup_reg->Suspend_AFE_GAIN1_CONN, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN1_CUR, pBackup_reg->Suspend_AFE_GAIN1_CUR, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN2_CON0, pBackup_reg->Suspend_AFE_GAIN2_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN2_CON1, pBackup_reg->Suspend_AFE_GAIN2_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN2_CON2, pBackup_reg->Suspend_AFE_GAIN2_CON2, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN2_CON3, pBackup_reg->Suspend_AFE_GAIN2_CON3, MASK_ALL);
	Afe_Set_Reg(AFE_GAIN2_CONN, pBackup_reg->Suspend_AFE_GAIN2_CONN, MASK_ALL);

	Afe_Set_Reg(AFE_ASRC_CON0, pBackup_reg->Suspend_AFE_ASRC_CON0, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON1, pBackup_reg->Suspend_AFE_ASRC_CON1, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON2, pBackup_reg->Suspend_AFE_ASRC_CON2, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON3, pBackup_reg->Suspend_AFE_ASRC_CON3, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON4, pBackup_reg->Suspend_AFE_ASRC_CON4, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON6, pBackup_reg->Suspend_AFE_ASRC_CON6, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON7, pBackup_reg->Suspend_AFE_ASRC_CON7, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON8, pBackup_reg->Suspend_AFE_ASRC_CON8, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON9, pBackup_reg->Suspend_AFE_ASRC_CON9, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON10, pBackup_reg->Suspend_AFE_ASRC_CON10, MASK_ALL);
	Afe_Set_Reg(AFE_ASRC_CON11, pBackup_reg->Suspend_AFE_ASRC_CON11, MASK_ALL);
	Afe_Set_Reg(PCM_INTF_CON1, pBackup_reg->Suspend_PCM_INTF_CON1, MASK_ALL);
	Afe_Set_Reg(PCM_INTF_CON2, pBackup_reg->Suspend_PCM_INTF_CON2, MASK_ALL);
	Afe_Set_Reg(PCM2_INTF_CON, pBackup_reg->Suspend_PCM2_INTF_CON, MASK_ALL);
	AudDrv_Clk_Off();
	pr_notice("-%s\n", __func__);
}

void AudDrv_Store_reg_AFE(struct AudAfe_Suspend_Reg *pBackup_reg)
{
	pr_notice("+%s\n", __func__);

	if (!pBackup_reg) {
		pr_warn("AudDrv pBackup_reg is null\n");
		pr_warn("AudDrv -AudDrv_Store_reg\n");
		return;
	}

	AudDrv_Clk_On();

	/* pBackup_reg->Suspend_AUDIO_TOP_CON0=            Afe_Get_Reg(AUDIO_TOP_CON0); */
	pBackup_reg->Suspend_AUDIO_TOP_CON3 = Afe_Get_Reg(AUDIO_TOP_CON3);
	pBackup_reg->Suspend_AFE_DAC_CON0 = Afe_Get_Reg(AFE_DAC_CON0);
	pBackup_reg->Suspend_AFE_DAC_CON1 = Afe_Get_Reg(AFE_DAC_CON1);
	pBackup_reg->Suspend_AFE_I2S_CON = Afe_Get_Reg(AFE_I2S_CON);
	pBackup_reg->Suspend_AFE_DAIBT_CON0 = Afe_Get_Reg(AFE_DAIBT_CON0);

	pBackup_reg->Suspend_AFE_CONN0 = Afe_Get_Reg(AFE_CONN0);
	pBackup_reg->Suspend_AFE_CONN1 = Afe_Get_Reg(AFE_CONN1);
	pBackup_reg->Suspend_AFE_CONN2 = Afe_Get_Reg(AFE_CONN2);
	pBackup_reg->Suspend_AFE_CONN3 = Afe_Get_Reg(AFE_CONN3);
	pBackup_reg->Suspend_AFE_CONN4 = Afe_Get_Reg(AFE_CONN4);

	pBackup_reg->Suspend_AFE_I2S_CON1 = Afe_Get_Reg(AFE_I2S_CON1);
	pBackup_reg->Suspend_AFE_I2S_CON2 = Afe_Get_Reg(AFE_I2S_CON2);
	pBackup_reg->Suspend_AFE_MRGIF_CON = Afe_Get_Reg(AFE_MRGIF_CON);

	pBackup_reg->Suspend_AFE_DL1_BASE = Afe_Get_Reg(AFE_DL1_BASE);
	pBackup_reg->Suspend_AFE_DL1_CUR = Afe_Get_Reg(AFE_DL1_CUR);
	pBackup_reg->Suspend_AFE_DL1_END = Afe_Get_Reg(AFE_DL1_END);
	pBackup_reg->Suspend_AFE_DL2_BASE = Afe_Get_Reg(AFE_DL2_BASE);
	pBackup_reg->Suspend_AFE_DL2_CUR = Afe_Get_Reg(AFE_DL2_CUR);
	pBackup_reg->Suspend_AFE_DL2_END = Afe_Get_Reg(AFE_DL2_END);
	pBackup_reg->Suspend_AFE_AWB_BASE = Afe_Get_Reg(AFE_AWB_BASE);
	pBackup_reg->Suspend_AFE_AWB_CUR = Afe_Get_Reg(AFE_AWB_CUR);
	pBackup_reg->Suspend_AFE_AWB_END = Afe_Get_Reg(AFE_AWB_END);
	pBackup_reg->Suspend_AFE_VUL_BASE = Afe_Get_Reg(AFE_VUL_BASE);
	pBackup_reg->Suspend_AFE_VUL_CUR = Afe_Get_Reg(AFE_VUL_CUR);
	pBackup_reg->Suspend_AFE_VUL_END = Afe_Get_Reg(AFE_VUL_END);
	pBackup_reg->Suspend_AFE_DAI_BASE = Afe_Get_Reg(AFE_DAI_BASE);
	pBackup_reg->Suspend_AFE_DAI_CUR = Afe_Get_Reg(AFE_DAI_CUR);
	pBackup_reg->Suspend_AFE_DAI_END = Afe_Get_Reg(AFE_DAI_END);

	pBackup_reg->Suspend_AFE_IRQ_CON = Afe_Get_Reg(AFE_IRQ_CON);
	pBackup_reg->Suspend_AFE_MEMIF_MON0 = Afe_Get_Reg(AFE_MEMIF_MON0);
	pBackup_reg->Suspend_AFE_MEMIF_MON1 = Afe_Get_Reg(AFE_MEMIF_MON1);
	pBackup_reg->Suspend_AFE_MEMIF_MON2 = Afe_Get_Reg(AFE_MEMIF_MON2);
	pBackup_reg->Suspend_AFE_MEMIF_MON3 = Afe_Get_Reg(AFE_MEMIF_MON3);
	pBackup_reg->Suspend_AFE_MEMIF_MON4 = Afe_Get_Reg(AFE_MEMIF_MON4);

	pBackup_reg->Suspend_AFE_ADDA_DL_SRC2_CON0 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0);
	pBackup_reg->Suspend_AFE_ADDA_DL_SRC2_CON1 = Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1);
	pBackup_reg->Suspend_AFE_ADDA_UL_SRC_CON0 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0);
	pBackup_reg->Suspend_AFE_ADDA_UL_SRC_CON1 = Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1);
	pBackup_reg->Suspend_AFE_ADDA_TOP_CON0 = Afe_Get_Reg(AFE_ADDA_TOP_CON0);
	pBackup_reg->Suspend_AFE_ADDA_UL_DL_CON0 = Afe_Get_Reg(AFE_ADDA_UL_DL_CON0);
	pBackup_reg->Suspend_AFE_ADDA_NEWIF_CFG0 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0);
	pBackup_reg->Suspend_AFE_ADDA_NEWIF_CFG1 = Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1);

	pBackup_reg->Suspend_AFE_FOC_CON = Afe_Get_Reg(AFE_FOC_CON);
	pBackup_reg->Suspend_AFE_FOC_CON1 = Afe_Get_Reg(AFE_FOC_CON1);
	pBackup_reg->Suspend_AFE_FOC_CON2 = Afe_Get_Reg(AFE_FOC_CON2);
	pBackup_reg->Suspend_AFE_FOC_CON3 = Afe_Get_Reg(AFE_FOC_CON3);
	pBackup_reg->Suspend_AFE_FOC_CON4 = Afe_Get_Reg(AFE_FOC_CON4);
	pBackup_reg->Suspend_AFE_FOC_CON5 = Afe_Get_Reg(AFE_FOC_CON5);

	pBackup_reg->Suspend_AFE_MON_STEP = Afe_Get_Reg(AFE_MON_STEP);
	pBackup_reg->Suspend_AFE_SIDETONE_DEBUG = Afe_Get_Reg(AFE_SIDETONE_DEBUG);
	pBackup_reg->Suspend_AFE_SIDETONE_MON = Afe_Get_Reg(AFE_SIDETONE_MON);
	pBackup_reg->Suspend_AFE_SIDETONE_CON0 = Afe_Get_Reg(AFE_SIDETONE_CON0);
	pBackup_reg->Suspend_AFE_SIDETONE_COEFF = Afe_Get_Reg(AFE_SIDETONE_COEFF);
	pBackup_reg->Suspend_AFE_SIDETONE_CON1 = Afe_Get_Reg(AFE_SIDETONE_CON1);
	pBackup_reg->Suspend_AFE_SIDETONE_GAIN = Afe_Get_Reg(AFE_SIDETONE_GAIN);
	pBackup_reg->Suspend_AFE_SGEN_CON0 = Afe_Get_Reg(AFE_SGEN_CON0);


	pBackup_reg->Suspend_AFE_ADDA_PREDIS_CON0 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON0);
	pBackup_reg->Suspend_AFE_ADDA_PREDIS_CON1 = Afe_Get_Reg(AFE_ADDA_PREDIS_CON1);

	pBackup_reg->Suspend_AFE_MRGIF_MON0 = Afe_Get_Reg(AFE_MRGIF_MON0);
	pBackup_reg->Suspend_AFE_MRGIF_MON1 = Afe_Get_Reg(AFE_MRGIF_MON1);
	pBackup_reg->Suspend_AFE_MRGIF_MON2 = Afe_Get_Reg(AFE_MRGIF_MON2);

	pBackup_reg->Suspend_AFE_MOD_PCM_BASE = Afe_Get_Reg(AFE_MOD_PCM_BASE);
	pBackup_reg->Suspend_AFE_MOD_PCM_END = Afe_Get_Reg(AFE_MOD_PCM_END);
	pBackup_reg->Suspend_AFE_MOD_PCM_CUR = Afe_Get_Reg(AFE_MOD_PCM_CUR);

	pBackup_reg->Suspend_AFE_HDMI_OUT_CON0 = Afe_Get_Reg(AFE_HDMI_OUT_CON0);
	pBackup_reg->Suspend_AFE_HDMI_OUT_BASE = Afe_Get_Reg(AFE_HDMI_OUT_BASE);
	pBackup_reg->Suspend_AFE_HDMI_OUT_CUR = Afe_Get_Reg(AFE_HDMI_OUT_CUR);
	pBackup_reg->Suspend_AFE_HDMI_OUT_END = Afe_Get_Reg(AFE_HDMI_OUT_END);
	pBackup_reg->Suspend_AFE_SPDIF_OUT_CON0 = Afe_Get_Reg(AFE_SPDIF_OUT_CON0);
	pBackup_reg->Suspend_AFE_SPDIF_BASE = Afe_Get_Reg(AFE_SPDIF_BASE);
	pBackup_reg->Suspend_AFE_SPDIF_CUR = Afe_Get_Reg(AFE_SPDIF_CUR);
	pBackup_reg->Suspend_AFE_SPDIF_END = Afe_Get_Reg(AFE_SPDIF_END);
	pBackup_reg->Suspend_AFE_HDMI_CONN0 = Afe_Get_Reg(AFE_HDMI_CONN0);
	pBackup_reg->Suspend_AFE_8CH_I2S_OUT_CON = Afe_Get_Reg(AFE_8CH_I2S_OUT_CON);

	pBackup_reg->Suspend_AFE_IEC_CFG = Afe_Get_Reg(AFE_IEC_CFG);
	pBackup_reg->Suspend_AFE_IEC_NSNUM = Afe_Get_Reg(AFE_IEC_NSNUM);
	pBackup_reg->Suspend_AFE_IEC_BURST_INFO = Afe_Get_Reg(AFE_IEC_BURST_INFO);
	pBackup_reg->Suspend_AFE_IEC_BURST_LEN = Afe_Get_Reg(AFE_IEC_BURST_LEN);
	pBackup_reg->Suspend_AFE_IEC_NSADR = Afe_Get_Reg(AFE_IEC_NSADR);
	pBackup_reg->Suspend_AFE_IEC_CHL_STAT0 = Afe_Get_Reg(AFE_IEC_CHL_STAT0);
	pBackup_reg->Suspend_AFE_IEC_CHL_STAT1 = Afe_Get_Reg(AFE_IEC_CHL_STAT1);
	pBackup_reg->Suspend_AFE_IEC_CHR_STAT0 = Afe_Get_Reg(AFE_IEC_CHR_STAT0);
	pBackup_reg->Suspend_AFE_IEC_CHR_STAT1 = Afe_Get_Reg(AFE_IEC_CHR_STAT1);

	pBackup_reg->Suspend_AFE_IRQ_MCU_CON = Afe_Get_Reg(AFE_IRQ_MCU_CON);
	pBackup_reg->Suspend_AFE_IRQ_STATUS = Afe_Get_Reg(AFE_IRQ_STATUS);
	pBackup_reg->Suspend_AFE_IRQ_CLR = Afe_Get_Reg(AFE_IRQ_CLR);
	pBackup_reg->Suspend_AFE_IRQ_CNT1 = Afe_Get_Reg(AFE_IRQ_CNT1);
	pBackup_reg->Suspend_AFE_IRQ_CNT2 = Afe_Get_Reg(AFE_IRQ_CNT2);
	pBackup_reg->Suspend_AFE_IRQ_MON2 = Afe_Get_Reg(AFE_IRQ_MON2);
	pBackup_reg->Suspend_AFE_IRQ_CNT5 = Afe_Get_Reg(AFE_IRQ_CNT5);
	pBackup_reg->Suspend_AFE_IRQ1_CNT_MON = Afe_Get_Reg(AFE_IRQ1_CNT_MON);
	pBackup_reg->Suspend_AFE_IRQ2_CNT_MON = Afe_Get_Reg(AFE_IRQ2_CNT_MON);
	pBackup_reg->Suspend_AFE_IRQ1_EN_CNT_MON = Afe_Get_Reg(AFE_IRQ1_EN_CNT_MON);
	pBackup_reg->Suspend_AFE_IRQ5_EN_CNT_MON = Afe_Get_Reg(AFE_IRQ5_EN_CNT_MON);
	pBackup_reg->Suspend_AFE_MEMIF_MINLEN = Afe_Get_Reg(AFE_MEMIF_MINLEN);
	pBackup_reg->Suspend_AFE_MEMIF_MAXLEN = Afe_Get_Reg(AFE_MEMIF_MAXLEN);
	pBackup_reg->Suspend_AFE_MEMIF_PBUF_SIZE = Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE);

	pBackup_reg->Suspend_AFE_GAIN1_CON0 = Afe_Get_Reg(AFE_GAIN1_CON0);
	pBackup_reg->Suspend_AFE_GAIN1_CON1 = Afe_Get_Reg(AFE_GAIN1_CON1);
	pBackup_reg->Suspend_AFE_GAIN1_CON2 = Afe_Get_Reg(AFE_GAIN1_CON2);
	pBackup_reg->Suspend_AFE_GAIN1_CON3 = Afe_Get_Reg(AFE_GAIN1_CON3);
	pBackup_reg->Suspend_AFE_GAIN1_CONN = Afe_Get_Reg(AFE_GAIN1_CONN);
	pBackup_reg->Suspend_AFE_GAIN1_CUR = Afe_Get_Reg(AFE_GAIN1_CUR);
	pBackup_reg->Suspend_AFE_GAIN2_CON0 = Afe_Get_Reg(AFE_GAIN2_CON0);
	pBackup_reg->Suspend_AFE_GAIN2_CON1 = Afe_Get_Reg(AFE_GAIN2_CON1);
	pBackup_reg->Suspend_AFE_GAIN2_CON2 = Afe_Get_Reg(AFE_GAIN2_CON2);
	pBackup_reg->Suspend_AFE_GAIN2_CON3 = Afe_Get_Reg(AFE_GAIN2_CON3);
	pBackup_reg->Suspend_AFE_GAIN2_CONN = Afe_Get_Reg(AFE_GAIN2_CONN);

	pBackup_reg->Suspend_AFE_ASRC_CON0 = Afe_Get_Reg(AFE_ASRC_CON0);
	pBackup_reg->Suspend_AFE_ASRC_CON1 = Afe_Get_Reg(AFE_ASRC_CON1);
	pBackup_reg->Suspend_AFE_ASRC_CON2 = Afe_Get_Reg(AFE_ASRC_CON2);
	pBackup_reg->Suspend_AFE_ASRC_CON3 = Afe_Get_Reg(AFE_ASRC_CON3);
	pBackup_reg->Suspend_AFE_ASRC_CON4 = Afe_Get_Reg(AFE_ASRC_CON4);
	pBackup_reg->Suspend_AFE_ASRC_CON6 = Afe_Get_Reg(AFE_ASRC_CON6);
	pBackup_reg->Suspend_AFE_ASRC_CON7 = Afe_Get_Reg(AFE_ASRC_CON7);
	pBackup_reg->Suspend_AFE_ASRC_CON8 = Afe_Get_Reg(AFE_ASRC_CON8);
	pBackup_reg->Suspend_AFE_ASRC_CON9 = Afe_Get_Reg(AFE_ASRC_CON9);
	pBackup_reg->Suspend_AFE_ASRC_CON10 = Afe_Get_Reg(AFE_ASRC_CON10);
	pBackup_reg->Suspend_AFE_ASRC_CON11 = Afe_Get_Reg(AFE_ASRC_CON11);
	pBackup_reg->Suspend_PCM_INTF_CON1 = Afe_Get_Reg(PCM_INTF_CON1);
	pBackup_reg->Suspend_PCM_INTF_CON2 = Afe_Get_Reg(PCM_INTF_CON2);
	pBackup_reg->Suspend_PCM2_INTF_CON = Afe_Get_Reg(PCM2_INTF_CON);

	AudDrv_Clk_Off();
	pr_notice("-%s\n", __func__);
}
