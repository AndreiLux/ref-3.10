#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust/cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <linux/xlog.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_pm_ldo.h>

#include "bq24297.h"

#include <linux/switch.h>

#ifndef MTK_BATTERY_NO_HAL
#include <mach/charging.h>
#endif

/**********************************************************
  *
  *   [I2C Slave Setting]
  *
  *********************************************************/
#define bq24297_SLAVE_ADDR_WRITE   0xD6
#define bq24297_SLAVE_ADDR_READ    0xD7

static struct switch_dev bq24297_reg09;

static struct i2c_client *new_client;
static const struct i2c_device_id bq24297_i2c_id[] = { {"bq24297", 0}, {} };

static int bq24297_driver_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int bq24297_driver_suspend(struct i2c_client *client, pm_message_t mesg);
static int bq24297_driver_resume(struct i2c_client *client);
static void bq24297_driver_shutdown(struct i2c_client *client);

static struct i2c_driver bq24297_driver = {
	.driver = {
		   .name = "bq24297",
		   },
	.probe = bq24297_driver_probe,
	.shutdown =  bq24297_driver_shutdown,
	.suspend = bq24297_driver_suspend,
	.resume = bq24297_driver_resume,
	.id_table = bq24297_i2c_id,
};

/**********************************************************
  *
  *   [Global Variable]
  *
  *********************************************************/
#define bq24297_REG_NUM 11
kal_uint8 bq24297_reg[bq24297_REG_NUM] = { 0 };

/**********************************************************
  *
  *   [I2C Function For Read/Write bq24297]
  *
  *********************************************************/
int bq24297_read_byte(kal_uint8 cmd, kal_uint8 *data)
{
	int ret;

	struct i2c_msg msg[2];

	msg[0].addr = new_client->addr;
	msg[0].buf = &cmd;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = new_client->addr;
	msg[1].buf = data;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(new_client->adapter, msg, 2);

	if (ret != 2)
		pr_err("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

int bq24297_write_byte(kal_uint8 cmd, kal_uint8 data)
{
	char buf[2];
	int ret;

	buf[0] = cmd;
	buf[1] = data;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_err("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

/**********************************************************
  *
  *   [Read / Write Function]
  *
  *********************************************************/
kal_uint32 bq24297_read_interface(kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK,
				  kal_uint8 SHIFT)
{
	kal_uint8 bq24297_reg = 0;
	int ret = 0;

	/* pr_info("--------------------------------------------------\n"); */

	ret = bq24297_read_byte(RegNum, &bq24297_reg);
	/* pr_info("[bq24297_read_interface] Reg[%x]=0x%x\n", RegNum, bq24297_reg); */

	bq24297_reg &= (MASK << SHIFT);
	*val = (bq24297_reg >> SHIFT);
	/* pr_info("[bq24297_read_interface] Val=0x%x\n", *val); */

	return ret;
}

kal_uint32 bq24297_config_interface(kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK,
				    kal_uint8 SHIFT)
{
	kal_uint8 bq24297_reg = 0;
	int ret = 0;

	/* pr_info("--------------------------------------------------\n"); */

	ret = bq24297_read_byte(RegNum, &bq24297_reg);
	/* pr_info("[bq24297_config_interface] Reg[%x]=0x%x\n", RegNum, bq24297_reg); */

	bq24297_reg &= ~(MASK << SHIFT);
	bq24297_reg |= (val << SHIFT);

	ret = bq24297_write_byte(RegNum, bq24297_reg);
	/* pr_info("[bq24297_config_interface] Write Reg[%x]=0x%x\n", RegNum, bq24297_reg); */

	/* Check */
	/* bq24297_read_byte(RegNum, &bq24297_reg); */
	/* pr_info("[bq24297_config_interface] Check Reg[%x]=0x%x\n", RegNum, bq24297_reg); */

	return ret;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
/* CON0---------------------------------------------------- */

void bq24297_set_en_hiz(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON0),
				       (kal_uint8) (val),
				       (kal_uint8) (CON0_EN_HIZ_MASK),
				       (kal_uint8) (CON0_EN_HIZ_SHIFT)
	    );
}

void bq24297_set_vindpm(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON0),
				       (kal_uint8) (val),
				       (kal_uint8) (CON0_VINDPM_MASK),
				       (kal_uint8) (CON0_VINDPM_SHIFT)
	    );
}

void bq24297_set_iinlim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON0),
				       (kal_uint8) (val),
				       (kal_uint8) (CON0_IINLIM_MASK),
				       (kal_uint8) (CON0_IINLIM_SHIFT)
	    );
}

kal_uint32 bq24297_get_iinlim(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;
	ret = bq24297_read_interface((kal_uint8) (bq24297_CON0),
				     (&val),
				     (kal_uint8) (CON0_IINLIM_MASK),
				     (kal_uint8) (CON0_IINLIM_SHIFT)
	    );
	return val;
}

/* CON1---------------------------------------------------- */

void bq24297_set_reg_rst(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_REG_RST_MASK),
				       (kal_uint8) (CON1_REG_RST_SHIFT)
	    );
}

void bq24297_set_wdt_rst(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_WDT_RST_MASK),
				       (kal_uint8) (CON1_WDT_RST_SHIFT)
	    );
}

void bq24297_set_otg_config(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_OTG_CONFIG_MASK),
				       (kal_uint8) (CON1_OTG_CONFIG_SHIFT)
	    );
}

void bq24297_set_chg_config(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_CHG_CONFIG_MASK),
				       (kal_uint8) (CON1_CHG_CONFIG_SHIFT)
	    );
}

void bq24297_set_sys_min(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_SYS_MIN_MASK),
				       (kal_uint8) (CON1_SYS_MIN_SHIFT)
	    );
}

void bq24297_set_boost_lim(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON1),
				       (kal_uint8) (val),
				       (kal_uint8) (CON1_BOOST_LIM_MASK),
				       (kal_uint8) (CON1_BOOST_LIM_SHIFT)
	    );
}

/* CON2---------------------------------------------------- */

void bq24297_set_ichg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON2),
				       (kal_uint8) (val),
				       (kal_uint8) (CON2_ICHG_MASK), (kal_uint8) (CON2_ICHG_SHIFT)
	    );
}

void bq24297_set_force_20pct(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON2),
				       (kal_uint8) (val),
				       (kal_uint8) (CON2_FORCE_20PCT_MASK),
				       (kal_uint8) (CON2_FORCE_20PCT_SHIFT)
	    );
}

/* CON3---------------------------------------------------- */

void bq24297_set_iprechg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON3),
				       (kal_uint8) (val),
				       (kal_uint8) (CON3_IPRECHG_MASK),
				       (kal_uint8) (CON3_IPRECHG_SHIFT)
	    );
}

void bq24297_set_iterm(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON3),
				       (kal_uint8) (val),
				       (kal_uint8) (CON3_ITERM_MASK), (kal_uint8) (CON3_ITERM_SHIFT)
	    );
}

/* CON4---------------------------------------------------- */

void bq24297_set_vreg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON4),
				       (kal_uint8) (val),
				       (kal_uint8) (CON4_VREG_MASK), (kal_uint8) (CON4_VREG_SHIFT)
	    );
}

void bq24297_set_batlowv(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON4),
				       (kal_uint8) (val),
				       (kal_uint8) (CON4_BATLOWV_MASK),
				       (kal_uint8) (CON4_BATLOWV_SHIFT)
	    );
}

void bq24297_set_vrechg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON4),
				       (kal_uint8) (val),
				       (kal_uint8) (CON4_VRECHG_MASK),
				       (kal_uint8) (CON4_VRECHG_SHIFT)
	    );
}

/* CON5---------------------------------------------------- */

void bq24297_set_en_term(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON5),
				       (kal_uint8) (val),
				       (kal_uint8) (CON5_EN_TERM_MASK),
				       (kal_uint8) (CON5_EN_TERM_SHIFT)
	    );
}

void bq24297_set_term_stat(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON5),
				       (kal_uint8) (val),
				       (kal_uint8) (CON5_TERM_STAT_MASK),
				       (kal_uint8) (CON5_TERM_STAT_SHIFT)
	    );
}

void bq24297_set_watchdog(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON5),
				       (kal_uint8) (val),
				       (kal_uint8) (CON5_WATCHDOG_MASK),
				       (kal_uint8) (CON5_WATCHDOG_SHIFT)
	    );
}

void bq24297_set_en_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON5),
				       (kal_uint8) (val),
				       (kal_uint8) (CON5_EN_TIMER_MASK),
				       (kal_uint8) (CON5_EN_TIMER_SHIFT)
	    );
}

void bq24297_set_chg_timer(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON5),
				       (kal_uint8) (val),
				       (kal_uint8) (CON5_CHG_TIMER_MASK),
				       (kal_uint8) (CON5_CHG_TIMER_SHIFT)
	    );
}

/* CON6---------------------------------------------------- */

void bq24297_set_treg(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON6),
				       (kal_uint8) (val),
				       (kal_uint8) (CON6_TREG_MASK), (kal_uint8) (CON6_TREG_SHIFT)
	    );
}

/* CON7---------------------------------------------------- */
kal_uint32 bq24297_get_dpdm_status(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON7),
				     (&val),
				     (kal_uint8) (CON7_DPDM_EN_MASK),
				     (kal_uint8) (CON7_DPDM_EN_SHIFT)
	    );
	return val;
}

void bq24297_set_dpdm_en(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_DPDM_EN_MASK),
				       (kal_uint8) (CON7_DPDM_EN_SHIFT)
	    );
}

void bq24297_set_tmr2x_en(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_TMR2X_EN_MASK),
				       (kal_uint8) (CON7_TMR2X_EN_SHIFT)
	    );
}

void bq24297_set_batfet_disable(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_BATFET_Disable_MASK),
				       (kal_uint8) (CON7_BATFET_Disable_SHIFT)
	    );
}

void bq24297_set_int_mask(kal_uint32 val)
{
	kal_uint32 ret = 0;

	ret = bq24297_config_interface((kal_uint8) (bq24297_CON7),
				       (kal_uint8) (val),
				       (kal_uint8) (CON7_INT_MASK_MASK),
				       (kal_uint8) (CON7_INT_MASK_SHIFT)
	    );
}

/* CON8---------------------------------------------------- */

kal_uint32 bq24297_get_system_status(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON8),
				     (&val), (kal_uint8) (0xFF), (kal_uint8) (0x0)
	    );
	return val;
}

kal_uint32 bq24297_get_vbus_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON8),
				     (&val),
				     (kal_uint8) (CON8_VBUS_STAT_MASK),
				     (kal_uint8) (CON8_VBUS_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq24297_get_chrg_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON8),
				     (&val),
				     (kal_uint8) (CON8_CHRG_STAT_MASK),
				     (kal_uint8) (CON8_CHRG_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq24297_get_pg_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON8),
				     (&val),
				     (kal_uint8) (CON8_PG_STAT_MASK),
				     (kal_uint8) (CON8_PG_STAT_SHIFT)
	    );
	return val;
}

kal_uint32 bq24297_get_vsys_stat(void)
{
	kal_uint32 ret = 0;
	kal_uint8 val = 0;

	ret = bq24297_read_interface((kal_uint8) (bq24297_CON8),
				     (&val),
				     (kal_uint8) (CON8_VSYS_STAT_MASK),
				     (kal_uint8) (CON8_VSYS_STAT_SHIFT)
	    );
	return val;
}

/**********************************************************
  *
  *   [Internal Function]
  *
  *********************************************************/
void bq24297_dump_register(void)
{
	int i = 0;
	for (i = 0; i < bq24297_REG_NUM; i++) {
		bq24297_read_byte(i, &bq24297_reg[i]);
		/* pr_info("[bq24297_dump_register] Reg[0x%X]=0x%X\n", i, bq24297_reg[i]); */
		battery_xlog_printk(BAT_LOG_FULL, "[bq24297_dump_register] Reg[0x%X]=0x%X\n", i,
				    bq24297_reg[i]);
	}
}

kal_uint8 bq24297_get_reg9_fault_type(kal_uint8 reg9_fault)
{
	kal_uint8 ret = 0;
/*	if((reg9_fault & (CON9_WATCHDOG_FAULT_MASK << CON9_WATCHDOG_FAULT_SHIFT)) !=0){
		ret = BQ_WATCHDOG_FAULT;
	} else
*/
	if ((reg9_fault & (CON9_OTG_FAULT_MASK << CON9_OTG_FAULT_SHIFT)) != 0) {
		ret = BQ_OTG_FAULT;
	} else if ((reg9_fault & (CON9_CHRG_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_CHRG_INPUT_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_INPUT_FAULT;
		else if ((reg9_fault & (CON9_CHRG_THERMAL_SHUTDOWN_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if ((reg9_fault & (CON9_CHRG_TIMER_EXPIRATION_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if ((reg9_fault & (CON9_BAT_FAULT_MASK << CON9_BAT_FAULT_SHIFT)) != 0)
		ret = BQ_BAT_FAULT;
	else if ((reg9_fault & (CON9_NTC_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_NTC_COLD_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_COLD_FAULT;
		else if ((reg9_fault & (CON9_NTC_HOT_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_HOT_FAULT;
	}
	return ret;
}

void bq24297_polling_reg09(void){
	int i, i2;
	kal_uint8 reg1;

	for (i2 = i = 0; i < 4  && i2 < 10; i++, i2++) {
		bq24297_read_byte((kal_uint8) (bq24297_CON9), &bq24297_reg[bq24297_CON9]);
		if ((bq24297_reg[bq24297_CON9] & 0x40) != 0) {/* OTG_FAULT bit */
			/* Disable OTG */
			bq24297_read_byte(1, &reg1);
			reg1 &= ~0x20; /* 0 = OTG Disable */
			bq24297_write_byte(1, reg1);
			msleep(1);
			/* Enable OTG */
			reg1 |= 0x20; /* 1 = OTG Enable */
			bq24297_write_byte(1, reg1);
		}
		if (bq24297_reg[bq24297_CON9] != 0) {
			i = 0; /* keep on polling if reg9 is not 0. This is to filter noises */
			/* need filter fault type here */
			switch_set_state(&bq24297_reg09, bq24297_get_reg9_fault_type(bq24297_reg[bq24297_CON9]));
		}
		msleep(10);
	}
	/* send normal fault state to UI */
	switch_set_state(&bq24297_reg09, BQ_NORMAL_FAULT);
}

static irqreturn_t ops_gpio_int_handler(int irq, void *dev_id)
{
	bq24297_polling_reg09();
	return IRQ_HANDLED;
}

static int bq24297_driver_suspend(struct i2c_client *client, pm_message_t mesg)
{
	pr_info("[bq24297_driver_suspend] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);

	return 0;
}

static int bq24297_driver_resume(struct i2c_client *client)
{
	pr_info("[bq24297_driver_resume] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		enable_irq(client->irq);

	return 0;
}

static void bq24297_driver_shutdown(struct i2c_client *client)
{
	pr_info("[bq24297_driver_shutdown] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);
}

static int bq24297_driver_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	pr_info("[bq24297_driver_probe]\n");

	new_client = client;

	/* --------------------- */
	bq24297_dump_register();

	bat_charger_register(bq24297_control_interface);

	bq24297_reg09.name = "bq24297_reg09";
	bq24297_reg09.index = 0;
	bq24297_reg09.state = 0;
	ret = switch_dev_register(&bq24297_reg09);

	if (ret < 0)
		pr_err("[bq24297_driver_probe] switch_dev_register() error(%d)\n", ret);

	if (client->irq > 0) {
		pr_info("[bq24297_driver_probe] enable fault interrupt and register interrupt handler\n");
		/* make sure we clean REG9 before enable fault interrupt */
		bq24297_read_byte((kal_uint8) (bq24297_CON9), &bq24297_reg[9]);
		if (bq24297_reg[9] != 0)
			bq24297_polling_reg09();

		bq24297_set_int_mask(0x1);	/* Enable fault interrupt */
		ret = request_threaded_irq(client->irq, NULL, ops_gpio_int_handler,
						IRQF_TRIGGER_FALLING |
						IRQF_ONESHOT, "ops_mt6397_chrdet_gpio", NULL);
		if (ret)
			pr_err("[bq24297_driver_probe] fault interrupt registration failed err = %d\n", ret);
	}

	return 0;
}

/**********************************************************
  *
  *   [platform_driver API]
  *
  *********************************************************/
kal_uint8 g_reg_value_bq24297 = 0;
static ssize_t show_bq24297_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("[show_bq24297_access] 0x%x\n", g_reg_value_bq24297);
	return sprintf(buf, "0x%x\n", g_reg_value_bq24297);
}

static ssize_t store_bq24297_access(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[store_bq24297_access]\n");

	if (buf != NULL && size != 0) {
		pr_info("[store_bq24297_access] buf is %s and size is %d\n", buf, size);
		reg_address = simple_strtoul(buf, &pvalue, 16);

		if (size > 3) {
			reg_value = simple_strtoul((pvalue + 1), NULL, 16);
			pr_info("[store_bq24297_access] write bq24297 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			ret = bq24297_config_interface(reg_address, reg_value, 0xFF, 0x0);
		} else {
			ret = bq24297_read_interface(reg_address, &g_reg_value_bq24297, 0xFF, 0x0);
			pr_info("[store_bq24297_access] read bq24297 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq24297);
			pr_info
			    ("[store_bq24297_access] Please use \"cat bq24297_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq24297_access, 0664, show_bq24297_access, store_bq24297_access);	/* 664 */

static int bq24297_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("******** bq24297_user_space_probe!! ********\n");

	ret_device_file = device_create_file(&(dev->dev), &dev_attr_bq24297_access);

	return 0;
}

struct platform_device bq24297_user_space_device = {
	.name = "bq24297-user",
	.id = -1,
};

static struct platform_driver bq24297_user_space_driver = {
	.probe = bq24297_user_space_probe,
	.driver = {
		   .name = "bq24297-user",
		   },
};

#ifndef BQ24297_BUSNUM
#define BQ24297_BUSNUM 4
#endif

#if 0				/* move to board-proj-battery.c */
static struct i2c_board_info i2c_bq24297 __initdata = { I2C_BOARD_INFO("bq24297", (0xd6 >> 1)) };
#endif
static int __init bq24297_init(void)
{
	int ret = 0;

	pr_info("[bq24297_init] init start\n");

#if 0				/* move to board-proj-battery.c */
	i2c_register_board_info(BQ24297_BUSNUM, &i2c_bq24297, 1);
#endif

	if (i2c_add_driver(&bq24297_driver) != 0)
		pr_info("[bq24297_init] failed to register bq24297 i2c driver.\n");
	else
		pr_info("[bq24297_init] Success to register bq24297 i2c driver.\n");

	/* bq24297 user space access interface */
	ret = platform_device_register(&bq24297_user_space_device);
	if (ret) {
		pr_info("****[bq24297_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq24297_user_space_driver);
	if (ret) {
		pr_info("****[bq24297_init] Unable to register driver (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit bq24297_exit(void)
{
	i2c_del_driver(&bq24297_driver);
}
module_init(bq24297_init);
module_exit(bq24297_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24297 Driver");
MODULE_AUTHOR("Tank Hung<tank.hung@mediatek.com>");
