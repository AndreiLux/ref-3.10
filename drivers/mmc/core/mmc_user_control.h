#ifndef _MMC_USER_CONTROL_H
#define _MMC_USER_CONTROL_H

#ifdef CONFIG_ENABLE_MMC_USER_CONTROL

/* START: MMC shutdown related structs */
struct mmc_shutdown_data {
	bool enabled;
	char *pattern_buff;
	int pattern_len;
	int delay_ms;
	rwlock_t root_lock;
};
/* END: MMC shutdown related structs */

#endif

#endif
