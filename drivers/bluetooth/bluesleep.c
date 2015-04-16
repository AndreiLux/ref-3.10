/*

   This	program	is free	software; you can redistribute it and/or modify
   it under	the	terms of the GNU General Public	License	version	2 as
   published by	the	Free Software Foundation.

   You should have received	a copy of the GNU General Public License
   along with this program;	if not,	write to the Free Software
   Foundation, Inc., 59	Temple Place, Suite	330, Boston, MA	 02111-1307	 USA

   This	program	is distributed in the hope that	it will	be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A	PARTICULAR PURPOSE.	 See the GNU General Public	License
   for more	details.
 */

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <net/bluetooth/bluetooth.h>
#include "hci_uart.h"

/*
 * Defines
 */
#define BT_SLEEP_DBG
#ifndef	BT_SLEEP_DBG
#define	BT_DBG(fmt,	arg...)
#endif

#define	VERSION		"1.1"
#define	PROC_DIR	"bluetooth/sleep"

#define DTS_COMP_BLUETOOTH_SLEEP_NAME "huawei,bluetooth_sleep"

/*
 *Define the GPIO polarity here
 */
#define LPM_BT_WAKE_ASSERT 1            /*Set 1 assert gpio host_wake_bt*/
#define LPM_BT_WAKE_DEASSERT 0        /*Set 0 deassert gpio host_wake_bt*/
#define LPM_HOST_WAKE_ASSERT 1        /*Set 1 assert gpio bt_wake_host*/
#define LPM_HOST_WAKE_DEASSERT 0        /*Set 0 deassert gpio bt_wake_host*/

typedef struct bluetooth_sleep_private_data {
	struct gpio	gpio_enable;		// gpio BT_enable
	struct gpio	gpio_bt_wake_host;		// gpio BT_wake_host
	struct gpio	gpio_host_wake_bt;		// gpio host_wake_BT
	struct semaphore bt_seam;
	struct wake_lock wake_lock;
	struct timer_list tx_timer; /* Transmission check timer */
	struct tasklet_struct hostwake_task; /*Tasklet to respond to change in hostwake line */
       struct proc_dir_entry *bluetooth_dir;
	struct proc_dir_entry *sleep_dir;
	unsigned long flags; /*Global state flags */
	int host_wake_irq;
	spinlock_t rw_lock; /*Lock for state transitions */
}bluetooth_sleep_data;

/* work	function */
static void	bluesleep_sleep_work(struct	work_struct	*work);
/* work	queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define	bluesleep_rx_busy() schedule_delayed_work(&sleep_workqueue,	0)
#define	bluesleep_tx_busy()	schedule_delayed_work(&sleep_workqueue,	0)
#define	bluesleep_rx_idle()	schedule_delayed_work(&sleep_workqueue,	0)
#define	bluesleep_tx_idle()	schedule_delayed_work(&sleep_workqueue,	0)

/* 1 second	timeout	*/
#define	TX_TIMER_INTERVAL	1
/* state variable names	and	bit	positions */
#define	BT_PROTO	0x01
#define	BT_TXDATA	0x02
#define	BT_ASLEEP	0x04

static struct bluetooth_sleep_private_data  *g_bs_data = NULL;

/**
 * @return 1 if	the	Host can go to sleep, 0 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
    /* check if gpio ext_wake and gpio host_wake are both deasserted */
	return (LPM_BT_WAKE_DEASSERT == gpio_get_value(g_bs_data->gpio_host_wake_bt.gpio)) &&
		(LPM_HOST_WAKE_DEASSERT == gpio_get_value(g_bs_data->gpio_bt_wake_host.gpio));

}
static void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP,	&g_bs_data->flags)) {
		printk("bt waking up...\n");
		/* Start the timer */
		mod_timer(&g_bs_data->tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		gpio_set_value(g_bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_ASSERT);
		clear_bit(BT_ASLEEP, &g_bs_data->flags);
		wake_lock(&g_bs_data->wake_lock);
	}
	else {
	/*Tx idle, Rx	busy, we must also make	host_wake asserted,	that is	low
	* 1 means bt chip can sleep, in bluesleep.c
	*/
	/* Here we depend on the status of gpio ext_wake, for stability	*/
		if (LPM_BT_WAKE_DEASSERT == gpio_get_value(g_bs_data->gpio_host_wake_bt.gpio)) {
			printk("bluesleep_sleep_wakeup wakeup bt chip\n");
			gpio_set_value(g_bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_ASSERT);
			mod_timer(&g_bs_data->tx_timer, jiffies +	(TX_TIMER_INTERVAL * HZ));
		}
	}
}

/**
 * @brief@	main sleep work	handling function which	update the flags
 * and activate	and	deactivate UART	,check FIFO.
 */
static void	bluesleep_sleep_work(struct	work_struct	*work)
{
	if (bluesleep_can_sleep()) {
	    printk("bluesleep_can_sleep\n");
		if (test_bit(BT_ASLEEP,	&g_bs_data->flags)) {
			printk("already	asleep");
			return;
		}
		printk("bt going to sleep...\n");
		set_bit(BT_ASLEEP, &g_bs_data->flags);
		wake_lock_timeout(&g_bs_data->wake_lock, HZ/2);
	} else {
	    printk("bluesleep_sleep_wakeup\n");
		bluesleep_sleep_wakeup();
	}
}

/**
 * A tasklet function that runs	in tasklet context and reads the value
 * of the HOST_WAKE	GPIO pin and further defer the work.
 * @param data Not used.
 */
static void	bluesleep_hostwake_task(unsigned long data)
{
	struct bluetooth_sleep_private_data *bs_data = (struct bluetooth_sleep_private_data *)data;
	unsigned long irq_flags;     //DTS2014030606211 z00182537 2014.0306 add
	if (!bs_data) {
		BT_ERR("%s: bs_data is NULL\n", __func__);
		return ;
	}
	printk("hostwake line change\n");

	//DTS2014030606211 z00182537 2014.0306 modify begin
	spin_lock_irqsave(&bs_data->rw_lock, irq_flags);

	if (LPM_HOST_WAKE_ASSERT == gpio_get_value(bs_data->gpio_bt_wake_host.gpio))
	{
		printk("bluesleep_rx_busy\n");
		bluesleep_rx_busy();
	}
	else
	{
		printk("bluesleep_rx_idle\n");
		bluesleep_rx_idle();
	}

	spin_unlock_irqrestore(&bs_data->rw_lock, irq_flags);
	//DTS2014030606211 z00182537 2014.0306 modify end
}

/**
 * Handles proper timer	action when	outgoing data is delivered to the
 * HCI line	discipline.	Sets BT_TXDATA.
 */
static void	bluesleep_outgoing_data(struct bluetooth_sleep_private_data *bs_data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&bs_data->rw_lock, irq_flags);
	/* log data	passing	by */
	set_bit(BT_TXDATA, &bs_data->flags);

	printk("bluesleep_outgoing_data\n");

	/* if the tx side is sleeping... */
	if (LPM_BT_WAKE_DEASSERT == gpio_get_value(bs_data->gpio_host_wake_bt.gpio)) {

		printk("tx was sleeping\n");
		bluesleep_sleep_wakeup();
	}

	spin_unlock_irqrestore(&bs_data->rw_lock, irq_flags);
}

/**
 * Handles transmission	timer expiration.
 * @param data Not used.
 */
static void	bluesleep_tx_timer_expire(unsigned long	data)
{
	unsigned long irq_flags = 0;
	struct bluetooth_sleep_private_data *bs_data = (struct bluetooth_sleep_private_data *)data;

	if (NULL == bs_data) {
		BT_ERR("%s: bluetooth_sleep_private_data is null\n", __func__);
		return;
	}

	spin_lock_irqsave(&bs_data->rw_lock, irq_flags);

	printk("Tx timer expired\n");

	/* were	we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &bs_data->flags)) {
		printk("Tx has been	idle\n");
		gpio_set_value(bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_DEASSERT);
		bluesleep_tx_idle();
	} else {
		printk("Tx data	during last	period\n");
		mod_timer(&bs_data->tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag	*/
	spin_unlock_irqrestore(&bs_data->rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an	interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void	*dev_id)
{
	/* schedule	a tasklet to handle	the	change
	* in the host wake	line
	*/
	struct bluetooth_sleep_private_data *bs_data = (struct bluetooth_sleep_private_data *)dev_id;

	printk("bluesleep_hostwake_isr===>>\n");
	tasklet_schedule(&bs_data->hostwake_task);
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error,	-1,	and	<code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(struct bluetooth_sleep_private_data *bs_data)
{
	int	retval = 0;

	down(&bs_data->bt_seam);
	if (test_bit(BT_PROTO, &bs_data->flags))	{
		BT_ERR("%s: bluesleep already start\n", __func__);
		up(&bs_data->bt_seam);
		return retval;
	}

	printk("%s, bluetooth power manage unit start +.\n", __func__);

	retval = request_irq(bs_data->host_wake_irq, bluesleep_hostwake_isr,
			IRQF_DISABLED |	IRQF_TRIGGER_RISING
				| IRQF_NO_SUSPEND, "bluetooth hostwake", bs_data);
	if (retval	< 0) {
		BT_ERR("%s: couldn't acquire BT_HOST_WAKE IRQ", __func__);
		goto err_request_irq;
	}

	retval = enable_irq_wake(bs_data->host_wake_irq);
	if (retval < 0)	{
		BT_ERR("%s: couldn't enable	BT_HOST_WAKE as	wakeup interrupt %d", __func__, retval);
		goto err_en_irqwake;
	}

	/* assert BT_WAKE */
	gpio_set_value(bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_ASSERT);

	/* start the timer */
	mod_timer(&bs_data->tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));

	set_bit(BT_PROTO, &bs_data->flags);

	wake_lock(&bs_data->wake_lock);

	up(&bs_data->bt_seam);
	printk("%s, bluetooth power manage unit start -.\n", __func__);

	return retval;

err_en_irqwake:
	free_irq(bs_data->host_wake_irq, bs_data);
err_request_irq:
	up(&bs_data->bt_seam);
	return retval;
}

/**
 * Stops the Sleep-Mode	Protocol on	the	Host.
 */
static void	bluesleep_stop(struct bluetooth_sleep_private_data *bs_data)
{
	down(&bs_data->bt_seam);

	if (!test_bit(BT_PROTO,	&bs_data->flags)) {
		up(&bs_data->bt_seam);
		BT_ERR("%s: bluesleep already stop\n", __func__);
		return;
	}

	printk("%s, bluetooth power manage unit stop +.\n", __func__);

	/* assert BT_WAKE */
	gpio_set_value(bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_ASSERT);

	cancel_delayed_work_sync(&sleep_workqueue);
	del_timer_sync(&bs_data->tx_timer);

	if (test_bit(BT_ASLEEP,	&bs_data->flags)) {
		clear_bit(BT_ASLEEP, &bs_data->flags);
	}

	if (disable_irq_wake(bs_data->host_wake_irq))
		BT_ERR("%s: couldn't disable hostwake IRQ wakeup mode\n", __func__);
	if (bs_data->host_wake_irq)
		free_irq(bs_data->host_wake_irq, bs_data);

	clear_bit(BT_PROTO,	&bs_data->flags);

	wake_unlock(&bs_data->wake_lock);

	up(&bs_data->bt_seam);
	printk("%s, bluetooth power manage unit stop -.\n", __func__);
}

/**
 * Read	the	<code>BT_WAKE</code> GPIO pin value	via	the	proc interface.
 * When	this function returns, <code>page</code> will contain a	1 if the
 * pin is high,	0 otherwise.
 * @param page Buffer for writing data.
 * @param start	Not	used.
 * @param offset Not used.
 * @param count	Not	used.
 * @param eof Whether or not there is more data	to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static ssize_t bluesleep_ap_wakeup_bt(struct file *filp, const char __user *buffer, size_t len, loff_t *off)
{
	char buf;
	unsigned long irq_flags;     //DTS2014030606211 z00182537 2014.0306 add
	if (len < 1 || (NULL == g_bs_data))
		return -EINVAL;

    printk("%s, bluesleep_ap_wakeup_bt \n", __func__);

	if (copy_from_user(&buf, buffer, sizeof(buf)))
		return -EFAULT;

	if (buf == '0') {
		printk("%s, ap_wakeup_bt 0 \n", __func__);
		//DTS2014030606211 z00182537 2014.0306 modify begin
		spin_lock_irqsave(&g_bs_data->rw_lock, irq_flags);
		clear_bit(BT_TXDATA, &g_bs_data->flags);
		spin_unlock_irqrestore(&g_bs_data->rw_lock, irq_flags);
		//DTS2014030606211 z00182537 2014.0306 modify end
	} else
		bluesleep_outgoing_data(g_bs_data);

	return len;
}

/**
 * Modify the low-power	protocol used by the Host via the proc interface.
 * @param file Not used.
 * @param buffer The buffer	to read	from.
 * @param count	The	number of bytes	to be written.
 * @param data Not used.
 * @return On success, the number of bytes written.	On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static ssize_t bluesleep_write_proc_proto(struct file *filp, const char __user *buffer, size_t len, loff_t *off)
{
	char proto = 0;

    if ((len < 1) || (NULL == g_bs_data))
		return -EINVAL;

	if (copy_from_user(&proto, buffer, sizeof(proto)))
		return -EFAULT;

	if (proto == '0')
		bluesleep_stop(g_bs_data);
	else if (proto == '1')
		bluesleep_start(g_bs_data);
	else
		BT_ERR("bluesleep_write_proc_proto error %d\n",
			proto);

	/* claim that we wrote everything */
	return len;
}

static const struct file_operations bt_proc_fops_proto= {
    .owner = THIS_MODULE,
    .write = bluesleep_write_proc_proto,
};

static const struct file_operations bt_proc_fops_btwrite= {
   .owner = THIS_MODULE,
   .write  = bluesleep_ap_wakeup_bt,
};

static int create_bluesleep_proc_sys(struct bluetooth_sleep_private_data *bs_data)
{
	int	retval = 0;

	bs_data->bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bs_data->bluetooth_dir == NULL) {
		BT_ERR("%s: unable to create /proc/bluetooth directory", __func__);
		retval = -ENOMEM;
		return retval;
	}

	bs_data->sleep_dir = proc_mkdir("sleep", bs_data->bluetooth_dir);
	if (bs_data->sleep_dir == NULL) {
		BT_ERR("%s: unable to create /proc/%s directory", __func__, PROC_DIR);
		retval = -ENOMEM;
		goto err_proc_sleep;
	}

	/* write proc entries proto*/
	proc_create("proto", S_IWUGO | S_IFREG, bs_data->sleep_dir, &bt_proc_fops_proto);

	/* write entries btwrite */
	proc_create("btwrite", S_IWUGO | S_IFREG, bs_data->sleep_dir, &bt_proc_fops_btwrite);

	return retval;

err_proc_sleep:
    remove_proc_entry("bluetooth", NULL);

	return retval;
}

static int  bluesleep_probe(struct platform_device *pdev)
{
	struct device *bluetooth_sleep_dev = &pdev->dev;
	struct device_node *np = bluetooth_sleep_dev->of_node;
	struct device_node *np_power = NULL;
	struct bluetooth_sleep_private_data *pdev_bluetooth_sleep_data = NULL;
	int ret = 0;

	pdev_bluetooth_sleep_data = devm_kzalloc(bluetooth_sleep_dev, sizeof(struct bluetooth_sleep_private_data), GFP_KERNEL);
	if ( pdev_bluetooth_sleep_data == NULL ) {
		dev_err(bluetooth_sleep_dev, "failed to allocate bluetooth_sleep_private_data\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "bluesleep_probe in \n");

	np_power = of_parse_phandle(np, "sleep_handle_name", 0);
	if(np_power == NULL)
    {
        BT_ERR("%s: get device node blutooth_power failed\n", __func__);
	    goto free_g_bs_data;
    }

	of_property_read_u32(np_power, "huawei,bt_en", &(pdev_bluetooth_sleep_data->gpio_enable.gpio));

	of_property_read_u32(np_power, "huawei,bt_wake_host", &(pdev_bluetooth_sleep_data->gpio_bt_wake_host.gpio));

	of_property_read_u32(np_power, "huawei,host_wake_bt", &(pdev_bluetooth_sleep_data->gpio_host_wake_bt.gpio));

    /*
	ret	= gpio_request(pdev_bluetooth_sleep_data->gpio_host_wake_bt.gpio, "bt_host_wake");
	printk(KERN_INFO " gpio_request ret:%d \n", ret);
	if (ret < 0) {
		BT_ERR("%s: bt_host_wake gpio_request failed\n", __func__);
		goto free_g_bs_data;
	}
	*/

	ret	= gpio_direction_input(pdev_bluetooth_sleep_data->gpio_bt_wake_host.gpio);  //213
	if (ret < 0) {
		BT_ERR("%s: bt_host_wake gpio_direction_input failed\n", __func__);
		goto free_bt_host_wake;
	}

    /*
	ret	= gpio_request(pdev_bluetooth_sleep_data->gpio_bt_wake_host.gpio, "bt_wake_host");
	if (ret < 0) {
		BT_ERR("%s: gpio_request bt_wake_host failed\n", __func__);
		goto free_bt_host_wake;
	}
	*/

	/* deassert bt wake */
	ret	= gpio_direction_output(pdev_bluetooth_sleep_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_DEASSERT); //164
	if (ret < 0) {
		BT_ERR("%s: bt_ext_wake gpio_direction_input failed\n", __func__);
		goto free_bt_ext_wake;
	}

	pdev_bluetooth_sleep_data->host_wake_irq = gpio_to_irq(pdev_bluetooth_sleep_data->gpio_bt_wake_host.gpio);  //213
	if (pdev_bluetooth_sleep_data->host_wake_irq < 0)	{
		BT_ERR("%s: couldn't find host_wake	irq\n", __func__);
		ret	= -ENODEV;
		goto free_bt_ext_wake;
	}

	ret = create_bluesleep_proc_sys(pdev_bluetooth_sleep_data);
	if (ret) {
		BT_ERR("%s: create_bluesleep_proc_sys %d\n",
			__func__, ret);
		goto free_bt_ext_wake;
	}

	/* Initialize spinlock.*/
	spin_lock_init(&pdev_bluetooth_sleep_data->rw_lock);

	/* Initialize semaphore.*/
	sema_init(&pdev_bluetooth_sleep_data->bt_seam, 1);

	/* Initialize bluetooth wakelock*/
	wake_lock_init(&pdev_bluetooth_sleep_data->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");

	/* Initialize timer	*/
	init_timer(&pdev_bluetooth_sleep_data->tx_timer);
	pdev_bluetooth_sleep_data->tx_timer.function = bluesleep_tx_timer_expire;
	pdev_bluetooth_sleep_data->tx_timer.data = (unsigned long)pdev_bluetooth_sleep_data;

	/* initialize host wake	tasklet	*/
	tasklet_init(&pdev_bluetooth_sleep_data->hostwake_task,
			bluesleep_hostwake_task, (unsigned long)pdev_bluetooth_sleep_data);

	// other code
	platform_set_drvdata(pdev, pdev_bluetooth_sleep_data);
	g_bs_data = pdev_bluetooth_sleep_data;

	printk(KERN_INFO "bluesleep_probe out \n");

	return ret;

free_bt_ext_wake:
	gpio_free(pdev_bluetooth_sleep_data->gpio_host_wake_bt.gpio);
free_bt_host_wake:
	gpio_free(pdev_bluetooth_sleep_data->gpio_bt_wake_host.gpio);
free_g_bs_data:
	platform_set_drvdata(pdev, NULL);
	kfree(pdev_bluetooth_sleep_data);
	pdev_bluetooth_sleep_data = NULL;

	return ret;
}

static int  bluesleep_remove(struct platform_device *pdev)
{
	struct bluetooth_sleep_private_data *bs_data = platform_get_drvdata(pdev);

	if (NULL == bs_data) {
		dev_err(&pdev->dev,	"%s, bs_data is null \n", __func__);
		return -1;
	}

	printk("[%s],+\n", __func__);

	/* deassert bt wake */
	gpio_set_value(bs_data->gpio_host_wake_bt.gpio, LPM_BT_WAKE_DEASSERT);

	if (test_bit(BT_PROTO, &bs_data->flags)) {
		if (disable_irq_wake(bs_data->host_wake_irq))
			BT_ERR("%s: couldn't disable hostwake IRQ wakeup mode\n", __func__);
		if (bs_data->host_wake_irq)
			free_irq(bs_data->host_wake_irq, bs_data);

		del_timer_sync(&bs_data->tx_timer);
	}

	gpio_free(bs_data->gpio_bt_wake_host.gpio);
	gpio_free(bs_data->gpio_host_wake_bt.gpio);

	remove_proc_entry("btwrite", bs_data->sleep_dir);
	remove_proc_entry("proto", bs_data->sleep_dir);
	remove_proc_entry("sleep", bs_data->bluetooth_dir);
	remove_proc_entry("bluetooth", 0);

	wake_lock_destroy(&bs_data->wake_lock);

	kfree(bs_data);
	bs_data = NULL;

	platform_set_drvdata(pdev, NULL);

	printk("[%s],-\n", __func__);

	return 0;
}

static void bluesleep_shutdown(struct platform_device *pdev)
{
	struct bluetooth_sleep_private_data *bs_data = platform_get_drvdata(pdev);

	if (NULL == bs_data) {
		BT_ERR("%s: bs_data is null\n", __func__);
		return;
	}

	printk("[%s],+\n", __func__);

	printk("[%s],-\n", __func__);

	return;
}

static const struct of_device_id bluetooth_sleep_match_table[] = {
	{
		.compatible = DTS_COMP_BLUETOOTH_SLEEP_NAME,   // compatible must match with which defined in dts
		.data = NULL,
	},
	{
	},
};

static struct platform_driver bluesleep_driver = {
	.probe  = bluesleep_probe,
	.remove	= bluesleep_remove,
	.shutdown  = bluesleep_shutdown,
	.driver	= {
		.name =	"bluesleep",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(bluetooth_sleep_match_table), // dts required code
	},
};
/**
 * Initializes the module.
 * @return On success, 0. On error,	-1,	and	<code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int	retval = 0;
	retval	= platform_driver_register(&bluesleep_driver);
	if (retval) {
		BT_ERR("%s:	platform_driver_register failed %d\n",
				__func__, retval);
	}
	return retval;
}

/**
 * Cleans up the module.
 */
static void	__exit bluesleep_exit(void)
{
	platform_driver_unregister(&bluesleep_driver);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver	ver	%s " VERSION);
MODULE_LICENSE("GPL");

