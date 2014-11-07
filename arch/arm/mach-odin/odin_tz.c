#include <linux/smp.h>
#include <linux/module.h>
#include <linux/platform_data/odin_tz.h>
#include <asm/io.h>
#include <asm/cputype.h>
#include <linux/smp.h>

#ifdef CONFIG_ODIN_SECURE_SMMU
#include "odin_sec_smmu.h"
#endif

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
        defined(__GNUC_MINOR__) && \
        defined(__GNUC_PATCHLEVEL__) && \
        ((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
        >= 40502
#define USE_ARCH_EXTENSION_SEC
#endif

#define SMC_SLEEP	-7
#define SMC_WRITE	-64
#define SMC_READ	-65
#define SMC_CPU_ENTRY	-66
#define SMC_DRVOPS	-67
#define SMC_SES_PM	-68


/* generic fast call parameters */
union fc_generic {
	struct {
		uint32_t cmd;
		uint32_t param[3];
	} as_in;
	struct {
		uint32_t resp;
		uint32_t ret;
		uint32_t param[2];
	} as_out;
};

extern void mc_fastcall(void *data);

static u32 _odin_smc(u32 cmd, u32 param1, u32 param2)
{
	register u32 reg0 asm("r0") = cmd;
	register u32 reg1 asm("r1") = param1;
	register u32 reg2 asm("r2") = param2;

	__asm__ volatile (
#ifdef USE_ARCH_EXTENSION_SEC
		/* This pseudo op is supported and required from
		 * binutils 2.21 on */
		".arch_extension sec\n\t"
#endif
		"smc 0\n"
		: "+r"(reg0), "+r"(reg1), "+r"(reg2)
	);

	return reg2;
}

/*
 * odin_smc wrapper to handle the multi core case
 */
static u32 odin_smc(u32 cmd, u32 param1, u32 param2)
{
	return _odin_smc(cmd, param1, param2);
}

u32 tz_secondary(u32 cpu_id, u32 phy_addr)
{
	if (0) {
		union fc_generic fc;
		memset(&fc, 0, sizeof(fc));

		fc.as_in.cmd = SMC_CPU_ENTRY;
		fc.as_in.param[0] = cpu_id;
		fc.as_in.param[1] = phy_addr;

		mc_fastcall(&fc);
	} else {
		odin_smc(SMC_CPU_ENTRY, cpu_id, phy_addr);
	}
	return 0;
}
EXPORT_SYMBOL(tz_secondary);

u32 tz_write(u32 value, u32 addr)
{
	if (0) {
		union fc_generic fc;
		memset(&fc, 0, sizeof(fc));

		fc.as_in.cmd = SMC_WRITE;
		fc.as_in.param[0] = addr;
		fc.as_in.param[1] = value;

		mc_fastcall(&fc);
	} else {
		odin_smc(SMC_WRITE, addr, value);
	}

	return 0;
}
EXPORT_SYMBOL(tz_write);

u32 tz_read(u32 addr)
{
	u32 ret;

	if (0) {
		union fc_generic fc;
		memset(&fc, 0, sizeof(fc));

		fc.as_in.cmd = SMC_READ;
		fc.as_in.param[0] = addr;

		mc_fastcall(&fc);
		ret = fc.as_out.param[0];
	} else {
		ret = odin_smc(SMC_READ, addr, 0);
	}
	return ret;
}
EXPORT_SYMBOL(tz_read);

u32 tz_sleep(u32 type, void *data)
{
	u32 ret;

	ret = odin_smc(SMC_SLEEP, type, (u32)data);

	return ret;
}
EXPORT_SYMBOL(tz_sleep);

u32 tz_ops(u32 type, void *data)
{
	u32 ret = 0;

#ifdef CONFIG_ODIN_SECURE_SMMU
	if (type == ODIN_NIU_INIT) {
		/* odin kernel driver operation */
		ret = odin_niu_tz_init((u32)data);
		return ret;
	} else if (type == ODIN_IOMMU_INIT) {
		/* odin kernel driver operation */
		ret = odin_iommu_tz_init((u32)data);
		return ret;
	}
#else
	ret = odin_smc(SMC_DRVOPS, type, (u32)data);
#endif

	return ret;
}
EXPORT_SYMBOL(tz_ops);

u32 tz_ses_pm(u32 module, u32 state)
{
	return odin_smc(SMC_SES_PM, module, state);
}
EXPORT_SYMBOL(tz_ses_pm);

#define SRAM_NEON_PTR		0x00003F100
#define CPU_OFFSET		0x40

static void __iomem *sram_va = NULL;

u32 tz_rotator(struct fc_neon_rot *data, u32 degree)
{
	void __iomem *sram_addr = NULL;

	sram_addr = sram_va + (CPU_OFFSET * (read_cpuid_mpidr() & 0xF));
	memcpy(sram_addr, data, sizeof(struct fc_neon_rot));

	if ((degree == ODIN_FC_NEON_MAP) || (degree == ODIN_FC_NEON_UNMAP))
		return odin_smc(degree, SRAM_NEON_PTR, 0);
	else
		return odin_smc(ODIN_FC_NEON_ROTATE, SRAM_NEON_PTR, degree);
}
EXPORT_SYMBOL(tz_rotator);

void odin_sram_map(void)
{
	sram_va = ioremap(SRAM_NEON_PTR, SZ_4K);
}
EXPORT_SYMBOL(odin_sram_map);
