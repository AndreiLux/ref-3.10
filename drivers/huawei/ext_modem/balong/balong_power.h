#ifndef _BALONG_POWER_H
#define _BALONG_POWER_H

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/types.h>

/* enum of power state machine*/
enum BALONG_POWER_STATE_E {
    BALONG_POW_S_OFF,
    BALONG_POW_S_INIT_ON,
    BALONG_POW_S_WAIT_READY,
    BALONG_POW_S_INIT_USB_ON,
    BALONG_POW_S_USB_ENUM_DONE,
    BALONG_POW_S_USB_L0,
    BALONG_POW_S_USB_L2,
    BALONG_POW_S_USB_L2_TO_L0,
    BALONG_POW_S_USB_L3,
    BALONG_POW_S_MAX,
};

/* status for balong ril, to notice ril modem state*/
enum BALONG_STATE_FLAG_E {
    BALONG_STATE_READY = 3,             /*uevent for ril:  modem c core is reay*/
    BALONG_STATE_RESET = 1,             /*uevent for ril:  modem reset occured, retart modem(do not power off)*/
    BALONG_STATE_REBOOT = 6,            /*uevent for ril:  modem reset occured, retart modem(need power off)*/
    BALONG_STATE_MAX,
};

enum T_ENUM_MODEM_STATE {
    MODEM_STATE_OFF = 0,
    MODEM_STATE_POWER,
    MODEM_STATE_READY,
    MODEM_STATE_INVALID,
    ENUM_MODEM_STATE_NUM,
};

enum BALONG_USB_SWITCH_STATUS_E{
    USB_TURNED_OFF,
    USB_TURNED_ON,
};

enum USB_CABLE_CONNECTED_STATUS_E{
    USB_CABLE_NOT_CONNECTED,
    USB_CABLE_CONNECTED,
};

struct balong_reset_ind_reg {
    resource_size_t base_addr;
    resource_size_t gpioic;
};

struct balong_power_plat_data {
    unsigned balong_power_on;
    unsigned balong_pmu_reset;
    unsigned balong_host_active;
    unsigned balong_host_wakeup;
    unsigned balong_slave_wakeup;
    unsigned balong_reset_ind;
    unsigned balong_usb_insert_ind;
    unsigned balong_slave_active;
};

/* For usb to call for bus suspend/resume */
extern void balong_power_runtime_idle(void);
extern void balong_power_runtime_resume(void);


#endif /*_balong_POWER_H*/
