/*
 * Copyright (c) Huawei Technologies Co., Ltd. 1998-2013. All rights reserved.
 *
 * File name: hw_kstate.c
 * Description: This file use to send kernel state.
 * Author: chenyouzhen@huawei.com
 * Version: 0.1
 * Date: 2013/11/24
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/wakelock.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/hw_kstate.h>

#define KSTATE_FIFO_SIZE             64        // kfifo buffer size
#define NETLINK_PACKAGE_MAX          32        // package size **must be less than 32, the value relies on user netlink buffer size**
#define KSTATE_BUFF_SIZE             256       // messges max size, **you can not change, user relies on this value**
#define KSTATE_WQDELAY_MAXUS         2000000   // max delayed time is 2s
#define KSTATE_WQDELAY_MINUS         1         // min delayed time is 1us
#define KSTATE_EDC_THRESHOLD_US      8000      // 8ms
#define KSTATE_SINGLE_MEMBER         1         // the array have only one number
#define LABEL(constant, mask, delay) { #constant, constant, mask, delay }

MODULE_LICENSE("Dual BSD/GPL");

struct label {
    const char* name; // command name
    int cmd;          // command id
    int mask;         // mask
    int delay;        // delay time
};

// various mask corresponding to the default delay time, command belong to the mask
static struct label kstate_labels[] = {
    LABEL(KSTATE_EDC_CMD,   KSTATE_FPS_MASK,     KSTATE_WQDELAY_MINUS),
    LABEL(KSTATE_ISNOT_CMD, KSTATE_LOG_MASK,     KSTATE_WQDELAY_MAXUS),
    LABEL(KSTATE_ISNOT_CMD, KSTATE_SUSPEND_MASK, KSTATE_WQDELAY_MAXUS),
    LABEL(KSTATE_ISNOT_CMD, KSTATE_FREEZER_MASK, KSTATE_WQDELAY_MINUS),
};

// send data to user
struct ksmsg {
    int mask;
    struct timeval time;
    int len;
    char buf[KSTATE_BUFF_SIZE];
};

// kstate netlink send to user package
static struct ksmsg kstate_package[NETLINK_PACKAGE_MAX];

// recv data from user
struct ksnlmsg {
    int value;
};

static struct delayed_work kstate_wk;
static struct kfifo ksfifo;
static struct sock *nlfd = NULL;
static spinlock_t kstate_kfifo_lock;
static spinlock_t kstate_mask_lock;
static int kstate_user_pid = 0;
static int kstate_delay_time = KSTATE_WQDELAY_MAXUS;
static int kstate_switch_mask = KSTATE_CLOSE_ALL_MASK;  // default all mask is disable
static int kstate_command_mask = KSTATE_CLOSE_ALL_MASK; // default all mask is disable calc
static int kstate_edc_count = 0;
static bool ktime_is_suspended = false;


// define some static function
static int kstate_suspend_notify(struct notifier_block *notify_block,
                unsigned long mode, void *unused);
static bool kstate_process_data(struct ksmsg *msg);

static struct notifier_block kstate_suspend_notifier = {
    .notifier_call = kstate_suspend_notify,
    .priority = INT_MIN,
};

/*
 * Function: kstate_suspend_notify
 * Description: suspend notify call back
**/
static int kstate_suspend_notify(struct notifier_block *notify_block,
                unsigned long mode, void *unused)
{
    switch (mode) {
    case PM_POST_SUSPEND:    // finishing suspend, resume
        ktime_is_suspended = false;
        kstate(KSTATE_SUSPEND_MASK, "PM_POST_SUSPEND");
        break;
    case PM_SUSPEND_PREPARE: // going to suspend, suspend
        kstate(KSTATE_SUSPEND_MASK, "PM_SUSPEND_PREPARE");
        ktime_is_suspended = true;
        break;
    default:
        break;
    }
    return 0;
}

/*
 * Function: kstate_get_delayed_time
 * Description: get work queue delayed time
 * return: time -- delayed time
**/
static int kstate_get_delayed_time(int mask)
{
    int i = 0;
    int len = sizeof(kstate_labels) / sizeof(kstate_labels[0]);
    int delay = KSTATE_WQDELAY_MAXUS;

    // select the smallest delayed time in kstate_labels
    for (i = 0; i < len; i++) {
        if (mask & kstate_labels[i].mask) {
            if (delay > kstate_labels[i].delay) {
                delay = kstate_labels[i].delay;
            }
        }
    }

    // select the smallest delayed time in wq
    if (delay > kstate_delay_time) {
        delay = kstate_delay_time;
    }

    kstate_delay_time = delay;
    return delay;
}

/*
 * Function: kstate_to_user
 * Description: send message to user space
 * input: msg -- kernel message
 *        num -- msg number
 * return: ret<0--failed, 0--success
**/
static int kstate_to_user(struct ksmsg *msg, int num, int hdr_type)
{
    struct sk_buff *skb = NULL;
    unsigned char *old_tail = NULL;
    struct nlmsghdr *nlh = NULL;
    struct ksmsg * scratch = NULL;
    int len;
    int ret = -1;
    int type = hdr_type;
    int index = 0;

    if (IS_ERR_OR_NULL(msg) || IS_ERR_OR_NULL(nlfd)) {
        pr_err("%s, message is null!\n", __func__);
        goto end;
    }

    if (num <= 0) {
        pr_debug("%s, message array number <= 0!\n", __func__);
        goto end;
    }

    // malloc buffer
    len = sizeof(struct ksmsg);
    len = NLMSG_SPACE(len);
    skb = alloc_skb(num * len, GFP_ATOMIC);
    if (!skb) {
        pr_err("%s, alloc skb failed!\n", __func__);
        goto end;
    }

    old_tail = skb->tail;

    // put data
    for(index = 0; index < num; index++){
        nlh = __nlmsg_put(skb, 0, 0, type, len - sizeof(*nlh), 0);
        if (!nlh) { // if put failed
            if (skb) {
                kfree_skb(skb);
            }
            goto end;
        }
        nlh->nlmsg_len = skb->tail - old_tail;
        scratch = (struct ksmsg*)NLMSG_DATA(nlh);
        memcpy(scratch, &msg[index], sizeof(msg[index]));
    }

    ret = netlink_unicast(nlfd, skb, kstate_user_pid, MSG_DONTWAIT); // free skb in netlink_unicast
    if (ret < 0) {
        pr_err("%s netlink_unicast failed!\n", __func__);
    }

end:
    return ret;
}

/*
 * Function: kstate_get_data
 * Description: get kernel state from kfifo and send to user.
 *              this func scheduled in cpu0 always, so havn't function reentrancy problems.
**/
static void kstate_get_data(void)
{
    int len = 0;

    // clear package data
    memset(kstate_package, 0, sizeof(kstate_package));

    while(!kfifo_is_empty(&ksfifo)) {
        len = 0;
        while (len < NETLINK_PACKAGE_MAX) { // get kfifo data
            if (kfifo_out(&ksfifo, &kstate_package[len], sizeof(kstate_package[len]))) {
                len++;
            } else {
                break;
            }
        }
        kstate_to_user(kstate_package, len, KSTATE_REPORT_TYPE);
    }
}

/*
 * Function: kstate_put_data
 * Description: put kernel state into kfifo
 * input: msg -- kernel message
**/
static void kstate_put_data(struct ksmsg *msg)
{
    if (IS_ERR_OR_NULL(msg)) {
        pr_err("%s : the msg point is err!\n", __func__);
        return;
    }

    spin_lock(&kstate_kfifo_lock);       // lock
    if (!kfifo_is_full(&ksfifo)) {
        kfifo_in(&ksfifo, msg, sizeof(struct ksmsg)); // put data in kfifo
        spin_unlock(&kstate_kfifo_lock); // unlock
    } else {
        spin_unlock(&kstate_kfifo_lock); // unlock
        kstate_to_user(msg, KSTATE_SINGLE_MEMBER, KSTATE_REPORT_TYPE);
        pr_info("%s : ksfifo is full.\n", __func__);
    }
}


/*
 * Function: kstate_schedule_work
 * Description: reset the workqueue
 * input: mask -- msg mask
**/
static void kstate_schedule_work(int mask)
{
    int delay = KSTATE_WQDELAY_MAXUS;

    // reset work queue delay
    cancel_delayed_work(&kstate_wk);

    delay = kstate_get_delayed_time(mask);

    // reset the workqueue delayed time is 1us.
    if (kfifo_len(&ksfifo) >= sizeof(kstate_package)) {
        pr_info("%s : ksfifo length >= %d.\n", __func__, NETLINK_PACKAGE_MAX);
        delay = KSTATE_WQDELAY_MINUS;
    }
    schedule_delayed_work_on(0, &kstate_wk, usecs_to_jiffies(delay)); // work is schedule in cpu 0
}

/*
 * Function: kstate_workqueue
 * Description: kstate work callback function
**/
void kstate_workqueue(struct work_struct *work)
{
    kstate_delay_time = KSTATE_WQDELAY_MAXUS;
    kstate_get_data();
}

/*
 * Function: kstate_check_mask
 * Description: get switch status
 * input: mask -- kstate mask
 * return: false--disable, true--enable
**/
static bool kstate_check_mask(int mask)
{
    return (mask & kstate_switch_mask);
}

/*
 * Function: kstate_set_on_off
 * Description: set this mask on or off
 * input: mask, val -- true(on),false(off)
**/
static void kstate_set_on_off(int mask, bool val)
{
    if (val) {
        kstate_switch_mask |= mask;
    } else {
        kstate_switch_mask &= (~mask);
    }
}

/*
 * Function: kstate_set_notifier
 * Description: register or unregister notifier
 * input: mask, val -- true(on),false(off)
**/
#define PRE_MASK_IS_OFF(type, mask) (type & mask & (~kstate_switch_mask)) // true, previous state is off
#define PRE_MASK_IS_ON(type, mask) (type & mask & kstate_switch_mask)     // true, previous state is on
static void kstate_set_notifier(int mask, bool val)
{
    int ret = 0;
    if (val) { // previous state is off
        if(PRE_MASK_IS_OFF(KSTATE_SUSPEND_MASK, mask)) { // type is suspend
            ret = register_pm_notifier(&kstate_suspend_notifier);
            if (ret < 0) {
                pr_err("%s : register_pm_notifier failed!\n", __func__);
            } else {
                pr_debug("%s : register_pm_notifier\n", __func__);
            }
        }
        if(PRE_MASK_IS_OFF(KSTATE_FPS_MASK, mask)) {     // type is FPS
            kstate_edc_count = 0;
        }
    } else { // previous state is on
        if(PRE_MASK_IS_ON(KSTATE_SUSPEND_MASK, mask)) {  // type is suspend
            ret = unregister_pm_notifier(&kstate_suspend_notifier);
            if (ret < 0) {
                pr_err("%s : unregister_pm_notifier failed!\n", __func__);
            } else {
                pr_debug("%s : unregister_pm_notifier\n", __func__);
            }
        }
        if(PRE_MASK_IS_ON(KSTATE_FPS_MASK, mask)) {      // type is FPS
            kstate_edc_count = 0;
        }
    }
}

/*
 * Function: kstate_set_mask
 * Description: set kstate mask
 * input: mask, val -- true(on),false(off)
**/
static void kstate_set_mask(int mask, bool val)
{
    spin_lock(&kstate_mask_lock);
    kstate_set_notifier(mask, val);
    kstate_set_on_off(mask, val);
    spin_unlock(&kstate_mask_lock);
}

/*
 * Function: vkstate
 * Description: parse kernel state
 * input: mask -- message mask
 *        fmt -- string
 * return: -1--failed, 0--success
**/
static int vkstate(int mask, const char *fmt, va_list args)
{
    int ret = 0;
    struct ksmsg msg;

    // stitching Information
    memset(&msg, 0, sizeof(msg));
    msg.mask = mask;

    if (!ktime_is_suspended) {
        do_gettimeofday(&(msg.time));
    }

    // Emit the output into the temporary buffer
    msg.len = vscnprintf(msg.buf, KSTATE_BUFF_SIZE - 1, fmt, args);

    // report the data yes or no
    if (!kstate_process_data(&msg)) {
        return ret;
    }

    // put data in kfifo
    kstate_put_data(&msg);
    kstate_schedule_work(mask);

    return ret;
}

/*
 * Function: kstate
 * Description: kernel information entry function
 * input: mask -- message mask
 *        fmt -- string
 * return: -1--failed, 0--success
**/
int kstate(int mask, const char *fmt, ...)
{
    va_list args;
    int r = -1;

    if (kstate_check_mask(mask)) {
        va_start(args, fmt);
        r = vkstate(mask, fmt, args);
        va_end(args);
    }

    return r;
}

/*
 * Function: kstate_get_command_mask
 * Description: get command mask
 * return: mask
**/
static int kstate_get_command_mask(void)
{
    const struct label *labels = kstate_labels;
    int len = sizeof(kstate_labels) / sizeof(kstate_labels[0]);
    int mask = 0;

    while(len > 0) {
        len--;
        if (labels[len].cmd >= KSTATE_BEGIN_CMD) {
            mask |= labels[len].mask;
        }
    }

    return mask;
}

/*
 * Function: kstate_get_command
 * Description: get the cmd and mask
 * input: buf, cmd, mask
 * return: -1--failed, 0--success
**/
static int kstate_get_command(const char *buf, int *cmd, int *mask)
{
    const struct label *labels = kstate_labels;
    int len = sizeof(kstate_labels) / sizeof(kstate_labels[0]);
    int ret = -1;

    if (IS_ERR_OR_NULL(buf) || IS_ERR_OR_NULL(cmd) || IS_ERR_OR_NULL(mask)) {
        goto end;
    }

    while(--len >= 0 && (strncmp(buf, labels->name, strlen(labels->name)) != 0)) {
        labels++;
    }

    if(len < 0) {
        goto end;
    }

    *cmd = labels->cmd;
    *mask = labels->mask;
    ret = 0;

end:
    return ret;
}


/*
 * Function: kstate_add_edc_count
 * Description: add edc count
 * input: void
 * return: 0--success, -1--failed
**/
static int kstate_add_edc_count(struct ksmsg *msg, int mask)
{
    static struct timeval pre_time;
    struct timeval cur_time;
    uint64_t deta_time_us;
    int ret = -1;

    if (IS_ERR_OR_NULL(msg) || (0 == (msg->mask & mask))) {
        goto end;
    }

    memcpy(&cur_time, &msg->time, sizeof(cur_time));

    deta_time_us = (cur_time.tv_sec - pre_time.tv_sec) * 1000000 // calc deta time
                 + (cur_time.tv_usec - pre_time.tv_usec);

    if (deta_time_us > KSTATE_EDC_THRESHOLD_US) { // one frame always larger than 16.6ms
        kstate_edc_count++;
        pre_time.tv_sec = cur_time.tv_sec;
        pre_time.tv_usec = cur_time.tv_usec;
        ret = 0;
    }

    if(kstate_edc_count < 0) { // exceed the threshold
        kstate_edc_count = 0;
        ret = -1;
    }

end:
    return ret;

}

/*
 * Function: kstate_process_data
 * Description: process kernel message
 * input: msg -- message
 * return: true--report, false--don't report
**/
static bool kstate_process_data(struct ksmsg *msg)
{
    bool ret = true;
    int mask = -1, cmd = -1, res = -1;
    if (IS_ERR_OR_NULL(msg) || 0 == (msg->mask & kstate_command_mask)) { // check mask
        goto end;
    }

    res = kstate_get_command(msg->buf, &cmd, &mask); // get cmd id and mask
    if(res < 0) {
        goto end;
    }

    switch (cmd) {
    case KSTATE_EDC_CMD:
        kstate_add_edc_count(msg, mask);
        ret = false;
        break;
    default:
        ret = true;
        break;
    }

end:
    return ret;
}

/*
 * Function: kstate_reply_data
 * Description: reply the data
 * input: cmd, data, type
**/
static void kstate_reply_data(int cmd, int data, int type)
{
    struct ksmsg msg;

    msg.mask = cmd;  // cmd type identify
    msg.time.tv_sec = 0;
    msg.time.tv_usec = 0;
    snprintf(msg.buf, sizeof(msg.buf), "%d", data);

    kstate_to_user(&msg, KSTATE_SINGLE_MEMBER, type);
}


/*
 * Function: kstate_cmd_sync
 * Description: sync the command data
 * input: cmd
**/
static void kstate_cmd_sync(int cmd)
{
    int result = 0;
    switch (cmd) {
    case KSTATE_EDC_CMD:
        result = kstate_edc_count;
        break;
    default:
        return;
    }

    kstate_reply_data(cmd, result, KSTATE_COMMAND_TYPE);
}

/*
 * Function: kstate_process_user_data
 * Description: process the user message
 * input: nlh -- netlink head msg
 *        msg -- netlink data
**/
static void kstate_process_user_data(struct nlmsghdr *nlh, struct ksnlmsg *msg)
{
    int value = 0;
    int type = 0;

    if (IS_ERR_OR_NULL(nlh) || IS_ERR_OR_NULL(msg)) {
        pr_err("%s : point is null!\n", __func__);
        return;
    }

    value = msg->value;
    kstate_user_pid = nlh->nlmsg_pid;
    type = nlh->nlmsg_type;

    switch (type) {
    case KSTATE_ON_TYPE:
    case KSTATE_OFF_TYPE:
        kstate_set_mask(value, (KSTATE_ON_TYPE == type));    // enable or disable monitor this(mask) type state
        kstate_reply_data(0, value, type);                   // reply netlink on_off
        break;
    case KSTATE_COMMAND_TYPE:
        kstate_cmd_sync(value); // get command data
        break;
    default:
        pr_err("%s : Error Pid is : %d!\n", __func__, kstate_user_pid);
        break;
    }
}

/*
 * Function: kstate_from_user
 * Description: receive the user message
 * input: skb -- store message
**/
static void kstate_from_user(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct ksnlmsg *msg;

    if (IS_ERR_OR_NULL(skb)) {
        pr_err("%s : skb err!\n", __func__);
        return;
    }

    if (skb->len <= NLMSG_LENGTH(sizeof(*msg))) {
        // processing netlink msg
        nlh = (struct nlmsghdr *)skb->data;
        msg = (struct ksnlmsg *)NLMSG_DATA(nlh);

        if ((nlh->nlmsg_len <= NLMSG_LENGTH(sizeof(*msg))) && (skb->len <= nlh->nlmsg_len)) {
            kstate_process_user_data(nlh, msg); // process netlink data
        }
    }
}


/*
 * Function: kstate_init
 * Description: kstate module init
 * return: -1--failed, 0--success
**/
int __init kstate_init(void)
{
    int ret = 0;

    struct netlink_kernel_cfg cfg = {
        .input = kstate_from_user,
    };

    nlfd = netlink_kernel_create(&init_net, NETLINK_HW_KSTATE, &cfg);

    if (IS_ERR_OR_NULL(nlfd)) {
        ret = PTR_ERR(nlfd);
        pr_err("%s : error create netlink!\n", __func__);
        goto init_end;
    }

    ret = kfifo_alloc(&ksfifo, KSTATE_FIFO_SIZE * sizeof(struct ksmsg), GFP_KERNEL);
    if (ret) {
        pr_err("%s : error kfifo_alloc!\n", __func__);
        goto init_err1;
    }

    spin_lock_init(&kstate_kfifo_lock);
    spin_lock_init(&kstate_mask_lock);
    INIT_DEFERRABLE_WORK(&kstate_wk, kstate_workqueue);
    kstate_command_mask = kstate_get_command_mask();
    pr_info("%s : success.\n", __func__);
    goto init_end;

init_err1:
    if (nlfd && nlfd->sk_socket) {
        sock_release(nlfd->sk_socket);
        nlfd = NULL;
    }
init_end:
    return ret;
}

/*
 * Function: kstate_exit
 * Description: kstate module exit
 * return: -1--failed, 0--success
**/
void __exit kstate_exit(void)
{
    if (nlfd && nlfd->sk_socket) {
        sock_release(nlfd->sk_socket);
        nlfd = NULL;
    }

    kfifo_free(&ksfifo);

    cancel_delayed_work_sync(&kstate_wk);
    pr_info("%s : success.\n", __func__);
}
module_init(kstate_init);
module_exit(kstate_exit);
