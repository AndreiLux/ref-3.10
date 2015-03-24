#ifndef _MT_SPM_IDLE_
#define _MT_SPM_IDLE_

#include <linux/kernel.h>

/* for IPI & Hotplug */
#define SPM_MCDI_FUNC

#define clc_emerg(fmt, args...)     pr_emerg("[CLC] " fmt, ##args)
#define clc_alert(fmt, args...)     pr_alert("[CLC] " fmt, ##args)
#define clc_crit(fmt, args...)      pr_crit("[CLC] " fmt, ##args)
#define clc_error(fmt, args...)     pr_err("[CLC] " fmt, ##args)
#define clc_warning(fmt, args...)   pr_warn("[CLC] " fmt, ##args)
#define clc_notice(fmt, args...)    pr_notice("[CLC] " fmt, ##args)
#define clc_info(fmt, args...)      pr_info("[CLC] " fmt, ##args)
#define clc_debug(fmt, args...)     pr_debug("[CLC] " fmt, ##args)

void spm_clean_ISR_status(void);

void spm_check_core_status(u32 target_core);
#if (defined CONFIG_MTK_LDVT || defined CONFIG_MT8135_FPGA)
static inline void spm_hot_plug_in_before(u32 target_core)
{
}

static inline void spm_hot_plug_out_after(u32 target_core)
{
}
#else
void spm_hot_plug_in_before(u32 target_core);
void spm_hot_plug_out_after(u32 target_core);
#endif
void spm_disable_sodi(void);
void spm_enable_sodi(void);
void spm_mcdi_wfi(void);

#endif
