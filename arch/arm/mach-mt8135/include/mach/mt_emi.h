#ifndef __MT_EMI_H__
#define __MT_EMI_H__

#define emi_readl(addr)             readl((volatile unsigned int *)(addr))
#define emi_writel(value, addr)     writel(value, (volatile unsigned int *)(addr))
#define emi_reg_sync_writel(v, a) \
	do {    \
	    emi_writel((v), (a));   \
	    dsb();  \
	} while (0)

#endif				/* !__MT_EMI_H__ */
