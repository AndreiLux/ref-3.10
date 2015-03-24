#ifndef _PMIC_H
#define _PMIC_H

extern kal_int32 power_control_interface(enum PMIC_POWER_CTRL_CMD cmd, void *data);
extern void pmic_lock(void);
extern void pmic_unlock(void);

#endif
