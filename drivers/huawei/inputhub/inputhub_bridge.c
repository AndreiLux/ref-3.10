/*
 *  drivers/misc/inputhub/inputhub_bridge.c
 *  Sensor Hub Channel Bridge
 *
 *  Copyright (C) 2013 Huawei, Inc.
 *  Author: huangjisong
 *
 */

#include "inputhub_bridge.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/huawei/mailbox.h>
#include <linux/huawei/dsm_pub.h>
#include "inputhub_route.h"

int (*api_inputhub_mcu_recv)(const char * buf,unsigned long length/*buf length*/)=0;
//called by inputhub_mcu module or test module.
extern int inputhub_route_init(void);
extern void inputhub_route_exit(void);
/*begin huangwen 20120706*/
static int isSensorMcuMode = 1; /*0:not mcu mode, Detect Sensor To AP by I2C   1:mcu mode , Detect Sensor To mcu*/

static struct hisi_mbox * mbox=NULL;
static struct notifier_block *nb = NULL;
static struct completion send_complete;
static int apr_log_count = 0;

int getSensorMcuMode( void )
{
    return isSensorMcuMode;
}
static void setSensorMcuMode(int mode)
{
    isSensorMcuMode = mode;
}

int inputhub_mcu_recv(const char *buf, unsigned long length)
{
    if (api_inputhub_mcu_recv != NULL) {
        return api_inputhub_mcu_recv(buf, length);
    } else {
        hwlog_err("---->error: api_inputhub_mcu_recv == NULL\n");
        return -1;
    }
}

//received data from mcu.
static int mbox_recv_notifier(struct notifier_block *nb, unsigned long len, void *msg)
{
/*
    int i;

    for (i = 0; i < len; ++i) {
        hwlog_info("-------->msg[%d] = %#.8x\n", i, ((int *)msg)[i]);
    }
*/
    inputhub_mcu_recv(msg, len * sizeof(int));//convert to bytes
    //send to mcu

    return 0;
}

int inputhub_mcu_connect( void )
{
    //connect to inputhub_route
    api_inputhub_mcu_recv=inputhub_route_recv_mcu_data;//should supply an interface

    //connect to mcu
    hwlog_info("----%s--->\n",__func__);
    nb = (struct notifier_block *)kmalloc(sizeof(struct notifier_block), GFP_KERNEL);
    if (!nb)
        return -ENOMEM;

    nb->next = NULL;
    nb->notifier_call = mbox_recv_notifier;
    if (!(mbox = hisi_mbox_get(HI3630_MAILBOX_RP_IOM3, nb))) {
        hwlog_err("failed to get mailbox in %s\n", __func__);
        kfree((void *)nb);
        return -ENODEV;
    }
    mbox->nb=nb;
    return 0;
}
int inputhub_mcu_disconnect( void )
{
    hisi_mbox_put(&mbox);
    return 0;
}

static void inputhub_mcu_send_complete(struct hisi_mbox_task *tx_task)
{
    if (tx_task->tx_buffer)
        kfree((void *)tx_task->tx_buffer);
    tx_task->tx_buffer = NULL;

    hisi_mbox_task_free(&tx_task);

    complete(&send_complete);
    return;
}

#define REG_BASE_ADDR       (0xFFF35000)
#define IOM3RSTSTAT_ADDR    (REG_BASE_ADDR + 0x1EC)
#define PERRSTSTAT4_ADDR    (REG_BASE_ADDR + 0x098)
#define PERSTAT5_ADDR       (REG_BASE_ADDR + 0x05C)
#define IOM3CLKEN_ADDR      (REG_BASE_ADDR + 0x1E0)
static inline unsigned int read_reg32(int phy_addr)
{
    unsigned int reg32_val = 0;
    unsigned int volatile *vir_addr = (unsigned int volatile *)ioremap(phy_addr, 4);

    if (vir_addr != NULL) {
        reg32_val = *vir_addr;
        iounmap(vir_addr);
    } else {
        hwlog_err("ioremap(%#.8x) failed in %s!\n", phy_addr, __func__);
    }

    return reg32_val;
}

extern void hisi_rdr_nmi_notify_iom3(void);
int sensorhub_img_dump(int type, void *buff, int size)
{
	hisi_rdr_nmi_notify_iom3();
	return 0;
}

struct dsm_client_ops sensorhub_ops = {
	.poll_state = NULL,
	.dump_func = sensorhub_img_dump,
};

static struct dsm_dev dsm_sensorhub = {
	.name = "sensorhub",
	.fops = &sensorhub_ops,
	.buff_size = 512,
};
struct dsm_client *shb_dclient = NULL;

int inputhub_mcu_send(const char *buf, unsigned long length)
{
    struct hisi_mbox_task *tx_task = NULL;
    mbox_msg_t *tx_buffer = NULL;
    mbox_msg_len_t len=0;
    int timeout = 0;
    int i, ret=0;
    if(NULL==mbox)
        inputhub_mcu_connect();
    if(NULL==mbox)
        return -1;
    len = (length + sizeof(mbox_msg_t) - 1) / (sizeof(mbox_msg_t));
    //todo:send 256 bytes max one time, and repeat until buf was sent.

    tx_buffer = (mbox_msg_t *)kmalloc(sizeof(mbox_msg_t)*len, GFP_KERNEL);
    if (!tx_buffer)
        return -1;
    memcpy(tx_buffer, buf, length);
    pr_debug("inputhub_mcu_send------------->length = %d, len = %d\n", (int)length, (int)len);
    for (i = 0; i < len; i++)
        pr_debug("tx_buffer[%d] = 0x%x\n", i, tx_buffer[i]);

    tx_task = hisi_mbox_task_alloc(mbox,
                HI3630_MAILBOX_RP_IOM3,
                tx_buffer,
                len,
                0,
                inputhub_mcu_send_complete,
                NULL);

    if (!tx_task) {
        kfree((void *)tx_buffer);
        return -1;
    }

    INIT_COMPLETION(send_complete);
    ret=hisi_mbox_msg_send_async(mbox,tx_task);
    if(ret)
    {
        hwlog_err("hisi_mbox_msg_send_async return %d.\n", ret);
        if (tx_task->tx_buffer)
            kfree((void *)tx_task->tx_buffer);
        tx_task->tx_buffer = NULL;
        hisi_mbox_task_free(&tx_task);
    }
    else
    {
        timeout = wait_for_completion_timeout(&send_complete, msecs_to_jiffies(1000));
        if (0 == timeout) {
            hwlog_err("send failed timeout, count %d\n", apr_log_count);
            if(!apr_log_count){
                if(!dsm_client_ocuppy(shb_dclient)){
                    dsm_client_record(shb_dclient, "IOM3RSTSTAT: 0x%x\n", read_reg32(IOM3RSTSTAT_ADDR));
                    dsm_client_record(shb_dclient, "PERRSTSTAT4: 0x%x\n", read_reg32(PERRSTSTAT4_ADDR));
                    dsm_client_record(shb_dclient, "PERSTAT5: 0x%x\n", read_reg32(PERSTAT5_ADDR));
                    dsm_client_record(shb_dclient, "IOM3CLKEN: 0x%x\n", read_reg32(IOM3CLKEN_ADDR));
                    dsm_client_notify(shb_dclient, DSM_ERR_SENSORHUB_IPC_TIMEOUT);
    	         }
            }
            apr_log_count++;
            return -1;
         }
    }
    hwlog_debug("send success\n");
    return 0;
}

static int g_boot_iom3 = 0x00070001;
static void inputhub_power_on_complete(struct hisi_mbox_task *tx_task)
{
    hisi_mbox_task_free(&tx_task);
    return;
}
static struct hisi_mbox *lpm3_mbox=NULL;
void boot_iom3(void)
{
    struct hisi_mbox_task *tx_task = NULL;
    if (!(lpm3_mbox = hisi_mbox_get(HI3630_MAILBOX_RP_LPM3, NULL))) {
        hwlog_err("failed to get mailbox in %s\n", __func__);
        return;
    }
    tx_task = hisi_mbox_task_alloc(lpm3_mbox,
                HI3630_MAILBOX_RP_LPM3,
                &g_boot_iom3,
                1,
                1,
                inputhub_power_on_complete,
                NULL);
    if(!tx_task) {
        hwlog_err("failed to alloc task %s\n", __func__);
        return;
    }
    if (hisi_mbox_msg_send_async(lpm3_mbox,tx_task) != 0) {
        hwlog_err("hisi_mbox_msg_send_async error in %s\n", __func__);
    }
}

static int  inputhub_mcu_init(void)
{
    int ret;
    init_completion(&send_complete);
    ret = inputhub_route_init();
    inputhub_mcu_connect();
    if (NULL == mbox) {
        hwlog_err("--------------------> faild to get mailbox in %s!\n", __func__);
        return -1;
    } else {
        hwlog_info("--------------------> succeed to get mailbox in %s!\n", __func__);
    }
    boot_iom3();
    setSensorMcuMode(1);
    shb_dclient = dsm_register_client(&dsm_sensorhub);
    hwlog_info("----%s--->\n",__func__);
    return ret;
}

static void __exit inputhub_mcu_exit(void)
{
    inputhub_route_exit();
    if(lpm3_mbox)
        hisi_mbox_put(&lpm3_mbox);
}
/*begin huangwen 20120706*/
fs_initcall_sync(inputhub_mcu_init);
/*end huangwen 20120706*/

module_exit(inputhub_mcu_exit);

MODULE_AUTHOR("Input Hub <smartphone@huawei.com>");
MODULE_DESCRIPTION("input hub bridge");
MODULE_LICENSE("GPL");
