#ifndef _LINUX_BQ_BCI_BATTERY_H
#define _LINUX_BQ_BCI_BATTERY_H


#define VCHRG_POWER_NONE_EVENT         (0x00)
#define VCHRG_NOT_CHARGING_EVENT       (0x10)
#define VCHRG_START_CHARGING_EVENT     (0x20)
#define VCHRG_START_AC_CHARGING_EVENT  (0x30)
#define VCHRG_START_USB_CHARGING_EVENT (0x40)
#define VCHRG_CHARGE_DONE_EVENT        (0x50)
#define VCHRG_STOP_CHARGING_EVENT      (0x60)
#define VCHRG_POWER_SUPPLY_OVERVOLTAGE (0x80)
#define VCHRG_POWER_SUPPLY_WEAKSOURCE  (0x90)
#define BATTERY_LOW_WARNING      0xA1
#define BATTERY_LOW_SHUTDOWN     0xA2

#define BATTERY_CAPACITY_FULL     (100)
#define BATTERY_MONITOR_INTERVAL  (10)
#define BATTERY_LOW_CAPACITY      (15)

#define LOW_VOL_IRQ_NUM           (13)
#define LOW_BAT_VOL_MASK_1        (0x3B)
#define LOW_BAT_VOL_MASK_2        (0x39)
#define LOW_BAT_VOL_MASK_3        (0x38)
#define LOW_BAT_VOL_MASK_4        (0x37)
#define LOW_BAT_VOL_MASK_5        (0x3D)

#define ERROR_BATT_NOT_EXIST            (0x0001)
#define ERROR_BATT_TEMP_STOP            (0x0002)
#define ERROR_BATT_TEMP_CHARGE          (0x0004)
#define ERROR_BATT_TEMP_OUT             (0x0008)
#define ERROR_VBUS_OVERVOLTAGE          (0x0010)
#define ERROR_BATT_OVERVOLTAGE          (0x0020)
#define ERROR_BATT_VOLTAGE              (0x0040)
#define ERROR_BATT_NOT_CHARGING         (0x0080)
#define ERROR_PRE_CHARGEDONE            (0x0100)
#define ERROR_NO_CHARGEDONE             (0x0200)
#define ERROR_BAD_CURR_SENSOR           (0x0400)
#define ERROR_UFCAPCITY_DEBOUNCE_100    (0x0800)
#define ERROR_FCC_DEBOUNCE              (0x1000)
#define ERROR_CAPACITY_CHANGE_FAST      (0x2000)
#define ERROR_BATT_TEMP_STAY            (0x4000)
#define ERROR_UFCAPCITY_DEBOUNCE_OTHER  (0x8000)

#define IMPOSSIBLE_IAVG            (9999)
#define RECHG_PROTECT_THRESHOLD    (150)

enum plugin_status {
    /* no charger plugin */
    PLUGIN_DEVICE_NONE,
   /* plugin usb charger */
    PLUGIN_USB_CHARGER,
   /* plugin ac charger */
   PLUGIN_AC_CHARGER,
};

int bq_register_notifier(struct notifier_block *nb,
                unsigned int events);
int bq_unregister_notifier(struct notifier_block *nb,
                unsigned int events);
int bq_bci_show_capacity(void);
#endif
