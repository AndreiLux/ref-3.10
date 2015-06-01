#include <linux/kernel.h>
#include <linux/module.h>

#include "core/met_drv.h"
#include "core/trace.h"
#include "plf_trace.h"

char* ms_formatH(char *__restrict__ buf, unsigned char cnt, unsigned int *__restrict__ value)
{
	char	*s = buf;
	int	len;

	if (cnt == 0) {
		buf[0] = '\0';
		return buf;
	}

	switch (cnt%4) {
	case 1:
		len = sprintf(s, "%x", value[0]);
		s += len;
		value += 1;
		cnt -= 1;
		break;
	case 2:
		len = sprintf(s, "%x,%x", value[0], value[1]);
		s += len;
		value += 2;
		cnt -= 2;
		break;
	case 3:
		len = sprintf(s, "%x,%x,%x", value[0], value[1], value[2]);
		s += len;
		value += 3;
		cnt -= 3;
		break;
	case 0:
		len = sprintf(s, "%x,%x,%x,%x", value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
		break;
	}

	while (cnt) {
		len = sprintf(s, ",%x,%x,%x,%x", value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
	}

	s[0] = '\0';

	return buf;
}

char* ms_formatD(char *__restrict__ buf, unsigned char cnt, unsigned int *__restrict__ value)
{
	char	*s = buf;
	int	len;

	if (cnt == 0) {
		buf[0] = '\0';
		return buf;
	}

	switch (cnt%4) {
	case 1:
		len = sprintf(s, "%u", value[0]);
		s += len;
		value += 1;
		cnt -= 1;
		break;
	case 2:
		len = sprintf(s, "%u,%u", value[0], value[1]);
		s += len;
		value += 2;
		cnt -= 2;
		break;
	case 3:
		len = sprintf(s, "%u,%u,%u", value[0], value[1], value[2]);
		s += len;
		value += 3;
		cnt -= 3;
		break;
	case 0:
		len = sprintf(s, "%u,%u,%u,%u", value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
		break;
	}

	while (cnt) {
		len = sprintf(s, ",%u,%u,%u,%u", value[0], value[1], value[2], value[3]);
		s += len;
		value += 4;
		cnt -= 4;
	}

	s[0] = '\0';

	return buf;
}

noinline void ms_emi(unsigned char cnt, unsigned int *value)
{
	char	buf[32*HVALUE_SIZE];	/* max cnt == 32 */

	if (cnt > 32)
		return;
	ms_formatH(buf, cnt, value);
	MET_PRINTK("%s\n", buf);
}

noinline void ms_ttype(unsigned char cnt, unsigned int *value)
{
	char	buf[5*HVALUE_SIZE];	/* max cnt == 5 */

	if (cnt > 5)
		return;
	ms_formatH(buf, cnt, value);
	MET_PRINTK("%s\n", buf);
}

noinline void ms_smi(unsigned char cnt, unsigned int *value)
{
	char	buf[60*HVALUE_SIZE];	/* max cnt == 60 */

	if (cnt > 60)
		return;
	ms_formatH(buf, cnt, value);
	MET_PRINTK("%s\n", buf);
}

noinline void ms_smit(unsigned char cnt, unsigned int *value)
{
	char	buf[14*HVALUE_SIZE];	/* max cnt == 14 */

	if (cnt > 14)
		return;
	ms_formatH(buf, cnt, value);
	MET_PRINTK("%s\n", buf);
}

#define MS_TH_FMT	"%5lu.%06lu"
#define MS_TH_VAL	(unsigned long)(timestamp), nano_rem/1000
#define MS_TH_UD_FMT1	",%d\n"
#define MS_TH_UD_FMT2	",%d,%d\n"
#define MS_TH_UD_FMT3	",%d,%d,%d\n"
#define MS_TH_UD_FMT4	",%d,%d,%d,%d\n"
#define MS_TH_UD_FMT5	",%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT6	",%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT7	",%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT8	",%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT9	",%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT10 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT11	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT12	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT13	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"
#define MS_TH_UD_FMT14	 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n"

#define MS_TH_UD_VAL1	,value[0]
#define MS_TH_UD_VAL2	,value[0],value[1]
#define MS_TH_UD_VAL3	,value[0],value[1],value[2]
#define MS_TH_UD_VAL4	,value[0],value[1],value[2],value[3]
#define MS_TH_UD_VAL5	,value[0],value[1],value[2],value[3],value[4]
#define MS_TH_UD_VAL6	,value[0],value[1],value[2],value[3],value[4],value[5]
#define MS_TH_UD_VAL7	,value[0],value[1],value[2],value[3],value[4],value[5],value[6]
#define MS_TH_UD_VAL8	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7]
#define MS_TH_UD_VAL9	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8]
#define MS_TH_UD_VAL10 ,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9]
#define MS_TH_UD_VAL11	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10]
#define MS_TH_UD_VAL12	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11]
#define MS_TH_UD_VAL13	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11],value[12]
#define MS_TH_UD_VAL14	,value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8],value[9],value[10],value[11],value[12],value[13]

noinline void ms_th(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	unsigned long nano_rem = do_div(timestamp, 1000000000);
	switch (cnt) {
	case 1: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT1, MS_TH_VAL MS_TH_UD_VAL1); break;
	case 2: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT2, MS_TH_VAL MS_TH_UD_VAL2); break;
	case 3: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT3, MS_TH_VAL MS_TH_UD_VAL3); break;
	case 4: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT4, MS_TH_VAL MS_TH_UD_VAL4); break;
	case 5: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT5, MS_TH_VAL MS_TH_UD_VAL5); break;
	case 6: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT6, MS_TH_VAL MS_TH_UD_VAL6); break;
	case 7: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT7, MS_TH_VAL MS_TH_UD_VAL7); break;
	case 8: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT8, MS_TH_VAL MS_TH_UD_VAL8); break;
	case 9: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT9, MS_TH_VAL MS_TH_UD_VAL9); break;
	case 10: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT10, MS_TH_VAL MS_TH_UD_VAL10); break;
	case 11: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT11, MS_TH_VAL MS_TH_UD_VAL11); break;
	case 12: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT12, MS_TH_VAL MS_TH_UD_VAL12); break;
	case 13: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT13, MS_TH_VAL MS_TH_UD_VAL13); break;
	case 14: MET_PRINTK(MS_TH_FMT MS_TH_UD_FMT14, MS_TH_VAL MS_TH_UD_VAL14); break;
	default : printk("Warnning!MET thermal Cnt Not support: %d\n" , cnt); break;
	}
}

#define MS_DRAMC_UD_FMT	"%x,%x,%x,%x\n"
#define MS_DRAMC_UD_VAL	value[0],value[1],value[2],value[3]
noinline void ms_dramc(unsigned long long timestamp, unsigned char cnt, unsigned int *value)
{
	switch (cnt) {
	case 4: MET_PRINTK(MS_DRAMC_UD_FMT, MS_DRAMC_UD_VAL); break;
	}
}

noinline void ms_mali(unsigned int freq, int cnt, unsigned int *value)
{
	char	buf[25*HVALUE_SIZE];	/* max cnt == 24 */
	int	len;

	if (cnt > 24)
		return;
	len = sprintf(buf, "%x,", freq);
	ms_formatH(buf+len, cnt, value);
	MET_PRINTK("%s\n", buf);
}

void ms_bw_limiter(unsigned long long timestamp,
			unsigned char cnt,
			unsigned int *value)
{
	switch (cnt) {
	case 9: MET_PRINTK(MP_FMT9, MP_VAL9); break;
	}
}
