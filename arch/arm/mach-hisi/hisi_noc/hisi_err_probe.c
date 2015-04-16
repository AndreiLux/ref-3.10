#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/bug.h>
#include <linux/delay.h>

#include "hisi_err_probe.h"
#include "hisi_noc.h"

extern void __iomem *get_errprobe_base(const char *name);

#define ERR_CODE_NR	7
char *err_code[] = {
	"Slave: returns Error response",
	"Master: Accesses reserved memory space",
	"Master: send the Burst to slave",
	"Master: accesses the powerdown area"
	"Master: Access Previlege error",
	"Master: received Hide Error Response from Firewall",
	"Master: accessed slave timeout and returned Error reponse",
};

#define MAX_OPC_NR	9

char *opc[] = {
	"RD: INCR READ",
	"RDW: WRAP READ",
	"RDL: EXCLUSIVE READ",
	"RDX: LOCK READ",
	"WR: INCR WRITE",
	"WDW: WRAP WRITE",
	"Reversed",
	"WRC: EXCLUSIVE WRITE",
	"PRE: FIXED BURST"
};


#define ERR_PROBE_CLEAR_BIT	  BIT(0)
#define ERR_PORBE_ENABLE_BIT	  BIT(0)
#define ERR_PORBE_ERRVLD_BIT			BIT(0)

static void print_errlog0(unsigned int val)
{
	unsigned int idx;

	pr_err("[ERR_LOG0 = 0x%x]:", val);

	idx = (val & ERR_LOG0_ERRCODE_MASK) >> ERR_LOG0_ERRCODE_SHIFT;
	if (idx < ERR_CODE_NR)
		pr_err("\t[err_code] %d:%s\n", idx, err_code[idx]);
	else
		pr_err("\t[err_code] out of range!\n");

	idx = (val & ERR_LOG0_OPC_MASK) >> ERR_LOG0_OPC_SHIFT;
	if (idx < MAX_OPC_NR)
		pr_err("\t[opc] %d:%s\n", idx, opc[idx]);
	else
		pr_err("\t[opc] out of range!\n");
}

static void print_errlog1(unsigned int val, unsigned int idx)
{
	struct noc_bus_info *noc_bus = &noc_buses_info[idx];
	unsigned int shift = 0;

	pr_err("[ERR_LOG1 = 0x%x]", val);

	shift = ffs(noc_bus->initflow_mask) - 1;
	idx = (val & (noc_bus->initflow_mask)) >> shift;
	if (idx < noc_bus->initflow_array_size)
		pr_err("\t[init_flow] %d: %s\n", idx, noc_bus->initflow_array[idx]);
	else
		pr_err("\t[init_flow] %d: %s\n", idx, "index is out of range!");

	shift = ffs(noc_bus->targetflow_mask) - 1;
	idx = (val & (noc_bus->targetflow_mask)) >> shift;
	if (idx < noc_bus->targetflow_array_size)
		pr_err("\t[target_flow] %d: %s\n", idx, noc_bus->targetflow_array[idx]);
	else
		pr_err("\t[target_flow] %d: %s\n", idx, "index is out of range!");
}

void noc_err_probe_hanlder(void __iomem *base, unsigned int idx)
{
	unsigned int val;

	/* dump all the err_log register */
	val = readl_relaxed(base + ERR_PORBE_ERRLOG0_OFFSET);
	print_errlog0(val);

	val = readl_relaxed(base + ERR_PORBE_ERRLOG1_OFFSET);
	print_errlog1(val, idx);

	val = readl_relaxed(base + ERR_PORBE_ERRLOG3_OFFSET);
	pr_err("[ERR_LOG3]: ADDRESS = 0x%x\n", val);


	val = readl_relaxed(base + ERR_PORBE_ERRLOG5_OFFSET);
	pr_err("[ERR_LOG5]: USER_SIGNAL = 0x%x\n", val);

	val = readl_relaxed(base + ERR_PORBE_ERRLOG7_OFFSET);
	pr_err("[ERR_LOG7]: SECURITY = %d\n", val);

	/* clear interrupt */
	writel_relaxed(ERR_PROBE_CLEAR_BIT, base + ERR_PORBE_ERRCLR_OFFSET);
}

void enable_err_probe(void __iomem *base)
{
	unsigned int val;
	val = readl_relaxed(base + ERR_PORBE_ERRVLD_OFFSET);
	if (val & ERR_PORBE_ERRVLD_BIT) {
			pr_err("ErrProbe happened before enabling interrupt\n");

			val = readl_relaxed(base + ERR_PORBE_ERRLOG0_OFFSET);
			print_errlog0(val);
			val = readl_relaxed(base + ERR_PORBE_ERRLOG1_OFFSET);
			pr_err("err_log1 = 0x%x\n", val);

			/* clear errvld */
			writel_relaxed(ERR_PROBE_CLEAR_BIT, base + ERR_PORBE_ERRCLR_OFFSET);
			wmb();
	}

	/* enable err probe intrrupt */
	writel_relaxed(ERR_PORBE_ENABLE_BIT, base + ERR_PORBE_FAULTEN_OFFSET);
}

void disable_err_probe(void __iomem *base)
{
	writel_relaxed(~ERR_PORBE_ENABLE_BIT, base + ERR_PORBE_FAULTEN_OFFSET);
}

void enable_err_probe_by_name(const char *name)
{
	void __iomem *base = get_errprobe_base(name);
	if (base == NULL) {
		pr_err("%s cannot get the node!\n", __func__);
		return;
	}

	enable_err_probe(base);
}
EXPORT_SYMBOL(enable_err_probe_by_name);


void disable_err_probe_by_name(char *name)
{
	void __iomem *base = get_errprobe_base(name);
	if (base == NULL) {
		pr_err("%s cannot get the node!\n", __func__);
		return;
	}

	disable_err_probe(base);
}
EXPORT_SYMBOL(disable_err_probe_by_name);
