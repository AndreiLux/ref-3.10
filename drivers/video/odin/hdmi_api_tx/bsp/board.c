/*
 * baord.c
 *
 *  Synopsys Inc.
 *  DWC PT02
 */
#include "board.h"
#include "access.h"
#include "../util/log.h"
#include "../util/error.h"

/* Logical register address. */
static const u8 drp_dcmrst = 0x00;
/* Logical register address. */
static const u8 drp_addr = 0x01;
/* Logical register address. */
static const u8 drp_datao_l = 0x02;
/* Logical register address. */
static const u8 drp_datao_h = 0x03;
/* data in low logical register address. */
static const u8 drp_datai_l = 0x04;
/* Logical register address. */
static const u8 drp_datai_h = 0x05;
/* Logical register address. */
static const u8 drp_cmd = 0x06;
/* Video data multiplexer 16-bit base address */
static const u16 video_mux_address = 0x7010;
/* Video DRP PLL controller 16-bit base address */
static const u16 video_pll_base_address = 0x7400;
/* Audio DRP PLL controller 16-bit base address */
static const u16 audio_pll_base_address = 0x7300;
/*  DRP PLL controller 16-bit nase address */
static const u16 tclk_pll_base_address = 0x7500;
/* Audio multiplexer 16-bit base address */
static const u16 audio_mux_address = 0x7106;

int board_initialize(u16 baseAddr, u16 pixelClock, u8 cd)
{
	unsigned i = 0;
	struct
	{
		const u16 addr;
		u8 data;
	} cfg[] =
	{
	/* reset */
	{ 0x7600, 0x00 },
	{ 0x7600, 0x01 },
	/* by default mask all interrupts */
	{ 0x0807, 0xFF }, /* VP */
	{ 0x10D2, 0xFF }, /* Packets */
	{ 0x10D6, 0xFF }, /* Packets */
	{ 0x10DA, 0xFF }, /* Packets */
	{ 0x3006, 0xFF }, /* PHY */
	{ 0x3027, 0xFF }, /* I2CM PHY */
	{ 0x3028, 0xFF }, /* I2CM PHY */
	{ 0x3102, 0xFF }, /* AS - I2S */
	{ 0x3302, 0xFF }, /* AS - SPDIF */
	{ 0x3404, 0xFF }, /* AS - HBR */
	{ 0x3506, 0xFF }, /* AS - GPA */
	{ 0x5008, 0xFF }, /* HDCP */
	{ 0x7D02, 0xFF }, /* CEC */
	{ 0x7E05, 0xFF }, /* I2C EDID */
	{ 0x7E06, 0xFF }, /* I2C EDID */
	};
	LOG_TRACE1(pixelClock);
	for (i = 0; i < (sizeof(cfg) / sizeof(cfg[0])); i++)
	{
		access_corewritebyte(cfg[i].data, cfg[i].addr);
	}
	/* Pull down the ZCALRST - for PHY GEN 2, on the HAPS51 DEMO BOARD */
	access_corewrite(0, 0x7900, 0, 1);
	return TRUE;
}

int board_audioclock(u16 baseAddr, u32 value)
{
	LOG_TRACE1(value);
	board_pllreset(audio_pll_base_address, 0x01);
	switch (value)
	{
		/* I2S (or SPDIF when DRU in bypass or GPA) 128 Factor, 32.0kHz Fs */
		case (128 * 32000):
			/* default */
			board_pllwrite(audio_pll_base_address, 0x0c, 0x1249);
			board_pllwrite(audio_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x1b, 0x1db6);
			board_pllwrite(audio_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x00, 0x0198);
			board_pllwrite(audio_pll_base_address, 0x01, 0x21df);
			board_pllwrite(audio_pll_base_address, 0x06, 0x3001);
			break;
		/* I2S (or SPDIF when DRU in bypass or GPA) 128 Factor, 48.0kHz Fs */
		case (128 * 48000):
			board_pllwrite(audio_pll_base_address, 0x0c, 0x128b);
			board_pllwrite(audio_pll_base_address, 0x0d, 0x0080);
			board_pllwrite(audio_pll_base_address, 0x1b, 0x1aaa);
			board_pllwrite(audio_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x00, 0x0298);
			board_pllwrite(audio_pll_base_address, 0x01, 0x21df);
			board_pllwrite(audio_pll_base_address, 0x06, 0x3001);
			break;
		/* I2S (or SPDIF when DRU in bypass or GPA) 128 Factor, 44.1kHz Fs */
		case (128 * 44100):
			board_pllwrite(audio_pll_base_address, 0x0c, 0x12cc);
			board_pllwrite(audio_pll_base_address, 0x0d, 0x0080);
			board_pllwrite(audio_pll_base_address, 0x1b, 0x1cb2);
			board_pllwrite(audio_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x00, 0x0218);
			board_pllwrite(audio_pll_base_address, 0x01, 0x21d1);
			board_pllwrite(audio_pll_base_address, 0x06, 0x3001);
			break;
		/* I2S (or SPDIF when DRU in bypass or GPA) 128 Factor, 88.2kHz Fs */
		case (128 * 88200):
		/* I2S (or SPDIF when DRU in bypass or GPA) 128 Factor, 96.0kHz Fs */
		case (128 * 96000):
			error_set(ERR_AUDIO_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("audio clock not supported", value);
			return FALSE;
			break;
		/* 512 Factor, 32.0kHz Fs*/
		case (512 * 32000):
			board_pllwrite(audio_pll_base_address, 0x0c, 0x128a);
			board_pllwrite(audio_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x1b, 0x13cf);
			board_pllwrite(audio_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(audio_pll_base_address, 0x00, 0x0218);
			board_pllwrite(audio_pll_base_address, 0x01, 0x21d1);
			board_pllwrite(audio_pll_base_address, 0x06, 0x3001);
			break;
		case (512 * 44100): /* SPDIF 44.1kHz or I2S 176.4kHz */
		case (512 * 48000): /* SPDIF 48.0kHz or I2S 192.0kHz */
		case (512 * 88200): /* SPDIF 88.2kHz */
		case (512 * 96000): /* SPDIF 96.0kHz */
		case (512 * 176400): /* SPDIF 176.4kHz */
		case (512 * 192000): /* SPDIF 192.0kHz */
		default:
			error_set(ERR_AUDIO_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("audio clock not supported", value);
			return FALSE;
	}
	board_pllreset(audio_pll_base_address, 0x01);
	board_pllreset(audio_pll_base_address, 0x00);
	return TRUE;
}

int board_pixelclock(u16 baseAddr, u16 value, u8 cd)
{
	LOG_TRACE1(value);
	board_pllreset(video_pll_base_address, 0x01);
	switch (value)
	{
		case 2520:
			/* default */
			board_pllwrite(video_pll_base_address, 0x0c, 0x1555);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1597);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0080);
			board_pllwrite(video_pll_base_address, 0x00, 0x0118);
			board_pllwrite(video_pll_base_address, 0x01, 0x21d4);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 2700:
			board_pllwrite(video_pll_base_address, 0x0c, 0x1249);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1249);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(video_pll_base_address, 0x00, 0x0198);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 5400:
			board_pllwrite(video_pll_base_address, 0x0c, 0x1249);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1105);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0080);
			board_pllwrite(video_pll_base_address, 0x00, 0x0198);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 7200:
			board_pllwrite(video_pll_base_address, 0x0c, 0x130c);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1105);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0080);
			board_pllwrite(video_pll_base_address, 0x00, 0x0298);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 7425:
			board_pllwrite(video_pll_base_address, 0x0c, 0x12cb);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1104);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(video_pll_base_address, 0x00, 0x0298);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 10800:
			board_pllwrite(video_pll_base_address, 0x0c, 0x1492);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1105);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0080);
			board_pllwrite(video_pll_base_address, 0x00, 0x0098);
			board_pllwrite(video_pll_base_address, 0x01, 0x21d7);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 14850:
			board_pllwrite(video_pll_base_address, 0x0c, 0x12cb);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1082);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(video_pll_base_address, 0x00, 0x0298);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		case 29700:
			board_pllwrite(video_pll_base_address, 0x0c, 0x12cb);
			board_pllwrite(video_pll_base_address, 0x0d, 0x0000);
			board_pllwrite(video_pll_base_address, 0x1b, 0x1041);
			board_pllwrite(video_pll_base_address, 0x1c, 0x0000);
			board_pllwrite(video_pll_base_address, 0x00, 0x0298);
			board_pllwrite(video_pll_base_address, 0x01, 0x21df);
			board_pllwrite(video_pll_base_address, 0x06, 0x3001);
			break;
		default:
			error_set(ERR_PIXEL_CLOCK_NOT_SUPPORTED);
			LOG_ERROR2("pixel clock not supported", value);
			return FALSE;
	}
	board_pllreset(video_pll_base_address, 0x01);
	board_pllreset(video_pll_base_address, 0x00);

	board_pllreset(tclk_pll_base_address, 0x01);
	if (((value * cd) / 8) < 7425)
	{ /* tclk N = 10, M = 10 */
		board_pllwrite(tclk_pll_base_address, 0x0c, 0x1145);
		board_pllwrite(tclk_pll_base_address, 0x0d, 0x0000);
		board_pllwrite(tclk_pll_base_address, 0x1b, 0x9145);
		board_pllwrite(tclk_pll_base_address, 0x1c, 0x0002);
		board_pllwrite(tclk_pll_base_address, 0x00, 0x0118);
		board_pllwrite(tclk_pll_base_address, 0x01, 0x21D1);
		board_pllwrite(tclk_pll_base_address, 0x06, 0x3001);
	}
	else if (((value * cd) / 8) > 14850)
	{ /* tclk N = 3, M = 3 */
		board_pllwrite(tclk_pll_base_address, 0x0c, 0x1042);
		board_pllwrite(tclk_pll_base_address, 0x0d, 0x0080);
		board_pllwrite(tclk_pll_base_address, 0x1b, 0xD042);
		board_pllwrite(tclk_pll_base_address, 0x1c, 0x0080);
		board_pllwrite(tclk_pll_base_address, 0x00, 0x0198);
		board_pllwrite(tclk_pll_base_address, 0x01, 0x21D1);
		board_pllwrite(tclk_pll_base_address, 0x06, 0x3001);
		board_pllreset(tclk_pll_base_address, 0x01);
		board_pllreset(tclk_pll_base_address, 0x00);
	}
	else
	{   /* tclk N = 7, M = 7 */
		board_pllwrite(tclk_pll_base_address, 0x0c, 0x10c4);
		board_pllwrite(tclk_pll_base_address, 0x0d, 0x0080);
		board_pllwrite(tclk_pll_base_address, 0x1b, 0x10c4);
		board_pllwrite(tclk_pll_base_address, 0x1c, 0x0080);
		board_pllwrite(tclk_pll_base_address, 0x00, 0x00d8);
		board_pllwrite(tclk_pll_base_address, 0x01, 0x21df);
		board_pllwrite(tclk_pll_base_address, 0x06, 0x3001);
	}
	board_pllreset(tclk_pll_base_address, 0x01);
	board_pllreset(tclk_pll_base_address, 0x00);

	while (access_corereadbyte(video_pll_base_address + 0x08) == 0)
		;
	return TRUE;
}

void board_pllwrite(u16 pllBaseAddress, u8 regAddr, u16 data)
{
	access_corewritebyte(0x00, pllBaseAddress + drp_cmd);
	access_corewritebyte(regAddr, pllBaseAddress + drp_addr);
	access_corewritebyte((u8) (data >> 8), pllBaseAddress + drp_datai_h);
	access_corewritebyte((u8) (data >> 0), pllBaseAddress + drp_datai_l);
	access_corewritebyte(0x01, pllBaseAddress + drp_cmd);
	access_corewritebyte(0x00, pllBaseAddress + drp_cmd);
}

void board_videogeneratorbypass(u16 baseAddr, u8 enable)
{
	access_corewritebyte(enable ? 0x01 : 0x00, video_mux_address);
}

u16 board_pllread(u16 pllBaseAddress, u8 regAddr)
{
	access_corewritebyte(0x00, pllBaseAddress + drp_cmd); /* drp_cmd 0x0 */
	/* drp_addr REG_ADDR */
	access_corewritebyte(regAddr, pllBaseAddress + drp_addr);
	access_corewritebyte(0x02, pllBaseAddress + drp_cmd); /* drp_cmd 0x2 */
	access_corewritebyte(0x00, pllBaseAddress + drp_cmd); /* drp_cmd 0x0 */
	return (access_corereadbyte(pllBaseAddress + drp_datao_h) << 8)
			| access_corereadbyte(pllBaseAddress + drp_datao_l);
}

void board_pllreset(u16 pllBaseAddress, u8 value)
{
	access_corewritebyte(value, pllBaseAddress + drp_dcmrst); /* drp_dcmrst 0x1 */
}

void board_audiogeneratorbypass(u16 baseAddr, u8 enable)
{
	access_corewritebyte(enable ? 0x01 : 0x00, audio_mux_address);
}

u32 board_supportedrefreshrate(u8 code)
{
	LOG_TRACE1(code);
	/* 	the following values are specific to the compliance board
		Board No.:
		PLL Board No.:
	 */
	switch (code)
	{
		case 1:
			return 60000;
		case 2:
		case 3:
			return 59940;
		case 4:
		case 5:
			return 60000;
		case 6:
		case 7:
			return 59940;
		case 8:
		case 9:
			return 60054;
		case 10:
		case 11:
			return 59940;
		case 12:
		case 13:
			return 60054;
		case 14:
		case 15:
			return 59940;
		case 16:
			return 60000;
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
			return 50000;
		case 32:
			return 24000;
		case 33:
			return 25000;
		case 34:
			return 30000;
		case 35:
		case 36:
			return 59940;
		case 37:
		case 38:
			return 50000;
		case 39:
			return 50000;
		case 40:
		case 41:
		case 42:
		case 43:
			return 100000;
		case 44:
		case 45:
			return 50000;
		case 46:
		case 47:
			return 120000;
		case 48:
		case 49:
		case 50:
		case 51:
			return 119880;
			break;
		case 52:
		case 53:
		case 54:
		case 55:
			return 200000;
		case 56:
		case 57:
		case 58:
		case 59:
			return 239760;
		case 60:
			return 24000;
		case 61:
			return 25000;
		case 62:
			return 30000;
		case 63:
			return 120000;
		case 64:
			return 100000;
		default:
			break;
	}
	return -1;
}
