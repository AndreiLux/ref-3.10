/*
 * platform_modem_crl.c: modem control platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#include <linux/platform_device.h>

#include <linux/gpio.h>

#include <linux/mdm_ctrl_board.h>
#include <linux/mdm_ctrl.h>

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>

#include <linux/slab.h>

#include "platform_modem_ctrl.h"

#if defined(pr_debug)
#undef pr_debug
#define pr_debug(...) 	printk("[MCD_PC_D] " __VA_ARGS__)
#endif

#if defined(pr_err)
#undef pr_err
#define pr_err(...) 	printk("[MCD_PC_E] " __VA_ARGS__)
#endif

#if defined(pr_info)
#undef pr_info
#define pr_info(...) 	printk("[MCD_PC_I] " __VA_ARGS__)
#endif

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need this sfi value for Odin. */
#else
/* Conversion table: SFI_NAME to mcd mdm version */
static struct sfi_to_mdm mdm_assoc_table[] = {
	/* IMC products */
	{"XMM_6260", MODEM_6260},
	{"XMM_6268", MODEM_6268},
	{"XMM_6360", MODEM_6360},
	{"XMM_7160", MODEM_7160},
	{"XMM_7260", MODEM_7260},
	/* Any other IMC products: set to 7160 by default */
	{"XMM", MODEM_7160},
	/* RMC products, not supported */
	{"CYGNUS", MODEM_UNSUP},
	{"PEGASUS", MODEM_UNSUP},
	{"RMC", MODEM_UNSUP},
	/* Whatever it may be, it's not supported */
	{"", MODEM_UNSUP},
};
#endif

struct cfg_match {
	char cfg_name[SFI_NAME_LEN + 1];
	char mdm_name[SFI_NAME_LEN + 1];
	int cpu_type;
};

static struct cfg_match cfg_assoc_tbl[] = {
	/* Saltbay PR1 */
	{"XMM7160_CONF_1", "XMM_7160_REV3", CPU_TANGIER},
	/* Saltbay PR2 */
	{"XMM7160_CONF_2", "XMM_7160_REV3_5", CPU_TANGIER},
	{"XMM7160_CONF_2", "XMM_7160_REV4", CPU_TANGIER},
	/* Saltbay PR2 7260 */
#if defined(CONFIG_MIGRATION_CTL_DRV)
	{"XMM7260_CONF_2", "XMM_7260_REV1", CPU_ODIN},
#else
	{"XMM7260_CONF_2", "XMM_7260_REV1", CPU_TANGIER},
#endif
	{"XMM7260_CONF_5", "XMM_7260_REV2", CPU_TANGIER},
	/* Baytrail FFRD8 */
	{"XMM7160_CONF_3", "XMM_7160", CPU_VVIEW2},
	/* CTP 7160 */
	{"XMM7160_CONF_4", "XMM_7160_REV3", CPU_CLVIEW},
	{"XMM7160_CONF_5", "XMM_7160_REV3_5", CPU_CLVIEW},
	{"XMM7160_CONF_5", "XMM_7160_REV4", CPU_CLVIEW},
	/* Redhookbay */
	{"XMM6360_CONF_1", "XMM_6360", CPU_CLVIEW},
	/* Moorefield */
#if defined(CONFIG_MIGRATION_CTL_DRV)
	{"XMM7260_CONF_1", "XMM_7260_REV1", CPU_ODIN},
#else
	{"XMM7260_CONF_1", "XMM_7260_REV1", CPU_ANNIEDALE},
#endif
	{"XMM7160_CONF_6", "XMM_7160_REV3_5", CPU_ANNIEDALE},
	{"XMM7160_CONF_6", "XMM_7160_REV4", CPU_ANNIEDALE},
	{"XMM7260_CONF_4", "XMM_7260_REV2", CPU_ANNIEDALE},
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need for Odin */
#else
	/* Cherrytrail */
	{"XMM7260_CONF_2", "XMM_7260_REV1", CPU_CHERRYVIEW},
#endif
};

/* Modem data */
static struct mdm_ctrl_mdm_data mdm_6260 = {
	.pre_on_delay = 200,
	.on_duration = 60,
	.pre_wflash_delay = 30,
	.pre_cflash_delay = 60,
	.flash_duration = 60,
	.warm_rst_duration = 60,
	.pre_pwr_down_delay = 60,
};

static struct mdm_ctrl_mdm_data mdm_generic = {
	.pre_on_delay = 200,
	.on_duration = 60,
	.pre_wflash_delay = 30,
#if defined(CONFIG_MIGRATION_CTL_DRV) /* Need find correct delay time to check flash download ready state timing */
	.pre_cflash_delay = 2000,
#else
	.pre_cflash_delay = 60,
#endif
	.flash_duration = 60,
	.warm_rst_duration = 60,
	.pre_pwr_down_delay = 650,
};

/* PMIC data */
static struct mdm_ctrl_pmic_data pmic_mfld = {
	.chipctrl = 0x0E0,
	.chipctrlon = 0x4,
	.chipctrloff = 0x2,
	.chipctrl_mask = 0xF8,
	.early_pwr_on = true,
	.early_pwr_off = false,
	.pwr_down_duration = 20000
};

static struct mdm_ctrl_pmic_data pmic_ctp = {
	.chipctrl = 0x100,
	.chipctrlon = 0x10,
	.chipctrloff = 0x10,
	.chipctrl_mask = 0x00,
	.early_pwr_on = false,
	.early_pwr_off = true,
	.pwr_down_duration = 20000
};

static struct mdm_ctrl_pmic_data pmic_mrfl = {
	.chipctrl = 0x31,
	.chipctrlon = 0x2,
	.chipctrloff = 0x0,
	.chipctrl_mask = 0xFC,
	.early_pwr_on = false,
	.early_pwr_off = true,
	.pwr_down_duration = 20000
};

static struct mdm_ctrl_pmic_data pmic_moor = {
	.chipctrl = 0x31,
	.chipctrlon = 0x2,
	.chipctrloff = 0x0,
	.chipctrl_mask = 0xFC,
	.early_pwr_on = false,
	.early_pwr_off = true,
	.pwr_down_duration = 20000
};

/* CPU Data */
static struct mdm_ctrl_cpu_data cpu_generic = {
	.gpio_rst_out_name = GPIO_RST_OUT,
	.gpio_pwr_on_name = GPIO_PWR_ON,
	.gpio_rst_bbn_name = GPIO_RST_BBN,
	.gpio_cdump_name = GPIO_CDUMP
};

static struct mdm_ctrl_cpu_data cpu_tangier = {
	.gpio_rst_out_name = GPIO_RST_OUT,
	.gpio_pwr_on_name = GPIO_PWR_ON,
	.gpio_rst_bbn_name = GPIO_RST_BBN,
	.gpio_cdump_name = GPIO_CDUMP_MRFL
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
/* Need to change to read data from device tree */
static struct mdm_ctrl_cpu_data cpu_odin = {
	.gpio_rst_out_name = RST_OUT_GPIO_NAME,
	.gpio_pwr_on_name = PWR_ON_GPIO_NAME,
	.gpio_rst_bbn_name = MODEM_RESET_GPIO_NAME,
	.gpio_cdump_name = CORE_DUMP_GPIO_NAME,
	.gpio_ap_wakeup_name = HOST_WAKEUP_GPIO_NAME,
	.gpio_slave_wakeup_name = SLAVE_WAKEUP_GPIO_NAME,
	.gpio_host_active_name = HOST_ACTIVE_GPIO_NAME,
	.gpio_modem_pwr_down_name = PWR_DOWN_GPIO_NAME,
	.gpio_force_crash_name = FORCE_CRASH_GPIO_NAME
};

static struct baseband_power_platform_data pm_odin = {
	.baseband_type = BASEBAND_XMM,
};
#endif


void *modem_data[] = {
	NULL,			/* MODEM_UNSUP */
	&mdm_6260,		/* MODEM_6260 */
	&mdm_generic,		/* MODEM_6268 */
	&mdm_generic,		/* MODEM_6360 */
	&mdm_generic,		/* MODEM_7160 */
	&mdm_generic		/* MODEM_7260 */
};

void *pmic_data[] = {
	NULL,			/* PMIC_UNSUP */
	&pmic_mfld,		/* PMIC_MFLD */
	&pmic_ctp,		/* PMIC_CLVT */
	&pmic_mrfl,		/* PMIC_MRFL */
	NULL,			/* PMIC_BYT, not supported throught SFI */
	&pmic_moor,		/* PMIC_MOOR */
	NULL,			/* PMIC_CHT, not supported throught SFI */
};

void *apcpu_data[] = {
	NULL,			/* CPU_UNSUP */
	&cpu_generic,		/* CPU_PWELL */
	&cpu_generic,		/* CPU_CLVIEW */
	&cpu_tangier,		/* CPU_TANGIER */
	NULL,				/* CPU_VVIEW, not supported throught SFI */
	&cpu_tangier,		/* CPU_ANNIEDALE */
	NULL,				/* CPU_CHERRYVIEW, not supported throught SFI */
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
void *pm_data[] = {
	NULL, 				/* PM_UNSUP */
	&pm_odin, 			/* PM_FLASHED */
	&pm_odin,			/* PM_FLASHELESS */
};
#endif

/*
 * Element to be read through sysfs entry
 */
static char modem_name[SFI_NAME_LEN];
static char cpu_name[SFI_NAME_LEN];
static char config_name[SFI_NAME_LEN];

/*
 * Modem name accessor
 */
static ssize_t modem_name_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", modem_name);
}

/* Read-only element */
static struct kobj_attribute modem_name_attribute = __ATTR_RO(modem_name);

/*
 * Cpu-name accessor
 */
static ssize_t cpu_name_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", cpu_name);
}

/* Read-only element */
static struct kobj_attribute cpu_name_attribute = __ATTR_RO(cpu_name);

/*
 * config name accessor
 */
static ssize_t config_name_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", config_name);
}

/* Read-only element */
static struct kobj_attribute config_name_attribute = __ATTR_RO(config_name);

static struct attribute *mdm_attrs[] = {
	&modem_name_attribute.attr,
	&cpu_name_attribute.attr,
	&config_name_attribute.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct attribute_group mdm_attr_group = {
	.attrs = mdm_attrs,
};

static struct kobject *telephony_kobj;

int create_sysfs_telephony_entry(void *pdata)
{
	int retval;

	/* Creating telephony directory */
	telephony_kobj = kobject_create_and_add("telephony", kernel_kobj);
	if (!telephony_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(telephony_kobj, &mdm_attr_group);
	if (retval)
		kobject_put(telephony_kobj);

	return retval;
}

#if defined(CONFIG_MIGRATION_CTL_DRV)
static int mcd_read_gpios_from_dtree(
			struct platform_device *pdev,
			struct mdm_ctrl_cpu_data *cpu_data,
			struct baseband_power_platform_data *pm_data)
{
	if (!pdev || !pm_data)
		goto invalid_param;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_rst_out_name, &cpu_data->gpio_rst_out))
		goto fail_gpio;
	pm_data->modem.xmm.ipc_cp_rst_out = cpu_data->gpio_rst_out;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_pwr_on_name, &cpu_data->gpio_pwr_on))
		goto fail_gpio;
	pm_data->modem.xmm.bb_on = cpu_data->gpio_pwr_on;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_rst_bbn_name, &cpu_data->gpio_rst_bbn))
		goto fail_gpio;
	pm_data->modem.xmm.bb_rst = cpu_data->gpio_rst_bbn;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_cdump_name, &cpu_data->gpio_cdump))
		goto fail_gpio;
	pm_data->modem.xmm.ipc_core_dump = cpu_data->gpio_cdump;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_ap_wakeup_name, &cpu_data->gpio_ap_wakeup))
		goto fail_gpio;
	pm_data->modem.xmm.ipc_ap_wake = cpu_data->gpio_ap_wakeup;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_slave_wakeup_name, &cpu_data->gpio_slave_wakeup))
		goto fail_gpio;
	pm_data->modem.xmm.ipc_bb_wake = cpu_data->gpio_slave_wakeup;

	if (of_property_read_u32(pdev->dev.of_node,
				cpu_odin.gpio_host_active_name, &cpu_data->gpio_host_active))
		goto fail_gpio;
	pm_data->modem.xmm.ipc_hsic_active = cpu_data->gpio_host_active;

	if (of_property_read_u32(pdev->dev.of_node,
			cpu_odin.gpio_modem_pwr_down_name, &cpu_data->gpio_modem_pwr_down))
		goto fail_gpio;
	pm_data->modem.xmm.bb_pwr_down = cpu_data->gpio_modem_pwr_down;

    if (of_property_read_u32(pdev->dev.of_node,
                cpu_odin.gpio_force_crash_name, &cpu_data->gpio_force_crash))
            goto fail_gpio;
    pm_data->modem.xmm.bb_force_crash = cpu_data->gpio_force_crash;

	return 0;

invalid_param:
	pr_debug("%s: invalid parameter.\n", __func__);
	return -EINVAL;

fail_gpio:
	pr_debug("%s: no gpio number in device tree\n", __func__);
	return -EIO;
}
#endif

void mcd_register_finalize(struct mcd_base_info const *info)
{
	switch (info->cpu_ver) {
	case CPU_PWELL:
	case CPU_CLVIEW:
	case CPU_TANGIER:
	case CPU_ANNIEDALE:
		{
			struct mdm_ctrl_cpu_data *cpu_data =
			    info->cpu_data;
			cpu_data->gpio_rst_out = 145;
			cpu_data->gpio_pwr_on = 148;
			cpu_data->gpio_rst_bbn = 156;
			if (info->cpu_ver == CPU_ANNIEDALE)
				cpu_data->gpio_cdump = 119;
			else
				cpu_data->gpio_cdump = 119;
			break;
		}

#if defined(CONFIG_MIGRATION_CTL_DRV)
	case CPU_ODIN:
		{
			struct mdm_ctrl_cpu_data *cpu_data =
				info->cpu_data;
			cpu_data->gpio_rst_out = 145;
			cpu_data->gpio_pwr_on = 148;
			cpu_data->gpio_rst_bbn = 156;
			cpu_data->gpio_cdump = 119;
			cpu_data->gpio_ap_wakeup = 118;
			cpu_data->gpio_slave_wakeup = 152;
			cpu_data->gpio_host_active = 157;
			pr_debug("%s: FINALIZE cpu information.\n", __func__);
		}
		break;
#endif
	}

	return;
}

/**
 * mcd_register_mdm_info - Register information retrieved from SFI table
 * @info: struct including modem name and PMIC.
 */
int mcd_register_mdm_info(struct mcd_base_info *info,
			  struct platform_device *pdev)
{
	struct mcd_base_info *mcd_reg_info =
	    kzalloc(sizeof(struct mcd_base_info), GFP_ATOMIC);
	if (!mcd_reg_info) {
		pr_err("SFI can't allocate mcd_reg_tmp_info memory");
		return -ENOMEM;
	};

	pr_info("%s : cpu info setup\n", __func__);
	info->modem_data = modem_data[info->mdm_ver];
	info->cpu_data = apcpu_data[info->cpu_ver];
	info->pmic_data = pmic_data[info->pmic_ver];
	mcd_register_finalize(info);

	memcpy(mcd_reg_info, info, sizeof(struct mcd_base_info));
	pdev->dev.platform_data = mcd_reg_info;

	return 0;
}

int mcd_get_modem_ver(char *mdm_name)
{
	int modem = 0;

	if (strstr(mdm_name, "CONF")) {
		while (cfg_assoc_tbl[modem].cfg_name) {
			if (strstr(mdm_name, cfg_assoc_tbl[modem].cfg_name)) {
				strncpy(modem_name, cfg_assoc_tbl[modem].mdm_name, SFI_NAME_LEN);
				break;
			}
			modem++;
		}
	} else
		strncpy(modem_name, mdm_name, SFI_NAME_LEN);

	modem = 0;
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need this for Odin */
#else
	/* Retrieve modem ID from modem name */
	while (mdm_assoc_table[modem].modem_name[0]) {
		/* Search for mdm_name in table.
		 * Consider support as far as generic name is in the table.
		 */
		if (strstr(mdm_name, mdm_assoc_table[modem].modem_name))
			return mdm_assoc_table[modem].modem_type;
		modem++;
	}
#endif
	return MODEM_UNSUP;
}

int mcd_get_config_ver(char *mdm_name, int mid_cpu)
{
	ssize_t i = 0;

	if (strstr(mdm_name, "CONF"))
		strncpy(config_name, mdm_name, SFI_NAME_LEN - 1);
	else {
		memset(config_name, 0, SFI_NAME_LEN);
		for (i = 0; i < ARRAY_SIZE(cfg_assoc_tbl); i++) {
			if (!strncmp(cfg_assoc_tbl[i].mdm_name, mdm_name, SFI_NAME_LEN)
					&& (cfg_assoc_tbl[i].cpu_type == mid_cpu)) {
				strncpy(config_name, cfg_assoc_tbl[i].cfg_name,
						SFI_NAME_LEN);
				/* Null terminate config_name to please KW */
				config_name[SFI_NAME_LEN - 1] = '\0';
				break;
			}
		}
		if (!strlen(config_name))
			return -1;
	}
	return 0;
}

int mcd_get_cpu_ver(void)
{
#if defined(CONFIG_MIGRATION_CTL_DRV)
    return CPU_ODIN;
#else
	enum intel_mid_cpu_type mid_cpu = intel_mid_identify_cpu();

	switch (mid_cpu) {
	case INTEL_MID_CPU_CHIP_PENWELL:
		strncpy(cpu_name, "PENWELL", SFI_NAME_LEN);
		return CPU_PWELL;
	case INTEL_MID_CPU_CHIP_CLOVERVIEW:
		strncpy(cpu_name, "CLOVERVIEW", SFI_NAME_LEN);
		return CPU_CLVIEW;
	case INTEL_MID_CPU_CHIP_TANGIER:
		strncpy(cpu_name, "TANGIER", SFI_NAME_LEN);
		return CPU_TANGIER;
	case INTEL_MID_CPU_CHIP_ANNIEDALE:
		strncpy(cpu_name, "ANNIEDALE", SFI_NAME_LEN);
		return CPU_ANNIEDALE;
	default:
		strncpy(cpu_name, "UNKNOWN", SFI_NAME_LEN);
		return CPU_UNSUP;
	}
	return CPU_UNSUP;
#endif
}

int mcd_get_pmic_ver(void)
{
#if defined(CONFIG_MIGRATION_CTL_DRV)
    return PMIC_ODIN;
#else
	/* Deduce the PMIC from the platform */
	switch (spid.platform_family_id) {
	case INTEL_MFLD_PHONE:
	case INTEL_MFLD_TABLET:
		return PMIC_MFLD;
	case INTEL_CLVTP_PHONE:
	case INTEL_CLVT_TABLET:
		return PMIC_CLVT;
	case INTEL_MRFL_PHONE:
	case INTEL_MRFL_TABLET:
		return PMIC_MRFL;
	case INTEL_BYT_PHONE:
	case INTEL_BYT_TABLET:
		return PMIC_BYT;
	case INTEL_MOFD_PHONE:
	case INTEL_MOFD_TABLET:
		return PMIC_MOOR;
	default:
		return PMIC_UNSUP;
	}
#endif
}

/*
 * modem_platform_data - Platform data builder for modem devices
 * @data: pointer to modem name retrived in sfi table
 */
void *modem_platform_data(void *data)
{
	char *mdm_name = data;
	struct mcd_base_info *mcd_info;
	pr_debug("SFI %s: modem info setup\n", __func__);

	mcd_info = kzalloc(sizeof(*mcd_info), GFP_KERNEL);
	if (!mcd_info)
		return NULL;

	mcd_info->mdm_ver = mcd_get_modem_ver(mdm_name);
	mcd_info->cpu_ver = mcd_get_cpu_ver();
	mcd_info->pmic_ver = mcd_get_pmic_ver();
	if (mcd_get_config_ver(mdm_name, mcd_info->cpu_ver)) {
		pr_err("%s: no telephony configuration found", __func__);
		kfree(mcd_info);
		return NULL;
	}
	pr_info("SFI %s cpu: %d mdm: %d pmic: %d.\n", __func__,
		mcd_info->cpu_ver, mcd_info->mdm_ver, mcd_info->pmic_ver);
	pr_info("SFI %s cpu: %s, mdm: %s:, conf: %s\n", __func__,
		cpu_name, mdm_name, config_name);


	return mcd_info;
}

static struct platform_device mcd_device = {
//	.name = DEVICE_NAME,
	.name = "modem_control",
	.id = -1,
};

#if defined(CONFIG_MIGRATION_CTL_DRV)
struct devs_id device_ids[] = {
           {"XMM_7260_REV1", SFI_DEV_TYPE_MDM, 0, /*&modem_platform_data*/NULL},
};
#endif

/*
 * sfi_handle_mdm - specific handler for intel's platform modem devices.
 * @pentry: sfi table entry
 * @dev: device id retrieved by sfi dev parser
 */
void sfi_handle_mdm(struct sfi_device_table_entry *pentry, struct devs_id *dev)
{
	void *pdata = NULL;

	//pr_info("SFI retrieve modem entry, name = %16.16s\n", pentry->name);

	pdata = dev->get_platform_data(dev->name);

	if (pdata) {
		pr_info("SFI register modem platform data for MCD device %s\n",
			dev->name);
		mcd_register_mdm_info(pdata, &mcd_device);
		platform_device_register(&mcd_device);
		if (!telephony_kobj) {
			pr_info("SFI creates sysfs entry for modem named %s\n",
				dev->name);
			create_sysfs_telephony_entry(pdata);
		} else {
			pr_info("Unexpected SFI entry for modem named %s\n",
				dev->name);
		}
		kfree(pdata);
	}
}

void mdm_platform_dev_init(void)
{
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Odin doesn't need a operation related with SFI. */
	platform_device_register(&mcd_device);
#else
	sfi_handle_mdm(NULL, &device_ids[0]);
#endif
}


#ifdef CONFIG_ACPI
acpi_status get_acpi_param(acpi_handle handle, int type, char *id,
			   union acpi_object **result)
{
	acpi_status status = AE_OK;
	struct acpi_buffer obj_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *out_obj;

	status = acpi_evaluate_object(handle, id, NULL, &obj_buffer);
	pr_err("%s: acpi_evaluate_object, status:%d\n", __func__, status);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR %d evaluating ID:%s\n", __func__, status, id);
		goto error;
	}

	out_obj = obj_buffer.pointer;
	if (!out_obj || out_obj->type != type) {
		pr_err("%s: Invalid type:%d for Id:%s\n", __func__, type, id);
		status = AE_BAD_PARAMETER;
		goto error;
	} else {
		*result = out_obj;
	}

 error:
	return status;
}
#endif

/*
 * Access ACPI resources/data to populate global object mcd_reg_info
 *
 * @pdev : The platform device object to identify ACPI data.
 */
void *retrieve_acpi_modem_data(struct platform_device *pdev)
{
    struct mcd_base_info *mcd_reg_info;
    struct mdm_ctrl_cpu_data *cpu_data;
	struct mdm_ctrl_mdm_data *mdm_data;
	struct mdm_ctrl_pmic_data *pmic_data;

    if (!pdev) {
        pr_err("%s: platform device is NULL.", __func__);
        return NULL;
    }

    mcd_reg_info = kzalloc(sizeof(struct mcd_base_info), GFP_ATOMIC);
    if (!mcd_reg_info) {
        pr_err("%s: can't allocate mcd_reg_tmp_info memory", __func__);
        goto free_mdm_info;
    }

#if defined(CONFIG_MIGRATION_CTL_DRV)
	mcd_reg_info->mdm_ver = mcd_get_modem_ver("XMM_7260_REV1");
#endif

	/* CPU Id */
	mcd_reg_info->cpu_ver = CPU_ODIN;
	mcd_reg_info->cpu_data = &cpu_odin;
	cpu_data = mcd_reg_info->cpu_data;

    /* Retrieve Modem name */
    mcd_reg_info->mdm_ver = MODEM_7260;
    mcd_reg_info->modem_data = modem_data[mcd_reg_info->mdm_ver];

    /* Retrieve Telephony configuration name from ACPI */
    mcd_get_config_ver(modem_name, mcd_reg_info->cpu_ver);

	mcd_reg_info->pmic_ver = PMIC_ODIN;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* PM information */
	mcd_reg_info->pm_ver = PM_FLASHELESS;
	mcd_reg_info->pm_data = pm_data[mcd_reg_info->pm_ver];
#endif

    pr_info("%s: cpu info setup\n", __func__);

#if defined(CONFIG_MIGRATION_CTL_DRV)
	if (mcd_read_gpios_from_dtree(pdev,
				mcd_reg_info->cpu_data,
				mcd_reg_info->pm_data))
		goto free_mdm_info;
#endif

	pr_info("%s:Setup GPIOs (PO:%d, RO:%d, RB:%d, CD:%d, HW:%d, SW:%d, HA:%d).\n",
		__func__,
		cpu_data->gpio_pwr_on,
		cpu_data->gpio_rst_out,
		cpu_data->gpio_rst_bbn,
		cpu_data->gpio_cdump,
		cpu_data->gpio_ap_wakeup,
		cpu_data->gpio_slave_wakeup,
		cpu_data->gpio_host_active);

    return mcd_reg_info;

free_mdm_info:
    pr_err("%s: ERROR retrieving data from ACPI!!!\n", __func__);
    kfree(mcd_reg_info);

    return NULL;

#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* don't need ACPI operations. */
#else
// #ifdef CONFIG_ACPI
	struct mcd_base_info *mcd_reg_info;
	acpi_status status = AE_OK;
	acpi_handle handle;
	union acpi_object *out_obj;
	union acpi_object *item;
	struct mdm_ctrl_cpu_data *cpu_data;
	struct mdm_ctrl_mdm_data *mdm_data;
	struct mdm_ctrl_pmic_data *pmic_data;
	int mid_cpu;

	if (!pdev) {
		pr_err("%s: platform device is NULL.", __func__);
		return NULL;
	}

	/* Get ACPI handle */
	handle = DEVICE_ACPI_HANDLE(&pdev->dev);

	mcd_reg_info = kzalloc(sizeof(struct mcd_base_info), GFP_ATOMIC);
	if (!mcd_reg_info) {
		pr_err("%s: can't allocate mcd_reg_tmp_info memory", __func__);
		goto free_mdm_info;
	}

	pr_info("%s: Getting platform data...\n", __func__);

	/* CPU name */
	status = get_acpi_param(handle, ACPI_TYPE_STRING, "CPU", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating CPU Name\n", __func__);
		goto free_mdm_info;
	}

	/* CPU Id */
	if (strstr(out_obj->string.pointer, "ValleyView2")) {
		mcd_reg_info->cpu_ver = CPU_VVIEW2;
		strncpy(cpu_name, "VALLEYVIEW2", SFI_NAME_LEN);
		/* mrfl is closest to BYT and anyway */
		/* we will overwrite most of the values */
		mcd_reg_info->cpu_data = &cpu_tangier;
		cpu_data = mcd_reg_info->cpu_data;
	} else if (strstr(out_obj->string.pointer, "CherryView")) {
		mcd_reg_info->cpu_ver = CPU_CHERRYVIEW;
		strncpy(cpu_name, "CHERRYVIEW", SFI_NAME_LEN);
		/* we will overwrite most of the values */
		mcd_reg_info->cpu_data = &cpu_tangier;
		cpu_data = mcd_reg_info->cpu_data;
	} else {
		pr_err("%s: ERROR CPU name %s Not supported!\n", __func__,
		       cpu_name);
		goto free_mdm_info;
	}

	/* Retrieve Modem name from ACPI */
	status = get_acpi_param(handle, ACPI_TYPE_STRING, "MDMN", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating Modem Name\n", __func__);
		goto free_mdm_info;
	}

	mcd_reg_info->mdm_ver = mcd_get_modem_ver(out_obj->string.pointer);
	if (mcd_reg_info->mdm_ver == MODEM_UNSUP) {
		pr_err("%s: ERROR Modem %s Not supported!\n", __func__,
		       modem_name);
		goto free_mdm_info;
	}
	mcd_reg_info->modem_data = modem_data[mcd_reg_info->mdm_ver];

	/* Retrieve Telephony configuration name from ACPI */
	status = get_acpi_param(handle, ACPI_TYPE_STRING, "CONF", &out_obj);
	if (ACPI_FAILURE(status)) {
		if (mcd_get_config_ver(modem_name, mcd_reg_info->cpu_ver)) {
			pr_err("%s: ERROR evaluating Modem Name\n", __func__);
			goto free_mdm_info;
		}
	} else
		mcd_get_config_ver(out_obj->string.pointer, mcd_reg_info->cpu_ver);

	/* PMIC */
	switch (mcd_reg_info->cpu_ver) {
	case CPU_VVIEW2:
		mcd_reg_info->pmic_ver = PMIC_BYT;
		/* mrfl is closest to BYT */
		mcd_reg_info->pmic_data = &pmic_mrfl;
		pmic_data = mcd_reg_info->pmic_data;
		break;
	case CPU_CHERRYVIEW:
		mcd_reg_info->pmic_ver = PMIC_CHT;
		/* moorefield is closest to CHT */
		mcd_reg_info->pmic_data = &pmic_moor;
		pmic_data = mcd_reg_info->pmic_data;
		break;
	default:
		mcd_reg_info->pmic_ver = PMIC_UNSUP;
		break;
	}

	status = get_acpi_param(handle, ACPI_TYPE_PACKAGE, "PMIC", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating PMIC info\n", __func__);
		goto free_mdm_info;
	}

	item = &(out_obj->package.elements[0]);
	pmic_data->chipctrl = (int)item->integer.value;
	item = &(out_obj->package.elements[1]);
	pmic_data->chipctrlon = (int)item->integer.value;
	item = &(out_obj->package.elements[2]);
	pmic_data->chipctrloff = (int)item->integer.value;
	item = &(out_obj->package.elements[3]);
	pmic_data->chipctrl_mask = (int)item->integer.value;
	pr_info("%s: Retrieved PMIC values:Reg:%x, On:%x, Off:%x, Mask:%x\n",
		__func__, pmic_data->chipctrl, pmic_data->chipctrlon,
		pmic_data->chipctrloff, pmic_data->chipctrl_mask);

	pr_info("%s: cpu info setup\n", __func__);

	/* finalize cpu data */
	cpu_data->gpio_pwr_on = acpi_get_gpio_by_index(&pdev->dev, 0, NULL);
	cpu_data->gpio_cdump = acpi_get_gpio_by_index(&pdev->dev, 1, NULL);
	cpu_data->gpio_rst_out = acpi_get_gpio_by_index(&pdev->dev, 2, NULL);
	cpu_data->gpio_rst_bbn = acpi_get_gpio_by_index(&pdev->dev, 3, NULL);

	pr_info("%s:Setup GPIOs(PO:%d, RO:%d, RB:%d, CD:%d)",
		__func__,
		cpu_data->gpio_pwr_on,
		cpu_data->gpio_rst_out,
		cpu_data->gpio_rst_bbn, cpu_data->gpio_cdump);

	status = get_acpi_param(handle, ACPI_TYPE_PACKAGE, "EPWR", &out_obj);
	if (ACPI_FAILURE(status)) {
		pr_err("%s: ERROR evaluating Early PWR info\n", __func__);
		goto free_mdm_info;
	}

	item = &(out_obj->package.elements[0]);
	pmic_data->early_pwr_on = (int)item->integer.value;
	item = &(out_obj->package.elements[1]);
	pmic_data->early_pwr_off = (int)item->integer.value;

	return mcd_reg_info;

 free_mdm_info:
	pr_err("%s: ERROR retrieving data from ACPI!!!\n", __func__);
	kfree(mcd_reg_info);
	return NULL;
#endif
}

/*
 * Entry point from MCD to populate modem parameters.
 *
 * @pdev : The platform device object to identify ACPI data.
 */
int retrieve_modem_platform_data(struct platform_device *pdev)  // entry
{
	int ret = -ENODEV;

	struct mcd_base_info *info;
	if (!pdev) {
		pr_err("%s: platform device is NULL, aborting\n", __func__);
		return ret;
	}
#if defined(CONFIG_MIGRATION_CTL_DRV)
	/* Don't need ACPI handler. */
#else
	if (!ACPI_HANDLE(&pdev->dev)) {
		pr_err("%s: platform device is NOT ACPI, aborting", __func__);
		goto out;
	}
#endif

	pr_err("%s: Retrieving modem info from ACPI for device %s\n",
	       __func__, pdev->name);
	info = retrieve_acpi_modem_data(pdev);

	if (!info)
		goto out;

	/* Store modem parameters in platform device */
	pdev->dev.platform_data = info;

	if (telephony_kobj) {
		pr_err("%s: Unexpected entry for device named %s\n",
		       __func__, pdev->name);
		goto out;
	}

	pr_err("%s: creates sysfs entry for device named %s\n",
	       __func__, pdev->name);
	ret = create_sysfs_telephony_entry(pdev->dev.platform_data);

 out:
	return ret;
}
