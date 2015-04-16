/*
 * hisi_sim_hotplug.c - SIM HOTPLUG driver
 *
 * Copyright (C) 2012 huawei Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/err.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mfd/hisi_hi6421v300.h>
#include "hisi_sim_hotplug.h"
#include <linux/wakelock.h>
#ifdef CONFIG_HISI_RPROC
#include <linux/huawei/hisi_rproc.h>
#endif
#include <linux/huawei/rdr.h>

struct sim_hotplug_state_info {
	u32 sim_msg_input_value;
	u32 sim_msg_input_flag;
	u32 sim_msg_output_value;
	u32 sim_msg_output_flag;
};

struct hisi_sim_hotplug_info {
	void __iomem *iomg_base;
	int	det_irq;
	int	det_gpio;
	struct workqueue_struct *sim_hotplug_det_wq;
	struct work_struct sim_hotplug_det_wk1;
	struct pinctrl *pctrl;
	struct pinctrl_state *sim_default;
	struct pinctrl_state *sim_idle;
	u8 sim_pluged;
	struct sim_hotplug_state_info sim_info;
	u32 sim_id;
	struct mutex sim_hotplug_lock;
	struct wake_lock  sim_hotplug_wklock;
};

enum sim_id {
	SIM0_ID = 0,
	SIM1_ID,
};

typedef struct {
	u32 time;
	u8 sim_no;
	u8 sim_status;
	u8 hpd_level;
	u8 det_level;
	u8 trace;
	u8 sim_mux[3];
} sim_log;

#define GPIO103_IOMG_ADDR	0x128
#define GPIO104_IOMG_ADDR	0x12c
#define GPIO105_IOMG_ADDR	0x130
#define GPIO106_IOMG_ADDR	0x134
#define GPIO107_IOMG_ADDR	0x138
#define GPIO108_IOMG_ADDR	0x13c
#define NO_NOTIFY		0
#define MAX_SIM_HOTPLUG_LOG	50
#define LOG_SIMHOTPLUG_BUF_LEN	600

sim_log *g_log_org;
u8 g_sim_log_idx = 0;

static void set_sim_status(struct hisi_sim_hotplug_info *info, u32 sim_no, u32 sim_status, u32 trace, u32 notify)
{
	struct timeval time;
	sim_log log_temp;

	do_gettimeofday(&time);
	log_temp.time = time.tv_sec;
	log_temp.sim_no = sim_no;
	log_temp.sim_status = sim_status;
	log_temp.hpd_level = -1;
	log_temp.det_level = gpio_get_value(info->det_gpio);
	log_temp.trace = trace;

	if (SIM0_ID == info->sim_id) {
		log_temp.sim_mux[0] = readl(info->iomg_base + GPIO103_IOMG_ADDR);
		log_temp.sim_mux[1] = readl(info->iomg_base + GPIO104_IOMG_ADDR);
		log_temp.sim_mux[2] = readl(info->iomg_base + GPIO105_IOMG_ADDR);
	} else if (SIM1_ID == info->sim_id) {
		log_temp.sim_mux[0] = readl(info->iomg_base + GPIO106_IOMG_ADDR);
		log_temp.sim_mux[1] = readl(info->iomg_base + GPIO107_IOMG_ADDR);
		log_temp.sim_mux[2] = readl(info->iomg_base + GPIO108_IOMG_ADDR);
	} else {
		pr_err("%s: sim id [%d] is err!\n", __func__, info->sim_id);
	}

	memcpy(&g_log_org[g_sim_log_idx], &log_temp, sizeof(sim_log));

	if (g_sim_log_idx >= MAX_SIM_HOTPLUG_LOG - 1) {
		g_sim_log_idx = 0;
	} else {
		g_sim_log_idx++;
	}
	pr_info("sim%d set:%d\n", sim_no, sim_status);

}

static int sim_set_active(struct hisi_sim_hotplug_info *info)
{
	int err = 0;

	err = pinctrl_select_state(info->pctrl, info->sim_default);
	if (err < 0) {
		pr_err("set iomux normal error, %d\n", err);
		return err;
	}

	pr_info("[%s]:set iomux normal state ok!\n", __func__);

	return 0;
}

static int sim_set_inactive(struct hisi_sim_hotplug_info *info)
{
	int err = 0;

	err = pinctrl_select_state(info->pctrl, info->sim_idle);
	if (err < 0) {
		pr_err("set iomux idle error, %d\n", err);
		return err;
	}

	pr_info("[%s]:set iomux idle state ok!\n", __func__);

	return 0;
}
#ifdef CONFIG_HISI_RPROC
static int hisi_sim_sem_msg_to_lpm3(struct hisi_sim_hotplug_info *info, int sim_num, int sim_state)
{
	int ret = 0;
	u32 tx_buffer[2] = {0};

	wake_lock(&(info->sim_hotplug_wklock));
	mutex_lock(&(info->sim_hotplug_lock));
	if (SIM_IN_POSITION == sim_state) {
		tx_buffer[0] = info->sim_info.sim_msg_input_value;
		tx_buffer[1] = info->sim_info.sim_msg_input_flag;
	} else if (SIM_LEAVE_POSITION == sim_state) {
		tx_buffer[0] = info->sim_info.sim_msg_output_value;
		tx_buffer[1] =  info->sim_info.sim_msg_output_flag;
	}

	if (SIM0_ID == sim_num) {
		pr_info("%s, %d, tx_buffer[0]=0x%x, tx_buffer[1]=0x%x\n",
			__func__, __LINE__, tx_buffer[0], tx_buffer[1]);
	} else if (SIM1_ID == sim_num) {
		pr_info("%s, %d, tx_buffer[0]=0x%x, tx_buffer[1]=0x%x\n",
			__func__, __LINE__, tx_buffer[0], tx_buffer[1]);
	}
	/*sim hotplug state notify lpm3*/
	ret = RPROC_ASYNC_SEND(HISI_RPROC_LPM3, tx_buffer, 2,
					ASYNC_MSG, NULL, NULL);
	if (ret == 0) {
		pr_info("%s, %d, SIM[%d] state[%d] hotplug notify lpm3 success.\n",
			__func__, __LINE__, sim_num, info->sim_pluged);
	} else {
		pr_err("%s, %d, failed to notify lpm3 sim hotplug state .\n\n",
			__func__, __LINE__);
	}
	mutex_unlock(&(info->sim_hotplug_lock));
	wake_unlock(&(info->sim_hotplug_wklock));

	return ret;
}
#endif
static void inquiry_sim_det_irq_reg(struct work_struct *work)
{
	struct hisi_sim_hotplug_info *sim_hotplug_info = container_of(work, struct hisi_sim_hotplug_info, sim_hotplug_det_wk1);
	int ret = 0;

	if (!gpio_get_value(sim_hotplug_info->det_gpio)) {
		if (sim_hotplug_info->sim_pluged == SIM_LEAVE_POSITION) {
			sim_hotplug_info->sim_pluged = SIM_IN_POSITION;
			sim_set_active(sim_hotplug_info);
#ifdef CONFIG_HISI_RPROC
			ret = hisi_sim_sem_msg_to_lpm3(sim_hotplug_info, sim_hotplug_info->sim_id, SIM_IN_POSITION);
			if (ret) {
				pr_err("%s:, input sim card[%d]: ap send mesage to lpm3 fail!\n", __func__, sim_hotplug_info->sim_id);
			}
#endif
			set_sim_status(sim_hotplug_info, sim_hotplug_info->sim_id, sim_hotplug_info->sim_pluged, 0, NO_NOTIFY);
		}
	} else if (gpio_get_value(sim_hotplug_info->det_gpio)) {
		if (sim_hotplug_info->sim_pluged == SIM_IN_POSITION) {
			sim_hotplug_info->sim_pluged = SIM_LEAVE_POSITION;
			sim_set_inactive(sim_hotplug_info);
#ifdef CONFIG_HISI_RPROC
			ret = hisi_sim_sem_msg_to_lpm3(sim_hotplug_info, sim_hotplug_info->sim_id, SIM_LEAVE_POSITION);
			if (ret) {
				pr_err("%s:, output sim card[%d]: ap send mesage to lpm3 fail!\n", __func__, sim_hotplug_info->sim_id);
			}
#endif
			set_sim_status(sim_hotplug_info, sim_hotplug_info->sim_id, sim_hotplug_info->sim_pluged, 1, NO_NOTIFY);
		}
	} else {
		set_sim_status(sim_hotplug_info, -1, -1, -1, NO_NOTIFY);
	}
}

static irqreturn_t sim_det_irq_handler(int irq, void *data)
{
	struct hisi_sim_hotplug_info *sim_hotplug_info = data;

	if (sim_hotplug_info == NULL) {
		pr_info("[%s]:%d get irq data is error\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	queue_work(sim_hotplug_info->sim_hotplug_det_wq, &sim_hotplug_info->sim_hotplug_det_wk1);

	return IRQ_HANDLED;
}

static int sim_hotplug_dt_init(struct hisi_sim_hotplug_info *info, struct device_node *np, struct device *dev)
{
	int ret = 0;
	enum of_gpio_flags flags;
	unsigned int sim_info[2] = {0};
	u32 sim_id = 0;

	ret = of_property_read_u32_array(np, "ap_to_lpm3_sim_input",
						sim_info, 2);
	if (ret) {
		dev_err(dev, "no ap_to_lpm3_sim_input state\n");
		return ret;
	}
	info->sim_info.sim_msg_input_value = sim_info[0];
	info->sim_info.sim_msg_input_flag = sim_info[1];

	ret = of_property_read_u32_array(np, "ap_to_lpm3_sim_output",
						sim_info, 2);
	if (ret) {
		dev_err(dev, "no ap_to_lpm3_sim_output state\n");
		return ret;
	}
	info->sim_info.sim_msg_output_value = sim_info[0];
	info->sim_info.sim_msg_output_flag = sim_info[1];

	ret = of_property_read_u32_array(np, "sim_id",
						&sim_id, 1);
	if (ret) {
		dev_err(dev, "no sim hotplug sim_id\n");
		return ret;
	}
	info->sim_id = sim_id;

	/*set det gpio to irq*/
	info->det_gpio = of_get_gpio_flags(np, 0, &flags);
	if (info->det_gpio < 0)
		return info->det_gpio;

	if (!gpio_is_valid(info->det_gpio))
		return -EINVAL;

	ret = gpio_request_one(info->det_gpio, GPIOF_IN, "sim_det");
	if (ret < 0) {
		pr_err("failed to request gpio%d\n", info->det_gpio);
		return ret;
	}

	info->det_irq = gpio_to_irq(info->det_gpio);

	/*create det workqueue*/
	info->sim_hotplug_det_wq = create_singlethread_workqueue("sim_hotplug_det");
	if (info->sim_hotplug_det_wq == NULL) {
		pr_err("failed to create work queue\n");
		return -ENOMEM;
	}

	INIT_WORK(&info->sim_hotplug_det_wk1, (void *)inquiry_sim_det_irq_reg);

	return ret;
}
static int sim_state_init(struct hisi_sim_hotplug_info *info, struct device *dev)
{
	int err = 0;

	info->pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(info->pctrl)) {
		dev_err(dev, "failed to devm pinctrl get\n");
		err = -EINVAL;
		return err;
	}

	info->sim_default = pinctrl_lookup_state(info->pctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(info->sim_default)) {
		dev_err(dev, "failed to pinctrl lookup state default\n");
		err = -EINVAL;
		return err;
	}

	info->sim_idle = pinctrl_lookup_state(info->pctrl, PINCTRL_STATE_IDLE);
	if (IS_ERR(info->sim_idle)) {
		dev_err(dev, "failed to pinctrl lookup state idle\n");
		err = -EINVAL;
		return err;
	}

	if (!gpio_get_value(info->det_gpio)) {
		info->sim_pluged = SIM_IN_POSITION;
		sim_set_active(info);
		set_sim_status(info, info->sim_id, info->sim_pluged, 2, NO_NOTIFY);
	} else {
		info->sim_pluged = SIM_LEAVE_POSITION;
		sim_set_inactive(info);
		set_sim_status(info, info->sim_id, info->sim_pluged, 3, NO_NOTIFY);
	}

	mutex_init(&(info->sim_hotplug_lock));
	wake_lock_init(&info->sim_hotplug_wklock, WAKE_LOCK_SUSPEND, "android-simhotplug");

	return 0;
}

static int get_iomux_base_addr(struct hisi_sim_hotplug_info *info)
{
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "pinctrl-single0");
	if (!np) {
		pr_err("failed to find pinctrl-single0 node!\n");
		return -1;
	}
	info->iomg_base = of_iomap(np, 0);
	if (!info->iomg_base) {
		pr_err("unable to map pinctrl-single0 dist registers!\n");
		return -1;
	}

	return 0;
}

static int hisi_sim_hotplug_probe(struct platform_device *pdev)
{
	struct hisi_sim_hotplug_info *info;
	struct device_node *np = NULL;
	struct device *dev = NULL;
	static int g_rdr_flag = 0;
	int ret = 0;
	u32 rdr_int = 0;

	if (pdev == NULL) {
		pr_err("[%s]sim_hotplug get  platform device para is err!\n", __func__);
		return -EINVAL;
	}

	dev = &pdev->dev;
	np = dev->of_node;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	ret = get_iomux_base_addr(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get iomux base addr\n");
		return ret;
	}

	if (g_rdr_flag == 0) {
		RDR_ASSERT(rdr_afreg(rdr_int, RDR_LOG_SIMHOTPLUG_BUF, RDR_U32, LOG_SIMHOTPLUG_BUF_LEN));
		g_log_org = (sim_log *)field_addr(u32, rdr_int);

		memset(g_log_org, 0xFF, MAX_SIM_HOTPLUG_LOG * sizeof(sim_log));
		g_rdr_flag = 1;
	}

	ret = sim_hotplug_dt_init(info, np, dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set sim hotplug gpio hw init\n");
		return ret;
	}

	ret = sim_state_init(info, dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set sim state hw init\n");
		goto free_sim_det_wq;
	}

	ret = request_threaded_irq(info->det_irq, sim_det_irq_handler, NULL,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
				   "sim_det", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to requset sim_det irq!\n");
		goto free_sim_lock;
	}

	platform_set_drvdata(pdev, info);

	return ret;

free_sim_lock:
	wake_lock_destroy(&info->sim_hotplug_wklock);
	mutex_destroy(&info->sim_hotplug_lock);
free_sim_det_wq:
	if (info->sim_hotplug_det_wq)
		destroy_workqueue(info->sim_hotplug_det_wq);

	return ret;
}

static int hisi_sim_hotplug_remove(struct platform_device *pdev)
{
	struct hisi_sim_hotplug_info *info;

	/*get hisi_sim_hotplug_info*/
	info = platform_get_drvdata(pdev);
	if (NULL == info) {
		pr_err("%s %d platform_get_drvdata NULL\n", __func__, __LINE__);
		return -1;
	}

	mutex_destroy(&info->sim_hotplug_lock);
	wake_lock_destroy(&info->sim_hotplug_wklock);

	if (info->sim_hotplug_det_wq)
		destroy_workqueue(info->sim_hotplug_det_wq);

	return 0;
}

static struct of_device_id hisi_sim_hotplug_of_match[] = {
	{ .compatible = "hisilicon,sim-hotplug0", },
	 { .compatible = "hisilicon,sim-hotplug1", },
	{ },
};
MODULE_DEVICE_TABLE(of, hisi_sim_hotplug_of_match);

#ifdef CONFIG_PM
static int hisi_sim_hotplug_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hisi_sim_hotplug_info *info;

	pr_info("[%s]:suspend+\n", __func__);
	/*get hisi_sim_hotplug_info*/
	info = platform_get_drvdata(pdev);
	if (NULL == info) {
		pr_err("%s %d platform_get_drvdata NULL\n", __func__, __LINE__);
		return -1;
	}

	if (!mutex_trylock(&info->sim_hotplug_lock)) {
		dev_err(&pdev->dev, "%s: mutex_trylock.\n", __func__);
		return -EAGAIN;
	}
	pr_info("[%s]:suspend-\n", __func__);
	return 0;
}

static int hisi_sim_hotplug_resume(struct platform_device *pdev)
{
	struct hisi_sim_hotplug_info *info;

	pr_info("[%s]:resume+\n", __func__);
	/*get hisi_sim_hotplug_info*/
	info = platform_get_drvdata(pdev);
	if (NULL == info) {
		pr_err("%s %d platform_get_drvdata NULL\n", __func__, __LINE__);
		return -1;
	}

	mutex_unlock(&(info->sim_hotplug_lock));
	pr_info("[%s]:resume-\n", __func__);
	return 0;
}
#endif

static struct platform_driver hisi_sim_hotplug_driver = {
	.probe		= hisi_sim_hotplug_probe,
	.remove		= hisi_sim_hotplug_remove,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "hisi-sim_hotplug",
		.of_match_table	= hisi_sim_hotplug_of_match,
	},
#ifdef CONFIG_PM
	.suspend        = hisi_sim_hotplug_suspend,
	.resume         = hisi_sim_hotplug_resume,
#endif
};

int __init hisi_sim_hotplug_init(void)
{
	return platform_driver_register(&hisi_sim_hotplug_driver);
}

static void __exit hisi_sim_hotplug_exit(void)
{
	platform_driver_unregister(&hisi_sim_hotplug_driver);
}

late_initcall(hisi_sim_hotplug_init);
module_exit(hisi_sim_hotplug_exit);

MODULE_DESCRIPTION("Sim hotplug driver");
MODULE_LICENSE("GPL v2");
