#ifndef __CLDMA_PLATFORM_H__
#define __CLDMA_PLATFORM_H__
#include "ccci_core.h"
#include <mach/sync_write.h>

#define ccif_write32(b, a, v)           mt_reg_sync_writel(v, (b)+(a))
#define ccif_write16(b, a, v)           mt_reg_sync_writew(v, (b)+(a))
#define ccif_write8(b, a, v)            mt_reg_sync_writeb(v, (b)+(a))
#define ccif_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccif_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccif_read8(b, a)                ioread8((void __iomem *)((b)+(a)))

//MD peripheral register: MD bank8; AP bank2
/* Modem WDT */
#define WDT_MD_MODE     (0x00)
#define WDT_MD_LENGTH   (0x04)
#define WDT_MD_RESTART  (0x08)
#define WDT_MD_STA      (0x0C)
#define WDT_MD_SWRST    (0x1C)
#define WDT_MD_MODE_KEY (0x0000220E)

/* CCIF */
#define APCCIF_CON    (0x00)
#define APCCIF_BUSY   (0x04)
#define APCCIF_START  (0x08)
#define APCCIF_TCHNUM (0x0C)
#define APCCIF_RCHNUM (0x10)
#define APCCIF_ACK    (0x14)
#define APCCIF_CHDATA (0x100)

extern int md_ccif_power_off(struct ccci_modem *md, unsigned int timeout);
extern int md_ccif_power_on(struct ccci_modem *md);
extern int md_ccif_let_md_go(struct ccci_modem *md);

#endif //__CLDMA_PLATFORM_H__
