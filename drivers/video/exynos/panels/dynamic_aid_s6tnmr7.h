#ifndef __DYNAMIC_AID_XXXX_H
#define __DYNAMIC_AID_XXXX_H __FILE__

#include "dynamic_aid.h"
#include "dynamic_aid_gamma_curve.h"

enum {
	IV_VT,
	IV_3,
	IV_11,
	IV_23,
	IV_35,
	IV_51,
	IV_87,
	IV_151,
	IV_203,
	IV_255,
	IV_MAX,
};

enum {
	IBRIGHTNESS_2NT,
	IBRIGHTNESS_3NT,
	IBRIGHTNESS_4NT,
	IBRIGHTNESS_5NT,
	IBRIGHTNESS_6NT,
	IBRIGHTNESS_7NT,
	IBRIGHTNESS_8NT,
	IBRIGHTNESS_9NT,
	IBRIGHTNESS_10NT,
	IBRIGHTNESS_11NT,
	IBRIGHTNESS_12NT,
	IBRIGHTNESS_13NT,
	IBRIGHTNESS_14NT,
	IBRIGHTNESS_15NT,
	IBRIGHTNESS_16NT,
	IBRIGHTNESS_17NT,
	IBRIGHTNESS_19NT,
	IBRIGHTNESS_20NT,
	IBRIGHTNESS_21NT,
	IBRIGHTNESS_22NT,
	IBRIGHTNESS_24NT,
	IBRIGHTNESS_25NT,
	IBRIGHTNESS_27NT,
	IBRIGHTNESS_29NT,
	IBRIGHTNESS_30NT,
	IBRIGHTNESS_32NT,
	IBRIGHTNESS_34NT,
	IBRIGHTNESS_37NT,
	IBRIGHTNESS_39NT,
	IBRIGHTNESS_41NT,
	IBRIGHTNESS_44NT,
	IBRIGHTNESS_47NT,
	IBRIGHTNESS_50NT,
	IBRIGHTNESS_53NT,
	IBRIGHTNESS_56NT,
	IBRIGHTNESS_60NT,
	IBRIGHTNESS_64NT,
	IBRIGHTNESS_68NT,
	IBRIGHTNESS_72NT,
	IBRIGHTNESS_77NT,
	IBRIGHTNESS_82NT,
	IBRIGHTNESS_87NT,
	IBRIGHTNESS_93NT,
	IBRIGHTNESS_98NT,
	IBRIGHTNESS_105NT,
	IBRIGHTNESS_111NT,
	IBRIGHTNESS_119NT,
	IBRIGHTNESS_126NT,
	IBRIGHTNESS_134NT,
	IBRIGHTNESS_143NT,
	IBRIGHTNESS_152NT,
	IBRIGHTNESS_162NT,
	IBRIGHTNESS_172NT,
	IBRIGHTNESS_183NT,
	IBRIGHTNESS_195NT,
	IBRIGHTNESS_207NT,
	IBRIGHTNESS_220NT,
	IBRIGHTNESS_234NT,
	IBRIGHTNESS_249NT,
	IBRIGHTNESS_265NT,
	IBRIGHTNESS_282NT,
	IBRIGHTNESS_300NT,
	IBRIGHTNESS_HBM,
	IBRIGHTNESS_MAX
};

#define VREG_OUT_X1000		6400	/* VREG_OUT x 1000 */

static const int index_voltage_table[IBRIGHTNESS_MAX] =
{
	0,		/* IV_VT */
	3,		/* IV_3 */
	11,		/* IV_11 */
	23,		/* IV_23 */
	35,		/* IV_35 */
	51,		/* IV_51 */
	87,		/* IV_87 */
	151,	/* IV_151 */
	203,	/* IV_203 */
	255		/* IV_255 */
};

static const int index_brightness_table[IBRIGHTNESS_MAX] =
{
	2,	/* IBRIGHTNESS_2NT */
	3,	/* IBRIGHTNESS_3NT */
	4,	/* IBRIGHTNESS_4NT */
	5,	/* IBRIGHTNESS_5NT */
	6,	/* IBRIGHTNESS_6NT */
	7,	/* IBRIGHTNESS_7NT */
	8,	/* IBRIGHTNESS_8NT */
	9,	/* IBRIGHTNESS_9NT */
	10,	/* IBRIGHTNESS_10NT */
	11,	/* IBRIGHTNESS_11NT */
	12,	/* IBRIGHTNESS_12NT */
	13,	/* IBRIGHTNESS_13NT */
	14,	/* IBRIGHTNESS_14NT */
	15,	/* IBRIGHTNESS_15NT */
	16,	/* IBRIGHTNESS_16NT */
	17,	/* IBRIGHTNESS_17NT */
	19,	/* IBRIGHTNESS_19NT */
	20,	/* IBRIGHTNESS_20NT */
	21,	/* IBRIGHTNESS_21NT */
	22,	/* IBRIGHTNESS_22NT */
	24,	/* IBRIGHTNESS_24NT */
	25,	/* IBRIGHTNESS_25NT */
	27,	/* IBRIGHTNESS_27NT */
	29,	/* IBRIGHTNESS_29NT */
	30,	/* IBRIGHTNESS_30NT */
	32,	/* IBRIGHTNESS_32NT */
	34,	/* IBRIGHTNESS_34NT */
	37,	/* IBRIGHTNESS_37NT */
	39,	/* IBRIGHTNESS_39NT */
	41,	/* IBRIGHTNESS_41NT */
	44,	/* IBRIGHTNESS_44NT */
	47,	/* IBRIGHTNESS_47NT */
	50,	/* IBRIGHTNESS_50NT */
	53,	/* IBRIGHTNESS_53NT */
	56,	/* IBRIGHTNESS_56NT */
	60,	/* IBRIGHTNESS_60NT */
	64,	/* IBRIGHTNESS_64NT */
	68,	/* IBRIGHTNESS_68NT */
	72,	/* IBRIGHTNESS_72NT */
	77,	/* IBRIGHTNESS_77NT */
	82,	/* IBRIGHTNESS_82NT */
	87,	/* IBRIGHTNESS_87NT */
	93,	/* IBRIGHTNESS_93NT */
	98,	/* IBRIGHTNESS_98NT */
	105,	/* IBRIGHTNESS_105NT */
	111,	/* IBRIGHTNESS_111NT */
	119,	/* IBRIGHTNESS_119NT */
	126,	/* IBRIGHTNESS_126NT */
	134,	/* IBRIGHTNESS_134NT */
	143,	/* IBRIGHTNESS_143NT */
	152,	/* IBRIGHTNESS_152NT */
	162,	/* IBRIGHTNESS_162NT */
	172,	/* IBRIGHTNESS_172NT */
	183,	/* IBRIGHTNESS_183NT */
	195,	/* IBRIGHTNESS_195NT */
	207,	/* IBRIGHTNESS_207NT */
	220,	/* IBRIGHTNESS_220NT */
	234,	/* IBRIGHTNESS_234NT */
	249,	/* IBRIGHTNESS_249NT */
	265,	/* IBRIGHTNESS_265NT */
	282,	/* IBRIGHTNESS_282NT */
	300,	/* IBRIGHTNESS_300NT */
};

static const int gamma_default_0[IV_MAX*CI_MAX] =
{
	0x00, 0x00, 0x00,		/* IV_VT */
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x100, 0x100, 0x100,	/* IV_255 */
};

static const int *gamma_default = gamma_default_0;

static const struct formular_t gamma_formula[IV_MAX] =
{
	{0, 600},	/* IV_VT */
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{64, 320},
	{129, 640},	/* IV_255 */
};

static const int vt_voltage_value[] =
{0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186};

static const int vregout_voltage_table[IBRIGHTNESS_MAX] =
{
	6520, /* VREG_OUT x 1000 */
	261000 /* VREFH_OUT x 100000 */
};
static const int *pvregout_voltage_table = vregout_voltage_table;

static const int brightness_base_table[IBRIGHTNESS_MAX] =
{
	110,  /*IBRIGHTNESS_2NT */
	110,  /*IBRIGHTNESS_3NT */
	110,  /*IBRIGHTNESS_4NT */
	110,  /*IBRIGHTNESS_5NT */
	110,  /*IBRIGHTNESS_6NT */
	110,  /*IBRIGHTNESS_7NT */
	110,  /*IBRIGHTNESS_8NT */
	110,  /*IBRIGHTNESS_9NT */
	110,  /*IBRIGHTNESS_10NT */
	110,  /*IBRIGHTNESS_11NT */
	110,  /*IBRIGHTNESS_12NT */
	110,  /*IBRIGHTNESS_13NT */
	110,  /*IBRIGHTNESS_14NT */
	110,  /*IBRIGHTNESS_15NT */
	110,  /*IBRIGHTNESS_16NT */
	110,  /*IBRIGHTNESS_17NT */
	110,  /*IBRIGHTNESS_19NT */
	110,  /*IBRIGHTNESS_20NT */
	110,  /*IBRIGHTNESS_21NT */
	110,  /*IBRIGHTNESS_22NT */
	110,  /*IBRIGHTNESS_24NT */
	110,  /*IBRIGHTNESS_25NT */
	110,  /*IBRIGHTNESS_27NT */
	110,  /*IBRIGHTNESS_29NT */
	110,  /*IBRIGHTNESS_30NT */
	110,  /*IBRIGHTNESS_32NT */
	110,  /*IBRIGHTNESS_34NT */
	110,  /*IBRIGHTNESS_37NT */
	110,  /*IBRIGHTNESS_39NT */
	110,  /*IBRIGHTNESS_41NT */
	110,  /*IBRIGHTNESS_44NT */
	110,  /*IBRIGHTNESS_47NT */
	110,  /*IBRIGHTNESS_50NT */
	110,  /*IBRIGHTNESS_53NT */
	110,  /*IBRIGHTNESS_56NT */
	110,  /*IBRIGHTNESS_60NT */
	110,  /*IBRIGHTNESS_64NT */
	114,  /*IBRIGHTNESS_68NT */
	120,	/* IBRIGHTNESS_72NT */
	128,	/* IBRIGHTNESS_77NT */
	134,	/* IBRIGHTNESS_82NT */
	142,	/* IBRIGHTNESS_87NT */
	152,	/* IBRIGHTNESS_93NT */
	159,	/* IBRIGHTNESS_98NT */
	170,	/* IBRIGHTNESS_105NT */
	178,	/* IBRIGHTNESS_111NT */
	191,	/* IBRIGHTNESS_119NT */
	202,	/* IBRIGHTNESS_126NT */
	214,	/* IBRIGHTNESS_134NT */
	226,	/* IBRIGHTNESS_143NT */
	238,	/* IBRIGHTNESS_152NT */
	238,	/* IBRIGHTNESS_162NT */
	238,	/* IBRIGHTNESS_172NT */
	238,	/* IBRIGHTNESS_183NT */
	238,	/* IBRIGHTNESS_195NT */
	238,	/* IBRIGHTNESS_207NT */
	238,	/* IBRIGHTNESS_220NT */
	238,	/* IBRIGHTNESS_234NT */
	254,	/* IBRIGHTNESS_249NT */
	268,	/* IBRIGHTNESS_265NT */
	285,	/* IBRIGHTNESS_282NT */
	300,  /*IBRIGHTNESS_300NT */
};
const int *pbrightness_base_table = brightness_base_table;


static const int *gamma_curve_tables[IBRIGHTNESS_MAX] =
{
	gamma_curve_2p15_table, /* IBRIGHTNESS_2NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_3NT */
	gamma_curve_2p15_table, /* IBRIGHTNESS_4NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_5NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_6NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_7NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_8NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_9NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_10NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_11NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_12NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_13NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_14NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_15NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_16NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_17NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_19NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_20NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_21NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_22NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_24NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_25NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_27NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_29NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_30NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_32NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_34NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_37NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_39NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_41NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_44NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_47NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_50NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_53NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_56NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_60NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_64NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_68NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_72NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_77NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_82NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_87NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_93NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_98NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_105NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_111NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_119NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_126NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_134NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_143NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_152NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_162NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_172NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_183NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_195NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_207NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_220NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_234NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_249NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_265NT */
	gamma_curve_2p15_table,	/* IBRIGHTNESS_282NT */
	gamma_curve_2p20_table,	/* IBRIGHTNESS_300NT */

};

static const int *gamma_curve_lut = gamma_curve_2p20_table;

static const unsigned char aor_cmd[IBRIGHTNESS_MAX][3] = {
	{0x85, 0x93, 0x01},  /* IBRIGHTNESS_2NT */
	{0x85, 0x90, 0x01},	/* IBRIGHTNESS_3NT */
	{0x85, 0x8C, 0x01},	/* IBRIGHTNESS_4NT */
	{0x85, 0x88, 0x01},	/* IBRIGHTNESS_5NT */
	{0x85, 0x85, 0x01},	/* IBRIGHTNESS_6NT */
	{0x85, 0x81, 0x01},	/* IBRIGHTNESS_7NT */
	{0x85, 0x7D, 0x01},	/* IBRIGHTNESS_8NT */
	{0x85, 0x7B, 0x01},	/* IBRIGHTNESS_9NT */
	{0x85, 0x77, 0x01},	/* IBRIGHTNESS_10NT */
	{0x85, 0x73, 0x01},	/* IBRIGHTNESS_11NT */
	{0x85, 0x70, 0x01},	/* IBRIGHTNESS_12NT */
	{0x85, 0x6D, 0x01},	/* IBRIGHTNESS_13NT */
	{0x85, 0x69, 0x01},	/* IBRIGHTNESS_14NT */
	{0x85, 0x66, 0x01},	/* IBRIGHTNESS_15NT */
	{0x85, 0x63, 0x01},	/* IBRIGHTNESS_16NT */
	{0x85, 0x5F, 0x01},	/* IBRIGHTNESS_17NT */
	{0x85, 0x59, 0x01},	/* IBRIGHTNESS_19NT */
	{0x85, 0x56, 0x01},	/* IBRIGHTNESS_20NT */
	{0x85, 0x52, 0x01},	/* IBRIGHTNESS_21NT */
	{0x85, 0x4E, 0x01},	/* IBRIGHTNESS_22NT */
	{0x85, 0x47, 0x01},	/* IBRIGHTNESS_24NT */
	{0x85, 0x43, 0x01},	/* IBRIGHTNESS_25NT */
	{0x85, 0x3C, 0x01},	/* IBRIGHTNESS_27NT */
	{0x85, 0x35, 0x01},	/* IBRIGHTNESS_29NT */
	{0x85, 0x32, 0x01},	/* IBRIGHTNESS_30NT */
	{0x85, 0x2C, 0x01},	/* IBRIGHTNESS_32NT */
	{0x85, 0x23, 0x01},	/* IBRIGHTNESS_34NT */
	{0x85, 0x19, 0x01},	/* IBRIGHTNESS_37NT */
	{0x85, 0x11, 0x01},	/* IBRIGHTNESS_39NT */
	{0x85, 0x09, 0x01},	/* IBRIGHTNESS_41NT */
	{0x85, 0xFE, 0x00},	/* IBRIGHTNESS_44NT */
	{0x85, 0xF3, 0x00},	/* IBRIGHTNESS_47NT */
	{0x85, 0xE8, 0x00},	/* IBRIGHTNESS_50NT */
	{0x85, 0xDE, 0x00},	/* IBRIGHTNESS_53NT */
	{0x85, 0xD2, 0x00},	/* IBRIGHTNESS_56NT */
	{0x85, 0xc2, 0x00},	/* IBRIGHTNESS_60NT */
	{0x85, 0xb1, 0x00},	/* IBRIGHTNESS_64NT */
	{0x85, 0xa7, 0x00},	/* IBRIGHTNESS_68~143NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_72NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_77NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_82NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_87NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_93NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_98NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_105NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_111NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_119NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_126NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_134NT */
	{0x85, 0xa7, 0x00}, /* IBRIGHTNESS_143NT */
	{0x85, 0xA7, 0x00},	/* IBRIGHTNESS_152NT */
	{0x85, 0x96, 0x00},	/* IBRIGHTNESS_162NT */
	{0x85, 0x81, 0x00},	/* IBRIGHTNESS_172NT */
	{0x85, 0x6D, 0x00},	/* IBRIGHTNESS_183NT */
	{0x85, 0x56, 0x00},	/* IBRIGHTNESS_195NT */
	{0x85, 0x3c, 0x00},	/* IBRIGHTNESS_207NT */
	{0x85, 0x1F, 0x00},	/* IBRIGHTNESS_220NT */
	{0x85, 0x06, 0x00}, /* IBRIGHTNESS_234NT */
	{0x85, 0x06, 0x00}, /* IBRIGHTNESS_249NT */
	{0x85, 0x06, 0x00}, /* IBRIGHTNESS_265NT */
	{0x85, 0x06, 0x00}, /* IBRIGHTNESS_282NT */
	{0x85, 0x06, 0x00} /* IBRIGHTNESS_300NT */
};
const unsigned char (*paor_cmd)[3] = aor_cmd;

static const int offset_gradation[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{0, 38, 37, 36, 35, 33, 27, 19, 10, 0},    /* IBRIGHTNESS_2NT */
	{0, 32, 31, 30, 27, 25, 20, 13, 7, -2},    /* IBRIGHTNESS_3NT */
	{0, 25, 24, 24, 22, 21, 17, 11, 6, -2},    /* IBRIGHTNESS_4NT */
	{0, 23, 22, 21, 20, 18, 15, 10, 6, -2},    /* IBRIGHTNESS_5NT */
	{0, 22, 21, 20, 19, 17, 14, 8, 5, -2},	  /* IBRIGHTNESS_6NT */
	{0, 21, 20, 19, 17, 15, 12, 7, 5, -2},	  /* IBRIGHTNESS_7NT */
	{0, 20, 19, 17, 16, 13, 11, 7, 5, -2},	  /* IBRIGHTNESS_8NT */
	{0, 19, 18, 17, 15, 12, 10, 7, 6, -2},	  /* IBRIGHTNESS_9NT */
	{0, 17, 16, 15, 13, 11, 9, 7, 6, -1},	 /* IBRIGHTNESS_10NT */
	{0, 16, 15, 14, 13, 11, 9, 7, 5, -1},	 /* IBRIGHTNESS_11NT */
	{0, 15, 14, 13, 11, 10, 7, 6, 5, -1},	 /* IBRIGHTNESS_12NT */
	{0, 14, 14, 12, 11, 10, 7, 6, 5, 0},	/* IBRIGHTNESS_13NT */
	{0, 14, 13, 12, 10, 10, 7, 6, 5, 0},	/* IBRIGHTNESS_14NT */
	{0, 13, 13, 11, 10, 8, 7, 6, 5, 0},    /* IBRIGHTNESS_15NT */
	{0, 12, 12, 11, 9, 8, 7, 6, 5, 0},	  /* IBRIGHTNESS_16NT */
	{0, 12, 12, 10, 9, 8, 7, 6, 5, 0},	  /* IBRIGHTNESS_17NT */
	{0, 11, 11, 9, 8, 7, 6, 5, 4, 0},	 /* IBRIGHTNESS_19NT */
	{0, 11, 11, 9, 8, 6, 6, 5, 4, 0},	 /* IBRIGHTNESS_20NT */
	{0, 10, 10, 9, 8, 6, 6, 5, 4, 0},	 /* IBRIGHTNESS_21NT */
	{0, 10, 10, 8, 7, 6, 5, 5, 4, 0},	 /* IBRIGHTNESS_22NT */
	{0, 9, 9, 8, 6, 5, 5, 5, 4, 0},    /* IBRIGHTNESS_24NT */
	{0, 9, 9, 7, 6, 5, 5, 5, 4, 0},    /* IBRIGHTNESS_25NT */
	{0, 9, 9, 7, 6, 5, 5, 5, 4, 0},    /* IBRIGHTNESS_27NT */
	{0, 8, 8, 7, 6, 4, 4, 4, 4, 0},    /* IBRIGHTNESS_29NT */
	{0, 8, 8, 7, 6, 4, 4, 4, 4, 0},    /* IBRIGHTNESS_30NT */
	{0, 8, 8, 6, 5, 4, 4, 4, 4, 0},    /* IBRIGHTNESS_32NT */
	{0, 7, 7, 6, 5, 4, 4, 4, 4, 0},    /* IBRIGHTNESS_34NT */
	{0, 6, 6, 5, 5, 4, 4, 4, 4, 0},    /* IBRIGHTNESS_37NT */
	{0, 6, 6, 5, 4, 3, 3, 4, 4, 0},    /* IBRIGHTNESS_39NT */
	{0, 6, 6, 5, 4, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_41NT */
	{0, 6, 6, 5, 4, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_44NT */
	{0, 5, 5, 4, 4, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_47NT */
	{0, 5, 5, 4, 3, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_50NT */
	{0, 5, 5, 4, 3, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_53NT */
	{0, 5, 5, 4, 3, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_56NT */
	{0, 4, 4, 4, 3, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_60NT */
	{0, 4, 4, 3, 3, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_64NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_68NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_72NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_77NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_82NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_87NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_93NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_98NT */
	{0, 4, 3, 3, 3, 2, 3, 4, 2, 0},    /* IBRIGHTNESS_105NT */
	{0, 4, 4, 3, 3, 3, 3, 4, 3, 0},    /* IBRIGHTNESS_111NT */
	{0, 3, 3, 2, 3, 3, 3, 4, 3, 0},    /* IBRIGHTNESS_119NT */
	{0, 3, 3, 2, 3, 3, 3, 4, 3, 0},    /* IBRIGHTNESS_126NT */
	{0, 3, 3, 2, 3, 3, 3, 4, 3, 0},    /* IBRIGHTNESS_134NT */
	{0, 3, 3, 2, 2, 2, 3, 4, 3, 0},    /* IBRIGHTNESS_143NT */
	{0, 3, 3, 2, 2, 2, 3, 3, 2, 0},    /* IBRIGHTNESS_152NT */
	{0, 3, 3, 2, 1, 2, 3, 3, 2, 0},    /* IBRIGHTNESS_162NT */
	{0, 2, 2, 1, 1, 2, 2, 3, 2, 0},    /* IBRIGHTNESS_172NT */
	{0, 2, 2, 1, 1, 2, 2, 3, 2, 0},    /* IBRIGHTNESS_183NT */
	{0, 2, 1, 1, 1, 1, 2, 3, 2, 0},    /* IBRIGHTNESS_195NT */
	{0, 2, 1, 1, 1, 1, 2, 2, 1, 0},    /* IBRIGHTNESS_207NT */
	{0,1,1,1,0,0,1,2,1,0},	 /*IBRIGHTNESS_220NT  */
	{0, 0, 1, 1, 1, 1, 1, 1, 0, 0},    /* IBRIGHTNESS_234NT */
	{0,0,1,0,0,0,1,1,0,0},	 /*IBRIGHTNESS_249NT  */
	{0,0,1,1,-1,0,0,1,0,0},   /*IBRIGHTNESS_265NT  */
	{0,0,0,-1,-1,-1,-1,1,1,0},	 /*IBRIGHTNESS_282NT  */
	{0,0,0,0,0,0,0,0,0,0}	 /*IBRIGHTNESS_300NT  */
};
const int (*poffset_gradation)[IV_MAX] = offset_gradation;

static const struct rgb_t offset_color[IBRIGHTNESS_MAX][IV_MAX] =
{	/* V0 ~ V255 */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-5, 4, -10}},{{-10, 4, -10}},{{-15, 4, -9}},{{-15, 4, -9}},{{-6, 3, -5}},{{-2, 2, -3}},{{-7, 3, -5}}}, /* IBRIGHTNESS_2NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 4, -9}},{{-11, 3, -9}},{{-16, 3, -9}},{{-13, 5, -9}},{{-4, 3, -4}},{{-2, 1, -3}},{{-6, 0, -5}}}, /* IBRIGHTNESS_3NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 4, -9}},{{-11, 3, -8}},{{-14, 3, -9}},{{-10, 4, -9}},{{-3, 3, -3}},{{0, 1, -1}},{{-4, 0, -3}}}, /* IBRIGHTNESS_4NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 4, -8}},{{-11, 2, -7}},{{-14, 2, -9}},{{-9, 4, -8}},{{-2, 3, -3}},{{0, 0, -1}},{{-3, 0, -2}}}, /* IBRIGHTNESS_5NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 3, -9}},{{-11, 2, -7}},{{-12, 2, -8}},{{-9, 4, -8}},{{-2, 3, -2}},{{0, 0, -1}},{{-3, 0, -2}}}, /* IBRIGHTNESS_6NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 3, -8}},{{-11, 2, -7}},{{-12, 2, -8}},{{-9, 3, -8}},{{-2, 2, -2}},{{0, 0, -1}},{{-2, 0, -1}}}, /* IBRIGHTNESS_7NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 2, -8}},{{-11, 2, -7}},{{-11, 2, -8}},{{-7, 2, -7}},{{-1, 2, -2}},{{0, 0, -1}},{{-2, 0, 0}}}, /* IBRIGHTNESS_8NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 2, -8}},{{-11, 2, -7}},{{-11, 2, -8}},{{-7, 2, -7}},{{-1, 2, -2}},{{0, 0, -1}},{{-2, 0, 0}}}, /* IBRIGHTNESS_9NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-6, 2, -8}},{{-11, 2, -7}},{{-11, 2, -8}},{{-7, 2, -6}},{{-1, 2, -2}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_10NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-9, 2, -8}},{{-11, 2, -7}},{{-11, 2, -8}},{{-7, 2, -6}},{{-1, 2, -2}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_11NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-9, 3, -8}},{{-11, 3, -7}},{{-11, 2, -8}},{{-7, 2, -6}},{{-1, 2, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_12NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 4, -12}},{{-9, 3, -8}},{{-11, 3, -7}},{{-11, 2, -8}},{{-6, 2, -5}},{{-1, 2, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_13NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 6, -12}},{{-9, 3, -8}},{{-11, 3, -7}},{{-11, 2, -8}},{{-5, 2, -4}},{{-1, 1, -1}},{{0, 0, 0}},{{-1, 0, 0}}}, /* IBRIGHTNESS_14NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-11, 2, -8}},{{-5, 2, -4}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_15NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-11, 2, -7}},{{-5, 2, -4}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_16NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-11, 2, -7}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_17NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-11, 2, -7}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_19NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-11, 2, -7}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_20NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -16}},{{-9, 3, -8}},{{-10, 3, -7}},{{-10, 2, -6}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_21NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -15}},{{-9, 3, -8}},{{-10, 3, -7}},{{-9, 2, -5}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_22NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -15}},{{-8, 3, -8}},{{-10, 3, -7}},{{-8, 2, -4}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_24NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -13}},{{-8, 3, -8}},{{-8, 3, -6}},{{-6, 2, -3}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_25NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -13}},{{-7, 3, -7}},{{-7, 3, -6}},{{-6, 2, -3}},{{-4, 1, -3}},{{-1, 1, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_27NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -15}},{{-5, 3, -6}},{{-6, 3, -5}},{{-5, 2, -3}},{{-4, 1, -3}},{{-1, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_29NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -14}},{{-5, 3, -6}},{{-6, 3, -5}},{{-5, 2, -3}},{{-3, 1, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_30NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -14}},{{-5, 3, -6}},{{-6, 3, -5}},{{-5, 2, -3}},{{-3, 1, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_32NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -14}},{{-5, 3, -6}},{{-6, 2, -5}},{{-5, 1, -3}},{{-3, 1, -2}},{{-1, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_34NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -14}},{{-5, 3, -6}},{{-6, 2, -4}},{{-5, 1, -3}},{{-3, 1, -2}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_37NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -14}},{{-5, 3, -5}},{{-6, 2, -4}},{{-5, 1, -3}},{{-3, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_39NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 7, -13}},{{-4, 3, -5}},{{-5, 2, -4}},{{-4, 1, -3}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_41NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -12}},{{-4, 3, -5}},{{-5, 1, -4}},{{-4, 1, -3}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_44NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -12}},{{-4, 3, -5}},{{-5, 1, -4}},{{-4, 1, -3}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_47NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-6, 6, -12}},{{-3, 3, -4}},{{-3, 1, -4}},{{-3, 1, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_50NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 6, -11}},{{-3, 3, -4}},{{-3, 1, -4}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_53NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 5, -11}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_56NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 5, -11}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_60NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-5, 4, -10}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_64NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_68NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -10}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_72NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -9}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -2}},{{-2, 0, -1}},{{0, 0, -1}},{{0, 0, 0}},{{1, 0, 1}}}, /* IBRIGHTNESS_77NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -9}},{{-3, 2, -3}},{{-3, 1, -3}},{{-3, 0, -1}},{{-2, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_82NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -8}},{{-3, 2, -3}},{{-2, 1, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_87NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -8}},{{-3, 2, -3}},{{-2, 1, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_93NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -8}},{{-3, 2, -3}},{{-2, 1, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_98NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -8}},{{-3, 2, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_105NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -7}},{{-2, 2, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_111NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -7}},{{-2, 1, -3}},{{-2, 1, -2}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_119NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -7}},{{-2, 1, -3}},{{-2, 1, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{1, 0, 2}}}, /* IBRIGHTNESS_126NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -6}},{{-2, 1, -2}},{{-2, 1, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 2}}}, /* IBRIGHTNESS_134NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-4, 3, -6}},{{-2, 1, -2}},{{-2, 1, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 2}}}, /* IBRIGHTNESS_143NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 3, -5}},{{-2, 0, -2}},{{-2, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_152NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-3, 2, -4}},{{-2, 0, -2}},{{-2, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_162NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 2, -3}},{{-2, 0, -2}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_172NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 1, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_183NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{-2, 0, -1}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 1}}}, /* IBRIGHTNESS_195NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -3}},{{-2, 0, -1}},{{-1, 0, -1}},{{-1, 0, 0}},{{-1, 0, -1}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_207NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{-2, 0, -3}},{{-1, 0, -1}},{{-1, 0, -1}},{{-1, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_220NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_234NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_249NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_265NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_282NT */
	{{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}},{{0, 0, 0}}}, /* IBRIGHTNESS_300NT */
};
const struct rgb_t (*poffset_color)[IV_MAX] = offset_color;

#endif /* __DYNAMIC_AID_XXXX_D_H */
