#ifndef __MT_PMIC_WRAP_H__
#define __MT_PMIC_WRAP_H__

#include <linux/types.h>

enum {
	PWRAP_READ,
	PWRAP_WRITE,
};

int pwrap_read(u32 adr, u32 *rdata);
int pwrap_write(u32 adr, u32 wdata);
int pwrap_wacs2(bool write, u32 adr, u32 wdata, u32 *rdata);
void pwrap_dump_rwspin_user(void);

#endif				/* __MT_PMIC_WRAP_H__ */
