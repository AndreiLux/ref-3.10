#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_reg_base.h>
#ifdef CONFIG_SMP
#include <mach/hotplug.h>
#include <linux/cpu.h>
#endif

#define UNLOCK_KEY 0xC5ACCE55
#define HDBGEN (1 << 14)
#define MDBGEN (1 << 15)
#define DBGLAR      0xFB0
#define DBGOSLAR    0x300
#define DBGDSCR     0x088
#define DBGWVR      0x180
#define DBGWCR      0x1C0
#define DBGBVR      0x100
#define DBGBCR      0x140



#define DORMENT_DEBUG  0
#if (DORMENT_DEBUG == 1)
#define dbgmsg printk
#else
#define dbgmsg(...)
#endif

#define MAX_NR_WATCH_POINT 4
#define MAX_NR_BREAK_POINT 6
extern void save_dbg_regs(unsigned int data[]);
extern void restore_dbg_regs(unsigned int data[]);
extern unsigned read_cpuid(void);
extern unsigned read_clusterid(void);
static unsigned int reg_cluster_base[2] = { CA7_DEBUG_BASE, CA15_DEBUG_BASE };

void save_dbg_regs(unsigned int data[])
{
	unsigned cpu_id, cluster_id;
	int i;

	/*Because we always use cpu0's WCR,WCR... to resoter other cpu's data
	   so we set cpu_id=0 and cluster_id=0
	 */
	cpu_id = 0;
	cluster_id = 0;
	data[0] =
	    *((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 + DBGDSCR));
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		data[i * 2 + 1] =
		    *((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
						DBGWVR + 4 * i));
		data[i * 2 + 2] =
		    *((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
						DBGWCR + 4 * i));
	}

	for (i = 0; i < MAX_NR_BREAK_POINT; i++) {
		data[i * 2 + (MAX_NR_WATCH_POINT * 2 + 1)] =
		    *((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
						DBGBVR + 4 * i));
		data[i * 2 + (MAX_NR_WATCH_POINT * 2 + 1) + 1] =
		    *((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
						DBGBCR + 4 * i));

	}

}

void restore_dbg_regs(unsigned int data[])
{
	unsigned cpu_id, cluster_id;
	int i;

	/*Because we always use cpu0's WCR,WCR... to resoter other cpu's data
	   so we set cpu_id=0 and cluster_id=0
	 */
	cpu_id = 0;
	cluster_id = 0;
	*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 + DBGLAR)) =
	    UNLOCK_KEY;
	*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 + DBGOSLAR)) =
	    ~UNLOCK_KEY;
	*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 + DBGDSCR)) =
	    data[0];
	for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
		*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
					    DBGWVR + 4 * i)) =
		    (volatile unsigned int)data[i * 2 + 1];
		*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
					    DBGWCR + 4 * i)) =
		    (volatile unsigned int)data[i * 2 + 2];
	}
	for (i = 0; i < MAX_NR_BREAK_POINT; i++) {
		*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
					    DBGBVR + 4 * i)) =
		    (volatile unsigned int)data[i * 2 + 9];
		*((volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
					    DBGBCR + 4 * i)) =
		    (volatile unsigned int)data[i * 2 + 10];
	}
}

#if 1
#ifdef CONFIG_SMP
static int __cpuinit
regs_hotplug_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int i;
	unsigned int cpu = (unsigned int)hcpu;
	unsigned cpu_id, cluster_id;
	unsigned int hcpu_cluster = 0;
	unsigned int hcpu_cpu_id = 0;
	dbgmsg(KERN_ALERT "In hotplug callback Bike Add\n");

	/*Because we always use cpu0's WCR,WCR... to resoter other cpu's data
	   so we set cpu_id=0 and cluster_id=0
	 */
	cpu_id = 0;
	cluster_id = 0;
	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:

		if (cpu == 0) {
			hcpu_cluster = 0;
			hcpu_cpu_id = 0;
			dbgmsg("hcpu=%d,hcpu_cluster=%d,hcpu_cpu_id=%d\n", (unsigned int)hcpu,
			       hcpu_cluster, hcpu_cpu_id);
		} else if (cpu == 1) {
			hcpu_cluster = 0;
			hcpu_cpu_id = 1;
			dbgmsg("hcpu=%d,hcpu_cluster=%d,hcpu_cpu_id=%d\n", (unsigned int)hcpu,
			       hcpu_cluster, hcpu_cpu_id);
		} else if (cpu == 2) {
			hcpu_cluster = 1;
			hcpu_cpu_id = 0;
			dbgmsg("hcpu=%d,hcpu_cluster=%d,hcpu_cpu_id=%d\n", (unsigned int)hcpu,
			       hcpu_cluster, hcpu_cpu_id);
		} else if (cpu == 3) {
			hcpu_cluster = 1;
			hcpu_cpu_id = 1;
			dbgmsg("hcpu=%d,hcpu_cluster=%d,hcpu_cpu_id=%d\n", (unsigned int)hcpu,
			       hcpu_cluster, hcpu_cpu_id);
		} else {
			pr_info("Can not find the mapping of hcpu\n");
			ASSERT(0);
		}

		dbgmsg("regs_hotplug_callback clusterid = %d  cpu = %d hcpu= 0x%d\n", cluster_id,
		       cpu_id, (unsigned int)hcpu);

		*(volatile unsigned int *)(reg_cluster_base[hcpu_cluster] + hcpu_cpu_id * 0x2000 +
					   DBGLAR) = UNLOCK_KEY;
		*(volatile unsigned int *)(reg_cluster_base[hcpu_cluster] + hcpu_cpu_id * 0x2000 +
					   DBGOSLAR) = ~UNLOCK_KEY;
		*(volatile unsigned int *)(reg_cluster_base[hcpu_cluster] + hcpu_cpu_id * 0x2000 +
					   DBGDSCR) |=
		    *(volatile unsigned int *)(reg_cluster_base[cluster_id] + cpu_id * 0x2000 +
					       DBGDSCR);
		for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
			*(volatile unsigned int *)(reg_cluster_base[hcpu_cluster] +
						   hcpu_cpu_id * 0x2000 + DBGWVR + 4 * i) =
			    *((volatile unsigned int *)(reg_cluster_base[cluster_id] +
							cpu_id * 0x2000 + DBGWVR + i * 4));
			*(volatile unsigned int *)(reg_cluster_base[hcpu_cluster] +
						   hcpu_cpu_id * 0x2000 + DBGWCR + 4 * i) =
			    *((volatile unsigned int *)(reg_cluster_base[cluster_id] +
							cpu_id * 0x2000 + DBGWCR + i * 4));
		}

		for (i = 0; i < MAX_NR_BREAK_POINT; i++) {
			*((volatile unsigned int *)(reg_cluster_base[hcpu_cluster] +
						    hcpu_cpu_id * 0x2000 + DBGBVR + 4 * i)) =
			    *((volatile unsigned int *)(reg_cluster_base[cluster_id] +
							cpu_id * 0x2000 + DBGBVR + i * 4));
			*((volatile unsigned int *)(reg_cluster_base[hcpu_cluster] +
						    hcpu_cpu_id * 0x2000 + DBGBCR + 4 * i)) =
			    *((volatile unsigned int *)(reg_cluster_base[cluster_id] +
							cpu_id * 0x2000 + DBGBCR + i * 4));
		}

		break;

	default:
		break;
	}

	return NOTIFY_OK;
}


static struct notifier_block cpu_nfb __cpuinitdata = {
	.notifier_call = regs_hotplug_callback
};

static int __init regs_backup(void)
{

	register_cpu_notifier(&cpu_nfb);

	return 0;
}
module_init(regs_backup);
#endif
#endif
