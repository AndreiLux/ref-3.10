/*
 * ST-Ericsson M7450 modem device driver. Platform data.
 *
 * Copyright (C) ST-Ericsson AB 2012
 * Authors: Dmitry Tarnyagin <dmitry.tarnyagin@stericsson.com>
 * License terms: GNU General Public License (GPL) version 2
 */

//--------------------------------------------------
// #ifndef M7450_PLAT_H_INCLUDED
// #define M7450_PLAT_H_INCLUDED
//--------------------------------------------------

/**
 * enum m7450_hsic_resource - ST-Ericsson M7450 HSIC modem resource ids.
 *
 * @M7450_RESOURCE_ONSW:	ONSwC
 * @M7450_RESOURCE_PST:		PwrRst
 * @M7450_RESOURCE_SVC:		SERVICE
 * @M7450_RESOURCE_RO2:		RESOUT2
 * @M7450_RESOURCE_CWR:		HSIC CWR (USB_CA_WAKE)
 * @M7450_RESOURCE_AWR:		HSIC AWR (USB_AC_WAKE)
 * @M7450_RESOURCE_ARR:		HSIC ARR (USB_AC_RESUME)
 * @M7450_RESOURCE_COUNT:	Number of resources, must be last
 */
enum m7450_hsic_resource {
	M7450_RESOURCE_ONSW = 0,
	M7450_RESOURCE_PST,
	M7450_RESOURCE_RO2,
	M7450_RESOURCE_CWR,
	M7450_RESOURCE_AWR,
	M7450_RESOURCE_ARR,
	M7450_RESOURCE_COUNT
};

/**
 * struct m7450_hsic_id - ST-Ericsson M7450 HSIC modem PID/VID ids.
 *
 * @vid:	Vendor ID
 * @pid:	Product ID
*/

struct m7450_hsic_id {
	int vid;
	int pid;
};

/**
 * enum m7450_hsic_power_mode - ST-Ericsson M7450 HSIC link power mode.
 *
 * @M7450_HSIC_POWER_OFF: Power off.
 * EHCI can be switched off
 * PHY can be switched off
 * RESET state should be kept on the bus
 *
 * @M7450_HSIC_POWER_L3: Virtual detach.
 * EHCI can be put to deep sleep
 * PHY can be switched off
 * RESET state should be kept on the bus
 *
 * @M7450_HSIC_POWER_AUTO: Auto.
 * EHCI can be put to suspend by pm_runtime
 * PHY must be active and drive HSIC bus according to USB specification
 *
 * @M7450_HSIC_POWER_ON: On.
 * EHCI and PHY must be active and drive HSIC bus
*/

enum m7450_hsic_power_mode {
	M7450_HSIC_POWER_OFF,
	M7450_HSIC_POWER_L3,
	M7450_HSIC_POWER_AUTO,
	M7450_HSIC_POWER_ON,
};

/**
 * struct m7450_platform_data - ST-Ericsson M7450 HSIC modem platform data.
 *
 * @num_ids:Number of VID/PID pairs the driver should handle
 * @ids:	Array of VID/PID pairs the driver should handle
 * @usb_dev:	Pointer to platform-specific USB EHCI controller device
 * @cwr_cb:	Callback to be called when device detects activity on CWR line
 * @power_mode:	Current HSIC power mode.
 * @init:	Driver calls the callback at initialization time
 * @deinit:	Driver calls the callback at deinitialization time
 * @power:	Driver uses the callback to request HSIC power mode
 * @arr_set:	Driver uses the callback to toggle state of ARR line
 * @awr_set:	Driver uses the callback to toggle state of AWR line
 * @cwr_get:	Driver uses the callback to request current state of CWR line
 * @cwr_subscribe_irq: Driver uses the callback to subscribe for CWR interrupts
 * @suspend:Driver calls the callback (if set) on system suspend
 * @resume:	Driver calls the callback (if set) on system resume
 */

struct m7450_platform_data {
	const int num_ids;
	const struct m7450_hsic_id *ids;
	struct platform_device *usb_dev;
	enum m7450_hsic_power_mode power_mode;
	const char *regulator;
	struct regulator *hsic_reg;
	irqreturn_t (*cwr_cb)(int irq, void *data);
	int (*init)(struct platform_device *pdev);
	void (*deinit)(struct platform_device *pdev);
	int (*power)(struct platform_device *pdev,
			enum m7450_hsic_power_mode mode);
	void (*arr_set)(struct platform_device *pdev, bool on);
	void (*awr_set)(struct platform_device *pdev, bool on);
	bool (*cwr_get)(struct platform_device *pdev);
	int (*cwr_subscribe_irq)(struct platform_device *pdev,
			irqreturn_t (*cwr_cb)(int irq, void *data));
	int (*suspend)(struct platform_device *pdev);
	int (*resume)(struct platform_device *pdev);
};

//--------------------------------------------------
// #endif
//--------------------------------------------------
