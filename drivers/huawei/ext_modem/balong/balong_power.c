/*
 * kernel/drivers/huawei/modem/balong/balong_power.c
 *
 * Copyright (C) 2011 Hisilicon Co. Ltd.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/random.h>
#include <linux/time.h>
#include <linux/compiler.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>
#include <linux/hw_log.h>
#include <linux/io.h>
#include <linux/switch_usb.h>
#include <linux/huawei/usb/hisi_usb.h>

#include "balong_power.h"

#undef HWLOG_TAG
#define HWLOG_TAG balong_ext_modem
HWLOG_REGIST();


MODULE_LICENSE("GPL");


/************************************************************
                    MOCRO   DEFINES
*************************************************************/

/*ehci power on/off node*/
#define ECHI_DEVICE_FILE            "/sys/devices/ff07e000.hsic/ehci_power"

/*usb autosuspend node*/
#define USBCORE_AUTOSUSPEND         "/sys/module/usbcore/parameters/autosuspend"

/*usb switch node*/
#define USB_SWITCH_STATE            "/sys/devices/virtual/usbswitch/usbsw/swstate"

/*hsic connect status check reg*/
#define REG_BASE_USB2EHCI           0xFF07E000

/*define balong power name, must be same with dts*/
#define BALONG_POWER_DRIVER_NAME    "huawei,balong_ext_power"

/******************
*
* Modem Power On/Off state,
* for MDM_DLOADER use
*
******************/
#define POWER_SET_DEBUGOFF          0   /*modem power off*/
#define POWER_SET_DEBUGON           1   /*modem power on*/
#define POWER_SET_OFF               2   /*hsic power off*/
#define POWER_SET_ON                3   /*hsic power on*/
#define POWER_SET_HSIC_RESET        4   /*hsic reset*/

static const char *power_state_name[] =
{
    "POWER_OFF",
    "INIT_ON",
    "WAIT_READY",
    "INIT_USB_ON",
    "USB_ENUM_DONE",
    "USB_L0",
    "USB_L2",
    "USB_L2_TO_L0",
    "USB_L3",
    "USB_L3_ENUM",
};

const char* const modem_state_str[] = {
    "MODEM_STATE_OFF",	                // Modem下电状态，含：硬件下电、软件下电及异常下电；
    "MODEM_STATE_POWER",	            // Modem上电状态；
    "MODEM_STATE_READY",	            // Modem初始化完成状态，上电且代码初始化完成状态；
    "MODEM_STATE_INVALID",              //Invalid case
};

#define MAX_ENUM_WAIT               1000
#define MAX_ENUM_RETRY              20
#define MAX_ENUM_RESUME_RETRY       20
#define MAX_HSIC_RESET_RETRY        3
#define MAX_RESUME_RETRY_TIME       3


#define accu_udelay(usecs)          udelay(usecs)
#define accu_mdelay(msecs)          mdelay(msecs)

#define my_wake_lock(fg) \
    {\
    wake_lock(fg); \
    hwlog_info("balong_wake_lock\n"); \
    }

#define my_wake_unlock(fg) \
    {\
    wake_unlock(fg); \
    hwlog_info("balong_wake_unlock\n"); \
    }

/*for modem power on signal*/
#define GPIO_CP_PM_RDY              1
/*for modem ready signal*/
#define GPIO_CP_SW_RDY              0


DEFINE_MUTEX(balong_receive_sem);

/************************************************************
                    STATIC  VARIABLE  DEFINES
*************************************************************/
static enum BALONG_POWER_STATE_E        balong_curr_power_state;
static struct wake_lock             balong_wakelock;
static struct workqueue_struct      *workqueue_usb;
static struct workqueue_struct      *workqueue_ril;
static struct work_struct           l2_resume_work;
static struct work_struct           l3_resume_work;
static struct work_struct           modem_reset_work;
static struct work_struct           modem_ready_work;

static wait_queue_head_t            balong_wait_q;
static spinlock_t                   balong_state_lock;

static int                          usb_on_delay = 100;
static int                          modem_reset_process = 0;
static spinlock_t                   modem_reset_lock;

static int                          reset_isr_register = 0;

/********************************
    netlink variables for
    communicate between kernel and ril
*********************************/
struct balong_nl_packet_msg {
	int balong_event;
};

typedef enum Balong_KnlMsgType {
    NETLINK_BALONG_REG = 0,    /* send from rild to register the PID for netlink kernel socket */
    NETLINK_BALONG_KER_MSG,    /* send from balong to rild */
    NETLINK_BALONG_UNREG       /* when rild exit send this type message to unregister */
} BALONG_MSG_TYPE_EN;

static struct sock *balong_nlfd = NULL;
static unsigned int balong_rild_pid = 0;



static struct balong_power_plat_data* balong_driver_plat_data;




/********************************
    These are for usb switch
    between ap and modem
*********************************/
static volatile int                 usb_switch_status           = USB_TURNED_OFF;
static volatile int                 usb_cable_connected_status  = USB_CABLE_NOT_CONNECTED;

static int switch_usb_notifier_call_for_balong(struct notifier_block *this,
                    unsigned long code, void *cmd);

static struct notifier_block switch_usb_notifier = {
    .notifier_call = switch_usb_notifier_call_for_balong,
};

static int charger_notifier_call_for_balong(struct notifier_block *this,
                    unsigned long code, void *cmd);

static struct notifier_block charger_notifier = {
    .notifier_call = charger_notifier_call_for_balong,
};

/************************************************************
                    STATIC  FUNCTION  DEFINES
*************************************************************/

/*
 * netlink process functions, for notice ril when modem status changed
 * balong_netlink_init          ---> netlink init
 * balong_netlink_deinit        ---> netlink deinit
 * balong_change_notify_event   ---> send message to ril
 * kernel_balong_receive        ---> receive message from ril
 */
static void kernel_balong_receive(struct sk_buff *__skb)
{
    struct nlmsghdr *nlh;
	struct sk_buff *skb;

    hwlog_info("balong_power: kernel_balong_receive +\n");

	skb = skb_get(__skb);

    mutex_lock(&balong_receive_sem);

    if (skb->len >= NLMSG_HDRLEN) {
        nlh = nlmsg_hdr(skb);

        if ((nlh->nlmsg_len >= sizeof(struct nlmsghdr))&&
            (skb->len >= nlh->nlmsg_len)) {
            if (nlh->nlmsg_type == NETLINK_BALONG_REG) {
                balong_rild_pid = nlh->nlmsg_pid;
                hwlog_info("netlink receive reg packet: balong_rild_pid = %d\n",
                    balong_rild_pid);
            } else if(nlh->nlmsg_type == NETLINK_BALONG_UNREG) {
                hwlog_info("netlink receive reg packet\n");
                balong_rild_pid = 0;
            }
        }
    }
    mutex_unlock(&balong_receive_sem);
}

static void balong_netlink_init(void)
{
	struct netlink_kernel_cfg balong_nl_cfg = {
		.input = kernel_balong_receive,
	};

    balong_nlfd = netlink_kernel_create(&init_net,
                                   NETLINK_BALONG_RIL,
                                   &balong_nl_cfg);
    if(!balong_nlfd)
        hwlog_info("%s: netlink_kernel_create failed\n",__func__);
    else
        hwlog_info("%s: netlink_kernel_create success\n",__func__);

    return;
}

static void balong_netlink_deinit(void)
{
    if(balong_nlfd && balong_nlfd->sk_socket)
    {
        sock_release(balong_nlfd->sk_socket);
        balong_nlfd = NULL;
    }
}

/*
 * balong_change_notify_event
 * create uevent to signal ril do something
 * according to current modem event
 */
static int balong_change_notify_event(int event)
{
    int ret;
    int size;
    struct sk_buff *skb = NULL;
    struct nlmsghdr *nlh = NULL;
    struct balong_nl_packet_msg *packet = NULL;

    mutex_lock(&balong_receive_sem);
    if (!balong_rild_pid || !balong_nlfd ) {
        hwlog_err("%s: cannot notify event, g_rild_pid = %d\n",
            __func__,
            balong_rild_pid);
        ret = -1;
        goto end;
    }
    size = sizeof(struct balong_nl_packet_msg);
    skb = nlmsg_new(size, GFP_ATOMIC);
    if (!skb) {
        hwlog_info("%s: alloc skb fail\n",__func__);
        ret = -1;
        goto end;
    }
    nlh = nlmsg_put(skb, 0, 0, NETLINK_BALONG_KER_MSG, size, 0);
    if (!nlh) {
        kfree_skb(skb);
        skb = NULL;
        ret = -1;
        goto end;
    }
    packet = nlmsg_data(nlh);
    memset(packet, 0, sizeof(struct balong_nl_packet_msg));
    packet->balong_event = event;
    ret = netlink_unicast(balong_nlfd, skb, balong_rild_pid, MSG_DONTWAIT); //skb will be freed in netlink_unicast
    goto end;

end:
    mutex_unlock(&balong_receive_sem);
    return ret;
}


static void balong_power_reset_work(struct work_struct *work)
{
    /**/
    balong_change_notify_event(BALONG_STATE_RESET);
}

static void balong_modem_ready_work(struct work_struct *work)
{
    balong_change_notify_event(BALONG_STATE_READY);
}
/*
* GPIO_CP_SW_INDICATION hard irq
*/
static irqreturn_t mdm_sw_state_isr(int irq, void *dev_id)
{
    if(!balong_driver_plat_data)
        return IRQ_NONE;

    if(GPIO_CP_SW_RDY == gpio_get_value(balong_driver_plat_data->balong_slave_active )) {
        schedule_work(&modem_ready_work);
        hwlog_info("modem ready received\n");
    }

    return IRQ_HANDLED;
}


static irqreturn_t modem_reset_isr(int irq, void *dev_id)
{
    unsigned long flags;
    struct device* dev = (struct device*)dev_id;

    dev_info(dev, "modem reset abnorm, notice ril.\n");

    spin_lock_irqsave(&modem_reset_lock, flags);
    if(modem_reset_process != 1) {
        modem_reset_process = 1;
        spin_unlock_irqrestore(&modem_reset_lock, flags);

        if(!wake_lock_active(&balong_wakelock))
            wake_lock_timeout(&balong_wakelock, 5 * HZ);

        queue_work(workqueue_ril, &modem_reset_work);
   } else {
        spin_unlock_irqrestore(&modem_reset_lock, flags);
        dev_info(dev, "Modem reset is in process, do nothing!\n");
   }
   return IRQ_HANDLED;
}

static void request_reset_isr(struct device *dev)
{
    int status;
    unsigned long flags;

    if(!balong_driver_plat_data)
        return;

    if (reset_isr_register == 1)
        return;
    status = request_irq(gpio_to_irq(balong_driver_plat_data->balong_reset_ind),
                            modem_reset_isr,
                            IRQF_TRIGGER_FALLING|
                            IRQF_NO_SUSPEND,
                            "RESET_IND_IRQ",
                            dev);
    if (status) {
        hwlog_err("Request irq for balong reset failed, status = %d\n", status);
        reset_isr_register = 0;
        return;
    }
    reset_isr_register = 1;
    irq_set_irq_wake(gpio_to_irq(balong_driver_plat_data->balong_reset_ind), true);

    spin_lock_irqsave(&modem_reset_lock, flags);
    modem_reset_process = 0;
    spin_unlock_irqrestore(&modem_reset_lock, flags);
}

static void free_reset_isr(struct device *dev)
{
    if(!balong_driver_plat_data)
        return;

    if (reset_isr_register == 1)
        free_irq(gpio_to_irq(balong_driver_plat_data->balong_reset_ind), dev);

    reset_isr_register = 0;
}

static int get_ehci_connect_status(int* status)
{
    mm_segment_t oldfs;
    struct file *filp;
    char buf[4] = {0};
    int ret = 0;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(ECHI_DEVICE_FILE, O_RDWR, 0);

    if (!filp || IS_ERR(filp)) {
        hwlog_info("balong_power: Invalid EHCI filp=%ld\n", PTR_ERR(filp));
        ret = -EINVAL;
        set_fs(oldfs);
        return ret;
    } else {
        filp->f_op->read(filp, buf, 1, &filp->f_pos);
    }

    if (kstrtoint(buf, 10, status) != 0) {
        hwlog_info("balong_power: Invalid result get from ehci\n");
        ret = -EINVAL;
    }

    filp_close(filp, NULL);
    set_fs(oldfs);

    return ret;
}

static bool is_echi_connected(void)
{
    int status = 0;
    int retry = 100;

    for (; retry > 0; retry--) {
        get_ehci_connect_status(&status);
        if (status == 1)
            return true;
        msleep(50);
    }

    return false;
}

/* Enable/disable USB host controller and HSIC phy */
static void enable_usb_hsic(bool enable)
{
    mm_segment_t oldfs;
    struct file *filp;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(ECHI_DEVICE_FILE, O_RDWR, 0);
    hwlog_info("balong_power: EHCI filp=%ld\n", PTR_ERR(filp));

    if (!filp || IS_ERR(filp)) {
        /* Fs node not exit, register the ECHI device */
        /*
        if (enable && xmm_driver_plat_data &&
            xmm_driver_plat_data->echi_device){
            platform_device_register(xmm_driver_plat_data->echi_device);
        }
        */
        hwlog_err("balong_power: failed to access ECHI\n");
    } else {
        if (enable) {
            filp->f_op->write(filp, "1", 1, &filp->f_pos);
        }
        else {
            filp->f_op->write(filp, "0", 1, &filp->f_pos);
        }
        filp_close(filp, NULL);
    }
    set_fs(oldfs);
}

static void set_usb_hsic_suspend(bool suspend)
{
    mm_segment_t oldfs;
    struct file *filp;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    filp = filp_open(ECHI_DEVICE_FILE, O_RDWR, 0);

    if (!filp || IS_ERR(filp)) {
        hwlog_err("balong_power: Can not open EHCI file. filp=%ld\n", PTR_ERR(filp));
    } else {
        if (suspend) {
            filp->f_op->write(filp, "2", 1, &filp->f_pos);
        }
        else {
            filp->f_op->write(filp, "3", 1, &filp->f_pos);
        }
        filp_close(filp, NULL);
    }
    set_fs(oldfs);
}

/* Do CP power on */
static void balong_power_on(void)
{
    if(!balong_driver_plat_data)
        return;

    accu_mdelay(1);
    gpio_set_value(balong_driver_plat_data->balong_power_on, 1);
    gpio_set_value(balong_driver_plat_data->balong_pmu_reset, 1);
    hwlog_info("balong_power_on, set gpio %d as %d\n",
                balong_driver_plat_data->balong_power_on,
                gpio_get_value(balong_driver_plat_data->balong_power_on));
    accu_udelay(40);
}

/* Do CP power off */
static void balong_power_off(void)
{
    if(!balong_driver_plat_data)
        return;

    gpio_set_value(balong_driver_plat_data->balong_power_on, 0);
    accu_mdelay(1);
}

static inline void balong_power_change_state(enum BALONG_POWER_STATE_E state)
{
    if ((state >= BALONG_POW_S_MAX) || (balong_curr_power_state >= BALONG_POW_S_MAX))
    {
        hwlog_info("change state(%d) or current state(%d) is error\n",
					state,
					balong_curr_power_state);
        return;
    }
    hwlog_info("balong_power: Power state change: %s -> %s\n",
					power_state_name[balong_curr_power_state],
					power_state_name[state]);

    balong_curr_power_state = state;
}

static void balong_power_usb_on(void)
{
    if(!balong_driver_plat_data)
        return;

    gpio_set_value(balong_driver_plat_data->balong_host_active, 1);
    enable_usb_hsic(true);
    msleep(usb_on_delay);

    /* Pull down HOST_ACTIVE to reset CP's USB */
    gpio_set_value(balong_driver_plat_data->balong_host_active, 0);
    accu_mdelay(10);
    gpio_set_value(balong_driver_plat_data->balong_host_active, 1);
}


/* Check enumeration with CP's bootrom */
static int usb_enum_check1(void)
{
    int i;
    if(!balong_driver_plat_data)
        return -1;

    hwlog_info("balong_power: Start usb enum check 1\n");

    for (i=0; i<MAX_ENUM_RETRY; i++) {
        msleep(250 + (i * 2));
        /* Check ECHI connect state to see if the enumeration will be success */
        if (readl(ioremap(REG_BASE_USB2EHCI, 0x1000) + 0x54) & 1) {
            hwlog_info("balong_power: bootrom emun seems to be done after %d retrys\n", i);
            if (is_echi_connected()) {
                hwlog_info("balong_power: bootrom emun done\n");
                return 0;
            } else {
                hwlog_info("balong_power: connect signal detected but bootrom emun failed\n");
                return -1;
            }
        } else {
            gpio_set_value(balong_driver_plat_data->balong_host_active, 0);
            hwlog_info("balong_power: bootrom emun retry %d\n", i+1);
            msleep(10);
            gpio_set_value(balong_driver_plat_data->balong_host_active, 1);
        }
    }

    return -1;
}

/* Check enumeration with CP's system-img */
static int usb_enum_check2(void)
{
    int i;
    unsigned long flags;

    if(!balong_driver_plat_data)
        return -1;

    hwlog_info("balong_power: Start usb enum check 2\n");

    for (i=0; i<MAX_ENUM_RETRY; i++) {
        msleep(250 + (i * 2));
        /* Check ECHI connect state to see if the enumeration will be success */
        if (readl(ioremap(REG_BASE_USB2EHCI, 0x1000) + 0x54) & 1) {
            hwlog_info("balong_power: system emun seems to be done after %d retrys\n", i);
            if (is_echi_connected()) {
                hwlog_info("balong_power: system emun done\n");
                spin_lock_irqsave(&balong_state_lock, flags);
                balong_power_change_state(BALONG_POW_S_USB_ENUM_DONE);
                spin_unlock_irqrestore(&balong_state_lock, flags);
                return 0;
            } else {
                hwlog_info("balong_power: connect signal detected but system emun failed\n");
                return -1;
            }
        } else {
            gpio_set_value(balong_driver_plat_data->balong_host_active, 0);
            hwlog_info("balong_power: bootrom emun retry %d\n", i+1);
            msleep(10);
            gpio_set_value(balong_driver_plat_data->balong_host_active, 1);
        }
    }

    return -1;
}


/* Check enumeration with CP's system-img */
static int usb_enum_check3(void)
{
    int i;
    unsigned long flags;

    if(!balong_driver_plat_data)
        return -1;

    hwlog_info("balong_power: Start usb enum check 3\n");

    for (i=0; i<MAX_ENUM_RESUME_RETRY; i++) {
        spin_lock_irqsave(&modem_reset_lock, flags);
        if(modem_reset_process == 1) {
            spin_unlock_irqrestore(&modem_reset_lock, flags);
            hwlog_info("balong_power: Modem Reset detected, exit usb_enum_check3\n");
            break;
        } else {
            spin_unlock_irqrestore(&modem_reset_lock, flags);
        }
        msleep(250 + (i * 2));
        /* Check ECHI connect state to see if the enumeration will be success */
        if (readl(ioremap(REG_BASE_USB2EHCI, 0x1000) + 0x54) & 1) {
            hwlog_info("balong_power: resume emun seems to be done\n");
            if (is_echi_connected()) {
                hwlog_info("balong_power: resume enum done\n");
                gpio_set_value(balong_driver_plat_data->balong_slave_wakeup, 0);
                return 0;
            }
            else {
                hwlog_info("balong_power: connect signal detected but resume emun failed\n");
                return -1;
            }
        }
        else {
            hwlog_info("balong_power: resume emun retry %d\n", i+1);
            gpio_set_value(balong_driver_plat_data->balong_host_active, 0);
            accu_mdelay(10);
            gpio_set_value(balong_driver_plat_data->balong_host_active, 1);
        }
    }

    return -1;
}

/* Resume from L2 state */
static void balong_power_l2_resume(struct work_struct *work)
{
}

/* Resume from L3 state */
static void balong_power_l3_resume(struct work_struct *work)
{
}

static ssize_t balong_power_get(struct device *dev,
                 struct device_attribute *attr,
                 char *buf)
{
    ssize_t ret = balong_curr_power_state;

    return snprintf(buf, sizeof(ssize_t), "%d\n", ret);
}

static ssize_t balong_power_set(struct device *dev,
                 struct device_attribute *attr,
                 const char *buf, size_t count)
{
    int state;
    int i;
    unsigned long flags;
    struct balong_power_plat_data *plat = balong_driver_plat_data;

    if(!balong_driver_plat_data)
        return -EINVAL;

    hwlog_info( "Power PHY set to %s\n", buf);

    if (kstrtoint(buf, 10, &state) != 0)
        return -EINVAL;

    if (state == POWER_SET_ON) {
        free_reset_isr(dev);
        spin_lock_irqsave(&balong_state_lock, flags);
        balong_power_change_state(BALONG_POW_S_INIT_ON);
        spin_unlock_irqrestore(&balong_state_lock, flags);

        /* For flashless, enable USB to enumerate with CP's bootrom */
        enable_usb_hsic(true);
        msleep(usb_on_delay);

        msleep(20);
        gpio_set_value(plat->balong_host_active, 1);

        for (i=0; i<MAX_HSIC_RESET_RETRY; i++) {
            if (usb_enum_check1() == 0)
                break;
            enable_usb_hsic(false);
            msleep(10);
            enable_usb_hsic(true);
            msleep(usb_on_delay);
        }

        if (i == MAX_HSIC_RESET_RETRY) {
            /* Fail to enumarte, make caller to know */
            hwlog_info( "boot rom enum retry failed\n");
            return -1;
        }
    }
    else if (state == POWER_SET_OFF){
        free_reset_isr(dev);
        spin_lock_irqsave(&balong_state_lock, flags);
        my_wake_unlock(&balong_wakelock);
        balong_power_change_state(BALONG_POW_S_OFF);
        spin_unlock_irqrestore(&balong_state_lock, flags);

        enable_usb_hsic(false);

        gpio_set_value(plat->balong_host_active, 0);
        gpio_set_value(plat->balong_slave_wakeup, 0);
    }
    else if (state == POWER_SET_DEBUGON){
        hwlog_info("power set to 1, begin to power on modem\n");
        free_reset_isr(dev);
        msleep(20);
        balong_power_on();
        hwlog_info("power set to 1, end to power on modem\n");
    }
    else if (state == POWER_SET_DEBUGOFF){
        free_reset_isr(dev);
        balong_power_off();
    }
    else if (state == POWER_SET_HSIC_RESET){
        unsigned long tio = msecs_to_jiffies(30000);
        spin_lock_irqsave(&balong_state_lock, flags);
        balong_power_change_state(BALONG_POW_S_WAIT_READY);
        spin_unlock_irqrestore(&balong_state_lock, flags);
        enable_usb_hsic(false);
        gpio_set_value(plat->balong_host_active, 0);
        tio = wait_event_timeout(balong_wait_q,
                                (balong_curr_power_state == BALONG_POW_S_INIT_USB_ON),
                                tio);
        if (tio == 0) {
            hwlog_err( "Wait modem system on timeout\n");
            return -1;
        }

        for (i=0; i<MAX_HSIC_RESET_RETRY; i++) {
            balong_power_usb_on();
            if (usb_enum_check2() == 0)
                break;

            enable_usb_hsic(false);
            msleep(10);
        }

        if (i == MAX_HSIC_RESET_RETRY) {
            /* Failed to enumarte, make caller to know */
            hwlog_err( "system enum retry failed\n");
            return 0;
        }

        request_reset_isr(dev);
    }
#if 1 /* Command for debug use */
    else if (state == 5){ /* L0 to L2 */
        spin_lock_irqsave(&balong_state_lock, flags);
        balong_power_change_state(BALONG_POW_S_USB_L2);
        spin_unlock_irqrestore(&balong_state_lock, flags);
        set_usb_hsic_suspend(true);
    }
    else if (state == 6){ /* L2 to L0 */
        gpio_set_value(plat->balong_slave_wakeup, 1);
    }
    else if (state == 7){ /* to L3 */
        enable_usb_hsic(false);
        gpio_set_value(plat->balong_host_active, 0);
        spin_lock_irqsave(&balong_state_lock, flags);
        balong_power_change_state(BALONG_POW_S_USB_L3);
        spin_unlock_irqrestore(&balong_state_lock, flags);
    }
    else if (state == 8){ /* L3 to L0 */
        enable_usb_hsic(true);
        msleep(10);
        gpio_set_value(plat->balong_slave_wakeup, 1);
        if (usb_enum_check3() == 0) {
            spin_lock_irqsave(&balong_state_lock, flags);
            balong_power_change_state(BALONG_POW_S_USB_L0);
            spin_unlock_irqrestore(&balong_state_lock, flags);
          }
    }
    else if (state == 9){
        hwlog_info("0\n");
        accu_mdelay(1);
        hwlog_info("1\n");
        accu_mdelay(2);
        hwlog_info("2\n");
        accu_mdelay(3);
        hwlog_info("3\n");
        accu_mdelay(4);
        hwlog_info("4\n");
        accu_mdelay(5);
        hwlog_info("5\n");
        accu_mdelay(10);
        hwlog_info("10\n");
        accu_mdelay(20);
        hwlog_info("20\n");
    }
#endif

    return count;
}

static irqreturn_t host_wakeup_isr(int irq, void *dev_id)
{
    struct device* dev = (struct device*)dev_id;
    int val;
    unsigned long flags;

    if(!balong_driver_plat_data)
        return IRQ_NONE;

    val = gpio_get_value(balong_driver_plat_data->balong_host_wakeup);
    if (val) {
        irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
        hwlog_info( "Host wakeup rising[%d].\n",balong_curr_power_state);
    } else {
        irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
        hwlog_info( "Host wakeup falling[%d].\n",balong_curr_power_state);
    }

    spin_lock_irqsave(&balong_state_lock, flags);

    switch (balong_curr_power_state) {
    case BALONG_POW_S_WAIT_READY:
        if (!val) {
            balong_power_change_state(BALONG_POW_S_INIT_USB_ON);
            wake_up(&balong_wait_q);
        }
        break;

    case BALONG_POW_S_USB_ENUM_DONE:
        if (val) {
            balong_power_change_state(BALONG_POW_S_USB_L0);
            my_wake_lock(&balong_wakelock);
        }
        break;

    case BALONG_POW_S_USB_L2:
        if (!val) {
            balong_power_change_state(BALONG_POW_S_USB_L2_TO_L0);
            my_wake_lock(&balong_wakelock);

            if (gpio_get_value(balong_driver_plat_data->balong_slave_wakeup) == 0) {
                /* CP wakeup AP, must do something to make USB host know it */
                hwlog_info("It's CP wakeup AP, so resume BUS first\n");
                queue_work(workqueue_usb, &l2_resume_work);
            }
        }
        break;

    case BALONG_POW_S_USB_L2_TO_L0:
        if (val) {
            balong_power_change_state(BALONG_POW_S_USB_L0);
            /* If AP wakeup CP, set SLAVE_WAKEUP to low to complete */
            if (gpio_get_value(balong_driver_plat_data->balong_slave_wakeup))
                gpio_set_value(balong_driver_plat_data->balong_slave_wakeup, 0);

            queue_work(workqueue_usb, &l2_resume_work);
        }
        break;

    case BALONG_POW_S_USB_L3:
        if (!val && !wake_lock_active(&balong_wakelock))
                /* Wakeup by CP, wakelock the system */
                my_wake_lock(&balong_wakelock);
        break;

    case BALONG_POW_S_USB_L0:
        if (!val) {
            dev_notice(dev, "Receive HOST_WAKEUP at L0, workaround\n");
        }
        break;
    default:
        break;
    }

    spin_unlock_irqrestore(&balong_state_lock, flags);

    return IRQ_HANDLED;
}
static DEVICE_ATTR(modem_state, S_IRUGO | S_IWUSR, balong_power_get, balong_power_set);

static ssize_t modem_state_show(struct device *dev,
                                        struct device_attribute *attr,
                                        char *buf)
{
    int mdm_state;

    if(!balong_driver_plat_data)
        return 0;

    if( GPIO_CP_PM_RDY == gpio_get_value(balong_driver_plat_data->balong_reset_ind) ) {
        if( GPIO_CP_SW_RDY == gpio_get_value(balong_driver_plat_data->balong_slave_active) )
            mdm_state = MODEM_STATE_READY;
        else
            mdm_state = MODEM_STATE_POWER;
    } else
        mdm_state = MODEM_STATE_OFF;

    return ( snprintf(buf,PAGE_SIZE,"%s\n", modem_state_str[mdm_state]) );
}
static DEVICE_ATTR(state, S_IRUGO, modem_state_show, NULL);


static struct attribute *balong_ext_modem_attributes[] = {
    &dev_attr_modem_state.attr,
    &dev_attr_state.attr,
    NULL,
};

static const struct attribute_group balong_ext_modem_node = {
    .attrs = balong_ext_modem_attributes,
};

static struct platform_device balong_ext_modem_info = {
    .name = "balong_ext_modem",
    .id = -1,
};

static int balong_power_probe(struct platform_device *pdev)
{
    int status;
    int ret;
    int gpio;
    struct balong_power_plat_data *pdev_data;
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;

    ret = platform_device_register(&balong_ext_modem_info);
    if (ret) {
        hwlog_err( "%s: platform_device_register failed, ret:%d.\n",
                __func__, ret);
        return -1;
    }

    ret = sysfs_create_group(&balong_ext_modem_info.dev.kobj, &balong_ext_modem_node);
    if (ret) {
        hwlog_err( "sensor_input_info_init sysfs_create_group error ret =%d", ret);
        platform_device_unregister(&balong_ext_modem_info);
        return -1;
    }

    pdev_data = devm_kzalloc(dev, sizeof(struct balong_power_plat_data), GFP_KERNEL);
    if ( pdev_data == NULL ) {
        hwlog_err( "failed to allocate balong_power_plat_data\n");
        status = -1;
        goto error_out;
    }

    /* to control modem power on/off */
    gpio = of_get_named_gpio(np, "balong_power_on", 0);
    hwlog_info("pdev_data->balong_power_on is %d\n", gpio);
    if(!gpio_is_valid(gpio)) {
        hwlog_err( "gpio balong_power_on is not valid\n");
        status = -1;
        goto error_out;
    }
    pdev_data->balong_power_on = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_power_on, "BALONG_POWER_ON");
    if (ret) {
        hwlog_err("gpio balong_power_on request failed\n");
        status = -1;
        goto error_out;
    }
    gpio_direction_output(pdev_data->balong_power_on, 0);

    /* to process modem reset request */
    gpio = of_get_named_gpio(np, "balong_pmu_reset", 0);
    hwlog_info("pdev_data->balong_pmu_reset is %d\n", gpio);
    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_pmu_reset is not valid\n");
        status = -1;
        goto error_free_balong_power;
    }
    pdev_data->balong_pmu_reset = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_pmu_reset, "BALONG_PMU_RESET");
    if (ret) {
        hwlog_err("gpio balong_pmu_reset request failed\n");
        status = -1;
        goto error_free_balong_power;
    }
    gpio_direction_output(pdev_data->balong_pmu_reset, 1);
    status = request_irq(gpio_to_irq(pdev_data->balong_host_wakeup),
                            host_wakeup_isr,
                            IRQF_TRIGGER_HIGH |
                            IRQF_NO_SUSPEND,
                            "HOST_WAKEUP_IRQ",
                            &pdev->dev);
    if (status) {
        hwlog_err("Request irq for host wakeup failed\n");
        goto error_free_pmu_reset;
    }

    /* hsic low power management signal, AP-->MODEM */
    gpio = of_get_named_gpio(np, "balong_host_active", 0);
    hwlog_info("pdev_data->balong_host_active is %d\n", gpio);
    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_host_active is not valid\n");
        status = -1;
        goto error_free_pmu_reset;
    }
    pdev_data->balong_host_active = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_host_active, "BALONG_HOST_ACTIVE");
    if (ret) {
        hwlog_err("gpio balong_host_active request failed\n");
        status = -1;
        goto error_free_pmu_reset;
    }
    gpio_direction_output(pdev_data->balong_host_active, 0);

    /* modem wakeup ap */
    gpio = of_get_named_gpio(np, "balong_host_wakeup", 0);
    hwlog_info("pdev_data->balong_host_wakeup is %d\n", gpio);
    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_host_wakeup is not valid\n");
        status = -1;
        goto error_free_host_active;
    }
    pdev_data->balong_host_wakeup = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_host_wakeup, "BALONG_HOST_WAKEUP");
    if (ret) {
        hwlog_err( "gpio balong_host_wakeup request failed\n");
        status = -1;
        goto error_free_host_active;
    }
    gpio_direction_input(pdev_data->balong_host_wakeup);
     status = request_irq(gpio_to_irq(pdev_data->balong_host_wakeup),
                             host_wakeup_isr,
                             IRQF_TRIGGER_HIGH |
                             IRQF_NO_SUSPEND,
                             "HOST_WAKEUP_IRQ",
                             &pdev->dev);
     if (status) {
         hwlog_err("Request irq for host wakeup failed\n");
         goto error_free_host_wakeup;
     }

    /* ap wakeup modem */
    gpio = of_get_named_gpio(np, "balong_slave_wakeup", 0);
    hwlog_info("pdev_data->balong_slave_wakeup is %d\n", gpio);
    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_slave_wakeup is not valid\n");
        status = -1;
        goto error_free_host_wakeup;
    }
    pdev_data->balong_slave_wakeup = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_slave_wakeup, "BALONG_SLAVE_WAKEUP");
    if (ret) {
        hwlog_err( "gpio balong_slave_wakeup request failed\n");
        status = -1;
        goto error_free_host_wakeup;
    }
    gpio_direction_output(pdev_data->balong_slave_wakeup, 0);

    /* modem reset detect pin */
    gpio = of_get_named_gpio(np, "balong_reset_ind", 0);
    hwlog_info("pdev_data->balong_reset_ind is %d\n", gpio);

    if(!gpio_is_valid(gpio)) {
        hwlog_info("gpio balong_reset_ind is not valid\n");
        status = -1;
        goto error_free_slave_wakeup;
    }
    pdev_data->balong_reset_ind = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_reset_ind, "BALONG_RESET_IND");
    if (ret) {
        hwlog_err( "gpio balong_reset_ind request failed\n");
        status = -1;
        goto error_free_slave_wakeup;
    }
    gpio_direction_input(pdev_data->balong_reset_ind);
    
    /* usb insert/extract signal to modem */
    gpio = of_get_named_gpio(np, "balong_usb_insert_ind", 0);
    hwlog_info("pdev_data->balong_usb_insert_ind is %d\n", gpio);

    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_usb_insert_ind is not valid\n");
        status = -1;
        goto error_free_reset_ind;
    }
    pdev_data->balong_usb_insert_ind = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_usb_insert_ind, "BALONG_USB_INSERT_IND");
    if (ret) {
        hwlog_err( "gpio balong_usb_insert_ind request failed\n");
        status = -1;
        goto error_free_reset_ind;
    }
    gpio_direction_output(pdev_data->balong_usb_insert_ind, 0);

    /* balong slave active:  for modem ready, when modem ready, AP receive edge falling trigger*/
    gpio = of_get_named_gpio(np, "balong_slave_active", 0);
    if(!gpio_is_valid(gpio)) {
        hwlog_err("gpio balong_slave_active is not valid");
        status = -1;
        goto error_free_usb_insert_ind;
    }
    pdev_data->balong_slave_active = (unsigned int)gpio;
    ret = gpio_request(pdev_data->balong_slave_active, "BALONG_SLAVE_ACTIVE");
    if (ret) {
        hwlog_err( "gpio balong_slave_active request failed\n");
        goto error_free_usb_insert_ind;
    }
    gpio_direction_input(pdev_data->balong_slave_active);
    status = request_irq(gpio_to_irq(pdev_data->balong_slave_active),
                            mdm_sw_state_isr,
                            IRQF_TRIGGER_RISING |
                            IRQF_TRIGGER_FALLING |
                            IRQF_NO_SUSPEND,
                            "mdm_sw_state_isr",
                            pdev);
    if (status) {
        hwlog_err("Request irq for slave active failed\n");
        goto error_free_slave_active;
    }

    INIT_WORK(&l2_resume_work, balong_power_l2_resume);
    INIT_WORK(&l3_resume_work, balong_power_l3_resume);
    INIT_WORK(&modem_reset_work, balong_power_reset_work);
    INIT_WORK(&modem_ready_work, balong_modem_ready_work);

    init_waitqueue_head(&balong_wait_q);

    /* Initialize works
     * when hsic power status changed, to notice modem to do some work
     */
    workqueue_usb = create_singlethread_workqueue("balong_usb_workqueue");
    if (!workqueue_usb) {
        dev_err(&pdev->dev, "Create balong usb workqueue failed\n");
        status = -1;
        goto error_free_slave_active;
    }
    /* Initialize works
     * when modem reset signal detech, notice ril to restart balong modem.
     */
    workqueue_ril = create_singlethread_workqueue("balong_ril_workqueue");
    if (!workqueue_ril) {
        dev_err(&pdev->dev, "Create balong ril workqueue failed\n");
        status = -1;
        goto error_free_slave_active;
    }

    /* This wakelock will be used to arrest system sleeping when USB is in L0 state */
    wake_lock_init(&balong_wakelock, WAKE_LOCK_SUSPEND, "balong_ext_modem");

    balong_curr_power_state = BALONG_POW_S_OFF;

    balong_driver_plat_data = pdev_data;

    return 0;

error_free_slave_active:
    if(gpio_is_valid(pdev_data->balong_slave_active))
        gpio_free(pdev_data->balong_slave_active);
error_free_usb_insert_ind:
    if(gpio_is_valid(pdev_data->balong_usb_insert_ind))
        gpio_free(pdev_data->balong_usb_insert_ind);
error_free_reset_ind:
    if(gpio_is_valid(pdev_data->balong_reset_ind))
        gpio_free(pdev_data->balong_reset_ind);
error_free_slave_wakeup:
    if(gpio_is_valid(pdev_data->balong_slave_wakeup))
        gpio_free(pdev_data->balong_slave_wakeup);
error_free_host_wakeup:
    if(gpio_is_valid(pdev_data->balong_host_wakeup))
        gpio_free(pdev_data->balong_host_wakeup);
error_free_host_active:
    if(gpio_is_valid(pdev_data->balong_host_active))
        gpio_free(pdev_data->balong_host_active);
error_free_pmu_reset:
    if(gpio_is_valid(pdev_data->balong_pmu_reset))
        gpio_free(pdev_data->balong_pmu_reset);
error_free_balong_power:
    if(gpio_is_valid(pdev_data->balong_power_on))
        gpio_free(pdev_data->balong_power_on);
error_out:
    sysfs_remove_group(&balong_ext_modem_info.dev.kobj, &balong_ext_modem_node);
    platform_device_unregister(&balong_ext_modem_info);

    return status;
}

static int balong_power_remove(struct platform_device *pdev)
{
    dev_dbg(&pdev->dev, "balong_power_remove\n");

    wake_lock_destroy(&balong_wakelock);

    destroy_workqueue(workqueue_usb);
    destroy_workqueue(workqueue_ril);

    return 0;
}

int balong_power_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

int balong_power_resume(struct platform_device *pdev)
{
    return 0;
}

static void modem_boot_shutdown(struct platform_device *pdev)
{
    dev_dbg(&pdev->dev, "modem_boot_shutdown\n");

    if( balong_power_set(&pdev->dev,NULL,"2",1)!=1 ) {    //power off modem at the init time;
        dev_err(&pdev->dev, "Failed to power off hsic\n");
    }

    if( balong_power_set(&pdev->dev,NULL,"0",1)!=1 ) {    //power off modem at the init time;
        dev_err(&pdev->dev, "Failed to power off modem\n");
    }
}


static const struct of_device_id balong_power_match_table[] = {
    {
        .compatible = BALONG_POWER_DRIVER_NAME,
        .data = NULL,
    },
    {
    },
};
MODULE_DEVICE_TABLE(of, balong_power_match_table);

static struct platform_driver balong_power_driver = {
    .probe = balong_power_probe,
    .remove = balong_power_remove,
    .shutdown = modem_boot_shutdown,
    .suspend = balong_power_suspend,
    .resume = balong_power_resume,
    .driver = {
        .name ="huawei,balong_ext_power",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(balong_power_match_table),
    },
};

static int __init balong_power_init(void)
{
    platform_driver_register(&balong_power_driver);
    spin_lock_init(&balong_state_lock);
    spin_lock_init(&modem_reset_lock);
    balong_netlink_init();
    return 0;
}

static void __exit balong_power_exit(void)
{
    platform_driver_unregister(&balong_power_driver);
    balong_netlink_deinit();
}


static void balong_usb_enable(void)
{
    if(balong_driver_plat_data)
    {
        hwlog_info("balong_usb_enable\n");
        gpio_set_value(balong_driver_plat_data->balong_usb_insert_ind, 1);
    }
}

static void balong_usb_disable(void)
{
    if(balong_driver_plat_data)
    {
        hwlog_info("balong_usb_disable\n");
        gpio_set_value(balong_driver_plat_data->balong_usb_insert_ind, 0);
    }
}

static int switch_usb_notifier_call_for_balong(struct notifier_block *this,
                    unsigned long code, void *cmd)
{
    switch (code)
    {
        case USB_TO_MODEM1:
            usb_switch_status = USB_TURNED_ON;
            break;
        case USB_TO_AP:
            usb_switch_status = USB_TURNED_OFF;
            break;
        default:
            break;
    }

    if(usb_cable_connected_status == USB_CABLE_CONNECTED)
    {
		if(usb_switch_status == USB_TURNED_ON)
        {
			balong_usb_enable();
		}
        else
        {
			balong_usb_disable();
		}
	}
	else
    {
		hwlog_info("usb switch detected, but usb is not connect,  do nothing\n");
	}
	return true;
}

static int charger_notifier_call_for_balong(struct notifier_block *this,
                    unsigned long code, void *cmd)
{
	switch (code)
    {
		case CHARGER_TYPE_SDP:
        case CHARGER_TYPE_CDP:
			usb_cable_connected_status = USB_CABLE_CONNECTED;
			break;
		case CHARGER_TYPE_NONE:
			usb_cable_connected_status = USB_CABLE_NOT_CONNECTED;
			break;
		default:
			return NOTIFY_OK;
	}
	if(usb_switch_status == USB_TURNED_ON)
    {
		if(usb_cable_connected_status == USB_CABLE_CONNECTED)
        {
			balong_usb_enable();
		}
        else
		{
			balong_usb_disable();
		}
	}
    else
    {
	    hwlog_info("usb charger detected, but usb switch is not set,  do nothing\n");
    }

	return NOTIFY_OK;
}

static int __init switch_usb_notifier_balong_init(void)
{
    int status = 0;
    int ret;
    enum hisi_charger_type plugin_stat = CHARGER_TYPE_NONE;
    struct device_node *np = NULL;

    np = of_find_compatible_node(NULL, NULL, BALONG_POWER_DRIVER_NAME);
    if(!np)
    {
        hwlog_info("%s: not balong ext modem, exit\n", __func__);
        return -1;
    }
    ret = of_device_is_available(np);
    if(ret != 1) {
        hwlog_info("%s: not balong ext modem, exit\n", __func__);
        return -1;
    }

    /*register notify block for usb event*/
    status = switch_usb_register_notifier(&switch_usb_notifier);
    if(status < 0)
    {
        hwlog_err("register balong usb switch notify failed\n");
    }else
    {
        hwlog_info("register balong usb switch notify ok\n");
    }

    status = hisi_charger_type_notifier_register(&charger_notifier);
    if(status < 0)
    {
        hwlog_err("register balong usb charger notify failed\n");
    }else
    {
        hwlog_info("register balong usb charger notify ok\n");
    }

    plugin_stat = hisi_get_charger_type();
    if ((CHARGER_TYPE_SDP == plugin_stat) ||
        (CHARGER_TYPE_CDP == plugin_stat))
    {
		usb_cable_connected_status = USB_CABLE_CONNECTED;
	} else {
		usb_cable_connected_status = USB_CABLE_NOT_CONNECTED;
	}

    return 0;
}

late_initcall(switch_usb_notifier_balong_init);

module_init(balong_power_init);
module_exit(balong_power_exit);
