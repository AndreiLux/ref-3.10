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

#ifndef _AUDIO_AFE_CONTROL_H
#define _AUDIO_AFE_CONTROL_H

#include "mt_soc_digital_type.h"
#include "AudDrv_Def.h"
#include "AudDrv_Kernel.h"
#include <mach/board-common-audio.h>

#define AUDDRV_DL1_MAX_BUFFER_LENGTH (0x4000)
#define MT8135_AFE_MCU_IRQ_LINE (104 + 32)
#define MASK_ALL          (0xFFFFFFFF)
#define AFE_MASK_ALL  (0xffffffff)

extern void *AFE_BASE_ADDRESS;
extern void *AFE_SRAM_ADDRESS;
extern struct AFE_MEM_CONTROL_T AFE_VUL_Control_context;
extern struct AFE_MEM_CONTROL_T AFE_DAI_Control_context;
extern struct AFE_MEM_CONTROL_T AFE_AWB_Control_context;

bool InitAfeControl(void);
bool ResetAfeControl(void);
int Register_Aud_Irq(void *dev);
void Auddrv_Reg_map(void);
void InitAudioGpioData(struct mt_audio_custom_gpio_data *gpio_data);

bool SetSampleRate(uint32_t Aud_block, uint32_t SampleRate);
bool SetChannels(uint32_t Memory_Interface, uint32_t channel);

bool SetIrqMcuCounter(uint32_t Irqmode, uint32_t Counter);
bool SetIrqEnable(uint32_t Irqmode, bool bEnable);
bool SetIrqMcuSampleRate(uint32_t Irqmode, uint32_t SampleRate);
bool GetIrqStatus(uint32_t Irqmode, struct AudioIrqMcuMode *Mcumode);

bool SetConnection(uint32_t ConnectionState, uint32_t Input, uint32_t Output);
bool SetMemoryPathEnable(uint32_t Aud_block, bool bEnable);
bool SetI2SDacEnable(bool bEnable);
bool GetI2SDacEnable(void);
void EnableAfe(bool bEnable);
bool Set2ndI2SOut(uint32_t samplerate);
bool SetI2SAdcIn(struct AudioDigtalI2S *DigtalI2S);
bool SetMrgI2SEnable(bool bEnable, unsigned int sampleRate);
bool SetI2SAdcEnable(bool bEnable);
bool Set2ndI2SEnable(bool bEnable);
bool SetI2SDacOut(uint32_t SampleRate);
bool SetHdmiPathEnable(bool bEnable);
bool SetHwDigitalGainMode(uint32_t GainType, uint32_t SampleRate, uint32_t SamplePerStep);
bool SetHwDigitalGainEnable(int GainType, bool Enable);
bool SetHwDigitalGain(uint32_t Gain, int GainType);

bool SetMemDuplicateWrite(uint32_t InterfaceType, int dupwrite);
bool EnableSideGenHw(uint32_t connection, bool direction, bool Enable);
void CleanPreDistortion(void);
bool EnableSideToneFilter(bool stf_on);
bool SetModemPcmEnable(int modem_index, bool modem_pcm_on);
bool SetModemPcmConfig(int modem_index, struct AudioDigitalPCM p_modem_pcm_attribute);

bool Set2ndI2SIn(struct AudioDigtalI2S *mDigitalI2S);
bool Set2ndI2SInConfig(unsigned int sampleRate, bool bIsSlaveMode);
bool Set2ndI2SInEnable(bool bEnable);

void SetDeepIdleEnableForAfe(bool enable);
void SetDeepIdleEnableForHdmi(bool enable);
void ResetFmChipMrgIf(void);
void SetDAIBTAttribute(void);
void SetDAIBTEnable(bool bEnable);
void DoAfeSuspend(void);
void DoAfeResume(void);
void AudDrv_Store_reg_AFE(struct AudAfe_Suspend_Reg *pBackup_reg);
void AudDrv_Recover_reg_AFE(struct AudAfe_Suspend_Reg *pBackup_reg);
void Auddrv_DL_SetPlatformInfo(enum Soc_Aud_Digital_Block digital_block,
			       struct snd_pcm_substream *substream);
void Auddrv_DL_ResetPlatformInfo(enum Soc_Aud_Digital_Block digital_block);
void Auddrv_ResetBuffer(enum Soc_Aud_Digital_Block digital_block);
int AudDrv_Allocate_DL1_Buffer(uint32_t Afe_Buf_Length);
int Auddrv_UpdateHwPtr(enum Soc_Aud_Digital_Block digital_block, struct snd_pcm_substream *substream);


#endif
