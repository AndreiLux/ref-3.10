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

#ifndef AUDIO_DEF_H
#define AUDIO_DEF_H

#define PM_MANAGER_API
#define AUDIO_MEMORY_SRAM
#define AUDIO_MEM_IOREMAP

/* below for audio debugging */
#define DEBUG_AUDDRV
#define DEBUG_AFE_REG
#define DEBUG_ANA_REG
#define DEBUG_AUD_CLK
/* #define DEEP_DEBUG_AUDDRV */

#ifdef DEEP_DEBUG_AUDDRV
#define PRINTK_AUDDEEPDRV(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AUDDEEPDRV(format, args...)
#endif

#ifdef DEBUG_AUDDRV
#define PRINTK_AUDDRV(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AUDDRV(format, args...)
#endif

#ifdef DEBUG_AFE_REG
#define PRINTK_AFE_REG(format, args...) pr_info(format, ##args)
#else
#define PRINTK_AFE_REG(format, args...)
#endif

#ifdef DEBUG_ANA_REG
#define PRINTK_ANA_REG(format, args...) pr_info(format, ##args)
#else
#define PRINTK_ANA_REG(format, args...)
#endif

#ifdef DEBUG_AUD_CLK
#define PRINTK_AUD_CLK(format, args...)  pr_info(format, ##args)
#else
#define PRINTK_AUD_CLK(format, args...)
#endif

#define PRINTK_AUD_ERROR(format, args...)  pr_err(format, ##args)

/* if need assert , use AUDIO_ASSERT(true) */
#define AUDIO_ASSERT(value) BUG_ON(false)

#define ENUM_TO_STR(enum) #enum

/**********************************
 *  Other Definitions             *
 **********************************/
#define BIT_00	0x00000001	/* ---- ---- ---- ---- ---- ---- ---- ---1 */
#define BIT_01	0x00000002	/* ---- ---- ---- ---- ---- ---- ---- --1- */
#define BIT_02	0x00000004	/* ---- ---- ---- ---- ---- ---- ---- -1-- */
#define BIT_03	0x00000008	/* ---- ---- ---- ---- ---- ---- ---- 1--- */
#define BIT_04	0x00000010	/* ---- ---- ---- ---- ---- ---- ---1 ---- */
#define BIT_05	0x00000020	/* ---- ---- ---- ---- ---- ---- --1- ---- */
#define BIT_06	0x00000040	/* ---- ---- ---- ---- ---- ---- -1-- ---- */
#define BIT_07	0x00000080	/* ---- ---- ---- ---- ---- ---- 1--- ---- */
#define BIT_08	0x00000100	/* ---- ---- ---- ---- ---- ---1 ---- ---- */
#define BIT_09	0x00000200	/* ---- ---- ---- ---- ---- --1- ---- ---- */
#define BIT_10	0x00000400	/* ---- ---- ---- ---- ---- -1-- ---- ---- */
#define BIT_11	0x00000800	/* ---- ---- ---- ---- ---- 1--- ---- ---- */
#define BIT_12	0x00001000	/* ---- ---- ---- ---- ---1 ---- ---- ---- */
#define BIT_13	0x00002000	/* ---- ---- ---- ---- --1- ---- ---- ---- */
#define BIT_14	0x00004000	/* ---- ---- ---- ---- -1-- ---- ---- ---- */
#define BIT_15	0x00008000	/* ---- ---- ---- ---- 1--- ---- ---- ---- */
#define BIT_16	0x00010000	/* ---- ---- ---- ---1 ---- ---- ---- ---- */
#define BIT_17	0x00020000	/* ---- ---- ---- --1- ---- ---- ---- ---- */
#define BIT_18	0x00040000	/* ---- ---- ---- -1-- ---- ---- ---- ---- */
#define BIT_19	0x00080000	/* ---- ---- ---- 1--- ---- ---- ---- ---- */
#define BIT_20	0x00100000	/* ---- ---- ---1 ---- ---- ---- ---- ---- */
#define BIT_21	0x00200000	/* ---- ---- --1- ---- ---- ---- ---- ---- */
#define BIT_22	0x00400000	/* ---- ---- -1-- ---- ---- ---- ---- ---- */
#define BIT_23	0x00800000	/* ---- ---- 1--- ---- ---- ---- ---- ---- */
#define BIT_24	0x01000000	/* ---- ---1 ---- ---- ---- ---- ---- ---- */
#define BIT_25	0x02000000	/* ---- --1- ---- ---- ---- ---- ---- ---- */
#define BIT_26	0x04000000	/* ---- -1-- ---- ---- ---- ---- ---- ---- */
#define BIT_27	0x08000000	/* ---- 1--- ---- ---- ---- ---- ---- ---- */
#define BIT_28	0x10000000	/* ---1 ---- ---- ---- ---- ---- ---- ---- */
#define BIT_29	0x20000000	/* --1- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_30	0x40000000	/* -1-- ---- ---- ---- ---- ---- ---- ---- */
#define BIT_31	0x80000000	/* 1--- ---- ---- ---- ---- ---- ---- ---- */
#define MASK_ALL          (0xFFFFFFFF)

#define MT_SOC_MACHINE_NAME "mt-soc-machine"
#define MT_SOC_DAI_NAME "snd-soc-dummy-dai"
#define MT_SOC_DL1_PCM   "mt-soc-dl1-pcm"
#define MT_SOC_UL1_PCM   "mt-soc-ul1-pcm"
#define MT_SOC_ROUTING_PCM  "mt-soc-routing-pcm"

#define MT_SOC_CODEC_TXDAI_NAME "mt-soc-codec-tx-dai"
#define MT_SOC_CODEC_RXDAI_NAME "mt-soc-codec-rx-dai"
#define MT_SOC_CODEC_PCMTXDAI_NAME "mt-soc-codec-pcmtx-dai"
#define MT_SOC_CODEC_PCMRXDAI_NAME "mt-soc-codec-pcmrx-dai"

#define MT_SOC_STUB_CPU_DAI "mt-soc-dai-driver"
#define MT_SOC_STUB_CPU_DAI_NAME "snd-soc-dummy-dai"
#define MT_SOC_CODEC_STUB_NAME "snd-soc-dummy"
#define MT_SOC_CODEC_NAME "mt-soc-codec"

#define MT_SOC_DL1_STREAM_NAME "MultiMedia1 PLayback"
#define MT_SOC_DL2_STREAM_NAME "MultiMedia2 PLayback"

#define MT_SOC_UL1_STREAM_NAME "MultiMedia1 Capture"
#define MT_SOC_UL2_STREAM_NAME "MultiMedia2 Capture"
#define MT_SOC_UL3_STREAM_NAME "MultiMedia3 Capture"
#define MT_SOC_UL4_STREAM_NAME "MultiMedia4 Capture"
#define MT_SOC_ROUTING_STREAM_NAME "MultiMedia Routing"

#define MT_SOC_HDMI_PLAYBACK_STREAM_NAME "HDMI Playback"
#define MT_SOC_HDMI_CPU_DAI_NAME "mt-audio-hdmi"
#define MT_SOC_HDMI_PLATFORM_NAME   "mt-soc-hdmi-pcm"
#define MT_SOC_HDMI_CODEC_DAI_NAME   "snd-soc-dummy-dai"

#define MT_SOC_BTSCO_STREAM_NAME        "BTSCO Stream"
#define MT_SOC_BTSCO_DL_STREAM_NAME     "BTSCO Playback Stream"
#define MT_SOC_BTSCO_UL_STREAM_NAME     "BTSCO Capture Stream"
#define MT_SOC_BTSCO_CPU_DAI_NAME "snd-soc-dummy-dai"
#define MT_SOC_BTSCO        "mt-soc-btsco-pcm"
#define MT_SOC_CODEC_BTSCODAI_NAME     "snd-soc-dummy-dai"

#define MT_SOC_DL1_AWB_NAME   "mt-soc-dl1-awb-pcm"
#define MT_SOC_DL1_AWB_STREAM_NAME     "DL1AWB Capture"
#define MT_SOC_DL1_AWB_CODEC_DAI_NAME     "snd-soc-dummy-dai"
#define MT_SOC_DL1_AWB_CPU_DAI_NAME    "mt-soc-dl1-awb-dai"


#endif
