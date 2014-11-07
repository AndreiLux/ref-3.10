#ifndef __MACH_ODIN_PM_H
#define __MACH_ODIN_PM_H

#define PM_CMD		 		0x0
#define SYSTEM_RESET 		0x99f00000
#define SYSTEM_POWER_OFF	0x99e00000

#define SOFTWARE_REBOOT		0x0
#define RECOVERY_REBOOT 	0x1E22E100
#define DOWNLOAD_REBOOT 	0x1E22E101
#define FASTBOOT_REBOOT 	0x1E22E102
#define CRASH_REBOOT    	0x1E22E103
#define WATCH_DOG_REBOOT	0x1E22E104
#define WDT_INT_CRASH_MODE	0x1E22E105
#define MODEM_CRASH_REBOOT  0x1E22E106
#define MAILBOX_TIMEOUT_REBOOT  0x1E22E107
#define UNKNOWN_REBOOT		0x1E22E108



/*
               
                              
                                    
 */
#define CRASH_MAGIC_KEY 0x1ACCE221
#define HIDDEN_REBOOT   0x1E22E112
#define LAF_CMD_REBOOT      0x1E22E113
#define NORMAL_CMD_REBOOT   0x1E22E114
#define BNR_CMD_REBOOT      0x1E22E115

/*              */

extern void odin_restart(char mode, const char *cmd);
extern void odin_set_pwr_shutdown(void);
#endif
