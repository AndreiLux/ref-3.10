
#include <linux/input.h>
#include <linux/delay.h>
#include <mach/hal_pub_kpd.h>
#include <mach/hal_priv_kpd.h>
#include <mach/board.h>
#include <linux/kpd.h>
#include <drivers/misc/mediatek/power/mt8135/upmu_common.h>
#include <linux/gpio.h>
#include <mach/mt_gpio_def.h>

#define KPD_DEBUG

#ifdef KPD_PWRKEY_USE_EINT
static u8 kpd_pwrkey_state = !KPD_PWRKEY_POLARITY;
#endif

static u16 kpd_keymap[KPD_NUM_KEYS];
static u16 kpd_keymap_state[KPD_NUM_MEMS] = {
	0xffff, 0xffff, 0xffff, 0xffff, 0x00ff
};

void kpd_slide_qwerty_init(struct input_dev *dev)
{
#ifdef KPD_HAS_SLIDE_QWERTY
	bool evdev_flag = false;
	bool power_op = false;
	struct input_handler *handler;
	struct input_handle *handle;
	handle = rcu_dereference(dev->grab);
	if (handle) {
		handler = handle->handler;
		if (strcmp(handler->name, "evdev") == 0) {
			return -1;
		}
	} else {
		list_for_each_entry_rcu(handle, &dev->h_list, d_node) {
			handler = handle->handler;
			if (strcmp(handler->name, "evdev") == 0) {
				evdev_flag = true;
				break;
			}
		}
		if (evdev_flag == false) {
			return -1;
		}
	}

	power_op = powerOn_slidePin_interface();
	if (!power_op) {
		pr_info(KPD_SAY "Qwerty slide pin interface power on fail\n");
	} else {
		kpd_print("Qwerty slide pin interface power on success\n");
	}

	mt65xx_eint_set_sens(KPD_SLIDE_EINT, KPD_SLIDE_SENSITIVE);
	mt65xx_eint_set_hw_debounce(KPD_SLIDE_EINT, KPD_SLIDE_DEBOUNCE);
	mt65xx_eint_registration(KPD_SLIDE_EINT, true, KPD_SLIDE_POLARITY,
				 kpd_slide_eint_handler, false);

	power_op = powerOff_slidePin_interface();
	if (!power_op) {
		pr_info(KPD_SAY "Qwerty slide pin interface power off fail\n");
	} else {
		kpd_print("Qwerty slide pin interface power off success\n");
	}
#endif
	return;
}

/************************************************************/
#ifdef CONFIG_MTK_LDVT
void mtk_kpd_gpio_set(struct platform_device *pdev)
{
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(&pdev->dev);
	unsigned *kcol = p_mtk_kpd_hw->kcol;
	unsigned *krow = p_mtk_kpd_hw->krow;
	int i;

	kpd_print("Enter mtk_kpd_gpio_set!\n");

	for (i = 0; i < KCOL_KROW_MAX; i++) {
		if (kcol[i] != 0) {
			/* KCOL: GPIO INPUT + PULL ENABLE + PULL UP */
			/* According to the comment above pin has to be in GPIO
			mode, but mt_set_gpio_mode(kcol[i], 1) sets it to EINT
			mode, so preserving the old code */
			mt_pin_set_mode_eint(kcol[i]);
			gpio_direction_input(kcol[i]);
			mt_pin_set_pull(kcol[i], MT_PIN_PULL_ENABLE);
			mt_pin_set_pull(kcol[i], MT_PIN_PULL_UP);
		}

		if (krow[i] != 0) {
			/* KROW: GPIO output + pull disable + pull down */
			/* According to the comment above pin has to be in GPIO
			mode, but mt_set_gpio_mode(krow[i], 1) sets it to EINT
			mode, so preserving the old code */
			mt_pin_set_mode_eint(krow[i]);
			gpio_direction_output(krow[i], 0);
			mt_pin_set_pull(krow[i], MT_PIN_PULL_DISABLE);
			mt_pin_set_pull(krow[i], MT_PIN_PULL_DOWN);
		}
	}
}
#endif

void kpd_ldvt_test_init(struct platform_device *pdev)
{
#ifdef CONFIG_MTK_LDVT
#if 0
	/* set kpd enable and sel register */
	mt65xx_reg_sync_writew(0x0, KP_SEL);
	mt65xx_reg_sync_writew(0x1, KP_EN);
	/* set kpd GPIO to kpd mode */
	mtk_kpd_gpio_set(pdev);
#else
	unsigned int a, b, c, j;
	unsigned int addr[] = {
		0x01d4, 0xc0d0, 0xc0d8, 0xc0e0, 0xc0e8, 0xc0f0, 0xc0c0, 0xc040, 0xc048, 0x0502
	};
	unsigned int data[] = {
		0x0000, 0x1249, 0x1249, 0x1249, 0x1249, 0x1249, 0x1249, 0xf000, 0x07ff, 0x4000
	};

	for (j = 0; j < 10; j++) {
		a = pwrap_read(addr[j], &c);
		if (a != 0)
			pr_info("kpd read fail, addr: 0x%x\n", addr[j]);
		pr_info("kpd read addr: 0x%x: data:0x%x\n", addr[j], c);
		a = pwrap_write(addr[j], data[j]);
		if (a != 0)
			pr_info("kpd write fail, addr: 0x%x\n", addr[j]);
		a = pwrap_read(addr[j], &c);
		if (a != 0)
			pr_info("kpd read fail, addr: 0x%x\n", addr[j]);
		pr_info("kpd read addr: 0x%x: data:0x%x\n", addr[j], c);
	}

	*(volatile u16 *)(KP_PMIC) = 0x1;
	pr_info("kpd register for pmic set!\n");
#endif
#endif
	return;
}

/*******************************kpd factory mode auto test *************************************/
void kpd_auto_test_for_factorymode(struct miscdevice *mdev)
{
	int i;
	int time = 500;
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(mdev->parent);
	unsigned *kcol = p_mtk_kpd_hw->kcol;

#ifdef KPD_PWRKEY_USE_PMIC
	kpd_pwrkey_pmic_handler(1);
	msleep(time);
	kpd_pwrkey_pmic_handler(0);
#endif

	if (p_mtk_kpd_hw->kpd_pmic_rstkey_map_en == TRUE) {
		kpd_pmic_rstkey_handler(1);
		msleep(time);
		kpd_pmic_rstkey_handler(0);
	}
	kpd_print("Enter kpd_auto_test_for_factorymode!\n");

	for (i = 0; i < KCOL_KROW_MAX; i++) {
		if (kcol[i] != 0) {
			msleep(time);
			kpd_print("kpd kcolumn %d pull down!\n", kcol[i]);
			mt_pin_set_pull(kcol[i], MT_PIN_PULL_DOWN);
			msleep(time);
			kpd_print("kpd kcolumn %d pull up!\n", kcol[i]);
			mt_pin_set_pull(kcol[i], MT_PIN_PULL_UP);
		}
	}
	return;
}

/********************************************************************/
void long_press_reboot_function_setting(struct platform_device *pdev)
{
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(&pdev->dev);

	if (get_boot_mode() == NORMAL_BOOT) {
		kpd_print("Normal Boot long press reboot selection\n");
		if (p_mtk_kpd_hw->kpd_pmic_lprst_td_en == TRUE) {
			kpd_print("Enable normal mode LPRST\n");
			if (p_mtk_kpd_hw->onekey_reboot_normal_mode == TRUE) {
				upmu_set_rg_pwrkey_rst_en(0x01);
				/* upmu_set_rg_fchr_pu_en(0x01); */
				upmu_set_rg_homekey_puen(0x01);
				upmu_set_rg_homekey_rst_en(0x00);
				upmu_set_rg_pwrkey_rst_td(p_mtk_kpd_hw->kpd_pmic_lprst_td_value);
			}

			if (p_mtk_kpd_hw->twokey_reboot_normal_mode == TRUE) {
				upmu_set_rg_pwrkey_rst_en(0x01);
				upmu_set_rg_homekey_rst_en(0x01);
				/* upmu_set_rg_fchr_pu_en(0x01); */
				upmu_set_rg_homekey_puen(0x01);
				upmu_set_rg_pwrkey_rst_td(p_mtk_kpd_hw->kpd_pmic_lprst_td_value);
			}
		} else {
			kpd_print("disable normal mode LPRST\n");
			upmu_set_rg_pwrkey_rst_en(0x00);
			upmu_set_rg_homekey_rst_en(0x00);
			/* upmu_set_rg_fchr_pu_en(0x01); */
			upmu_set_rg_homekey_puen(0x01);
		}
	} else {
		kpd_print("Other Boot Mode long press reboot selection\n");
		if (p_mtk_kpd_hw->kpd_pmic_lprst_td_en == TRUE) {
			kpd_print("Enable other mode LPRST\n");
			if (p_mtk_kpd_hw->onekey_reboot_normal_mode == TRUE) {
				upmu_set_rg_pwrkey_rst_en(0x01);
				/* upmu_set_rg_fchr_pu_en(0x01); */
				upmu_set_rg_homekey_puen(0x01);
				upmu_set_rg_homekey_rst_en(0x00);
				upmu_set_rg_pwrkey_rst_td(p_mtk_kpd_hw->kpd_pmic_lprst_td_value);
			}
			if (p_mtk_kpd_hw->twokey_reboot_normal_mode == TRUE) {
				upmu_set_rg_pwrkey_rst_en(0x01);
				upmu_set_rg_homekey_rst_en(0x01);
				/* upmu_set_rg_fchr_pu_en(0x01); */
				upmu_set_rg_homekey_puen(0x01);
				upmu_set_rg_pwrkey_rst_td(p_mtk_kpd_hw->kpd_pmic_lprst_td_value);
			}
		} else {
			kpd_print("disable other mode LPRST\n");
			upmu_set_rg_pwrkey_rst_en(0x00);
			upmu_set_rg_homekey_rst_en(0x00);
			/* upmu_set_rg_fchr_pu_en(0x01); */
			upmu_set_rg_homekey_puen(0x01);
		}
	}
}

/********************************************************************/
void kpd_wakeup_src_setting(int enable)
{
#ifndef EVB_PLATFORM
	int err = 0;
	if (enable == 1) {
		kpd_print("enable kpd as wake up source operation!\n");
		err = slp_set_wakesrc(WAKE_SRC_KP, true, false);
		if (err != 0) {
			kpd_print("enable kpd as wake up source fail!\n");
		}
	} else {
		kpd_print("disable kpd as wake up source operation!\n");
		err = slp_set_wakesrc(WAKE_SRC_KP, false, false);
		if (err != 0) {
			kpd_print("disable kpd as wake up source fail!\n");
		}
	}
#endif
}

/********************************************************************/
void kpd_init_keymap(struct platform_device *pdev, u16 keymap[])
{
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(&pdev->dev);
	int i = 0;
	memcpy(kpd_keymap, p_mtk_kpd_hw->kpd_init_keymap, sizeof(kpd_keymap)/sizeof(kpd_keymap[0]));
	for (i = 0; i < KPD_NUM_KEYS; i++) {
		keymap[i] = kpd_keymap[i];
	}
}

void kpd_init_keymap_state(u16 keymap_state[])
{
	int i = 0;
	for (i = 0; i < KPD_NUM_MEMS; i++) {
		keymap_state[i] = kpd_keymap_state[i];
	}
	pr_info(KPD_SAY "init_keymap_state %x %x %x %x %x done!\n", keymap_state[0],
		keymap_state[1], keymap_state[2], keymap_state[3], keymap_state[4]);
}

/********************************************************************/
void kpd_get_keymap_state(u16 state[])
{
	state[0] = *(volatile u16 *)KP_MEM1;
	state[1] = *(volatile u16 *)KP_MEM2;
	state[2] = *(volatile u16 *)KP_MEM3;
	state[3] = *(volatile u16 *)KP_MEM4;
	state[4] = *(volatile u16 *)KP_MEM5;
	pr_info(KPD_SAY "register = %x %x %x %x %x\n", state[0], state[1], state[2], state[3],
		state[4]);

}

void kpd_set_debounce(u16 val)
{
	mt65xx_reg_sync_writew((u16) (val & KPD_DEBOUNCE_MASK), KP_DEBOUNCE);
}

/********************************************************************/
void kpd_pmic_rstkey_hal(struct platform_device *pdev, unsigned long pressed)
{
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(&pdev->dev);
	struct pdev_inside_data *p_pdev_private_data = platform_get_drvdata(pdev);
	struct input_dev *kpd_input_dev = p_pdev_private_data->p_input_device;

	if (p_mtk_kpd_hw->kpd_pmic_rstkey_map_en == TRUE) {
		input_report_key(kpd_input_dev, p_mtk_kpd_hw->kpd_pmic_rstkey_map_value, pressed);
		input_sync(kpd_input_dev);
		pr_info(KPD_SAY "(%s) HW keycode =%d using PMIC\n",
			pressed ? "pressed" : "released",
			p_mtk_kpd_hw->kpd_pmic_rstkey_map_value);
	}
}

void kpd_pmic_pwrkey_hal(struct platform_device *pdev, unsigned long pressed)
{
#ifdef KPD_PWRKEY_USE_PMIC
	struct mtk_kpd_hardware *p_mtk_kpd_hw = dev_get_platdata(&pdev->dev);
	struct pdev_inside_data *p_pdev_private_data = platform_get_drvdata(pdev);
	struct input_dev *kpd_input_dev = p_pdev_private_data->p_input_device;

	input_report_key(kpd_input_dev, p_mtk_kpd_hw->kpd_pwrkey_map, pressed);
	input_sync(kpd_input_dev);
	pr_info(KPD_SAY "(%s) HW keycode =%d using PMIC\n",
		pressed ? "pressed" : "released", p_mtk_kpd_hw->kpd_pwrkey_map);
#endif
}

/***********************************************************************/
void kpd_pwrkey_handler_hal(unsigned long data)
{
#ifdef KPD_PWRKEY_USE_EINT
	bool pressed;
	u8 old_state = kpd_pwrkey_state;

	kpd_pwrkey_state = !kpd_pwrkey_state;
	pressed = (kpd_pwrkey_state == !!KPD_PWRKEY_POLARITY);
	pr_info(KPD_SAY "(%s) HW keycode = using EINT\n", pressed ? "pressed" : "released");
#ifdef KPD_DRV_CTRL_BACKLIGHT
	kpd_backlight_handler(pressed, KPD_PWRKEY_MAP);
#endif
	input_report_key(kpd_input_dev, KPD_PWRKEY_MAP, pressed);
	input_sync(kpd_input_dev);
	kpd_print("report Linux keycode = %u\n", KPD_PWRKEY_MAP);

	/* for detecting the return to old_state */
	mt65xx_eint_set_polarity(KPD_PWRKEY_EINT, old_state);
	mt65xx_eint_unmask(KPD_PWRKEY_EINT);
#endif
}

/***********************************************************************/
void mt_eint_register(void)
{
#ifdef KPD_PWRKEY_USE_EINT
	mt65xx_eint_set_sens(KPD_PWRKEY_EINT, KPD_PWRKEY_SENSITIVE);
	mt65xx_eint_set_hw_debounce(KPD_PWRKEY_EINT, KPD_PWRKEY_DEBOUNCE);
	mt65xx_eint_registration(KPD_PWRKEY_EINT, true, KPD_PWRKEY_POLARITY,
				 kpd_pwrkey_eint_handler, false);
#endif
}

/************************************************************************/
