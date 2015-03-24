#include <linux/kernel.h>
#include <linux/string.h>

#include <mach/mtk_rtc.h>
#include <mach/wd_api.h>
#include <mach/ext_wd_drv.h>

void arch_reset(char mode, const char *cmd)
{
	char reboot = 0;
	int res = 0;
	struct wd_api *wd_api = NULL;

	res = get_wd_api(&wd_api);
	pr_info("arch_reset: cmd = %s\n", cmd ? : "NULL");

	if (cmd && !strcmp(cmd, "charger")) {
		/* do nothing */
	} else if (cmd && !strcmp(cmd, "recovery")) {
		rtc_mark_recovery();
	} else if (cmd && !strcmp(cmd, "bootloader")) {
		rtc_mark_fast();
	} else if (cmd && !strcmp(cmd, "emergency")) {
		rtc_mark_emergency();
	}
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
	else if (cmd && !strcmp(cmd, "kpoc")) {
		rtc_mark_kpoc();
	} else if (cmd && !strcmp(cmd, "enter_kpoc")) {
		rtc_mark_enter_kpoc();
	}
#endif
	else {
		if (rtc_enter_kpoc_detected())
			reboot = 0;
		else
			reboot = 1;
	}

	if (res) {
		pr_info("arch_reset, get wd api error %d\n", res);
	} else {
		wd_api->wd_sw_reset(reboot);
	}
}
