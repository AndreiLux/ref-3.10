
#ifndef __LINUX_ODIN_MEMORY_H
#define __LINUX_ODIN_MEMORY_H

/* auto lpm register */

#define MC_LPCMD            0x50
#define MC_AUTOLPM0         0x54
#define MC_AUTOLPM1         0x58
#define MC_AUTOLPM2         0x5C

/* interrupt register */

#define MC_INTSTATUS		0x210
#define MC_INTACK			0x214
#define MC_INTMASK          0x218

/* interrupt status & mask bit */
#define ALL_OF_INT          0x1 << 26
#define DLL_RESYNC          0x1 << 25
#define TEMP_CHANGE_ALERT   0x1 << 21 /* b[20] is change alert in datasheet, but it's wrong */
#define TEMP_DANGER_ALERT   0x1 << 20 /* b[20] and b[21] is exchanged in real */
#define LPM_COMPLETE        0x1 << 4
#define INIT_COMPLETE       0x1 << 3
#define ERROR_PORT          0x1 << 2

#define NUM_MC 2

struct odin_mc {
	void __iomem *regs[NUM_MC];
	u32 base[NUM_MC];
	struct device *dev;
};

u32 mc_readl(u32 offs, u32 sel_ctrl);
void mc_writel( u32 val, u32 offs, u32 sel_ctrl);
void auto_lpm_disable(void);
void auto_lpm_enable(u32 clock_gate, u32 pdown , u32 selfref, u32 self_gating);
#endif
