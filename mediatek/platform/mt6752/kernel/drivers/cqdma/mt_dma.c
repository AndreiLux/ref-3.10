#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>

//#include <mach/mt_reg_base.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <mach/sync_write.h>
#include <mach/mt_clkmgr.h>
#include <mach/emi_mpu.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#define LDVT

static struct mtk_irq_mask mask;
extern void mt_irq_unmask(struct irq_data *data);
extern void mt_irq_mask(struct irq_data *data);
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
extern void mt_irq_dump_status(int irq);
#ifndef mt_reg_sync_writel(v, a)
  
#define mt_reg_sync_writel(v, a) \
        do {    \
            __raw_writel((v), IOMEM((a)));   \
            dsb();  \
        } while (0)


#endif

#define DMA_DEBUG   1
#if(DMA_DEBUG == 1)
#define dbgmsg printk
#else
#define dbgmsg(...)
#endif

/* 
 * DMA information
 */

#define NR_GDMA_CHANNEL     (1)
#define GDMA_START          (0)
#define NR_DMA              (NR_GDMA_CHANNEL)

/*
 * Register Definition
 */
#ifdef CONFIG_OF
void __iomem *CQ_DMA_BASE;
static unsigned int cqdma_irq_num = 0;
#else
#define CQ_DMA_BASE (0xF0212c00)
#endif
//#define DMA_GLOBAL_RUNNING_STATUS IOMEM((CQ_DMA_BASE + 0x0008)) 
//#define DMA_GLOBAL_GSEC_EN IOMEM((CQ_DMA_BASE + 0x0014))
//#define DMA_GDMA_SEC_EN(n) IOMEM((CQ_DMA_BASE + 0x0020 + 4 * (n)))

/*
 * General DMA channel register mapping
 */
#define DMA_INT_FLAG           IOMEM((CQ_DMA_BASE + 0x0000))
#define DMA_INT_EN             IOMEM((CQ_DMA_BASE + 0x0004))
#define DMA_START              IOMEM((CQ_DMA_BASE + 0x0008))
#define DMA_RESET              IOMEM((CQ_DMA_BASE + 0x000C))
#define DMA_STOP               IOMEM((CQ_DMA_BASE + 0x0010))
#define DMA_FLUSH              IOMEM((CQ_DMA_BASE + 0x0014))
#define DMA_CON                IOMEM((CQ_DMA_BASE + 0x0018))
#define DMA_SRC                IOMEM((CQ_DMA_BASE + 0x001C))
#define DMA_DST                IOMEM((CQ_DMA_BASE + 0x0020))
#define DMA_LEN1               IOMEM((CQ_DMA_BASE + 0x0024))
#define DMA_LEN2               IOMEM((CQ_DMA_BASE + 0x0028))
#define DMA_JUMP_ADDR          IOMEM((CQ_DMA_BASE + 0x002C))
#define DMA_IBUFF_SIZE         IOMEM((CQ_DMA_BASE + 0x0030))
#define DMA_CONNECT            IOMEM((CQ_DMA_BASE + 0x0034))
#define DMA_AXIATTR            IOMEM((CQ_DMA_BASE + 0x0038))
#define DMA_DBG_STAT           IOMEM((CQ_DMA_BASE + 0x0050))

#define DMA_SRC_4G_SUPPORT           (CQ_DMA_BASE + 0x0040)
#define DMA_DST_4G_SUPPORT       (CQ_DMA_BASE + 0x0044)
#define DMA_JUMP_4G_SUPPORT       (CQ_DMA_BASE + 0x0048)
#define DMA_VIO_DBG1           (CQ_DMA_BASE + 0x003c)
#define DMA_VIO_DBG           (CQ_DMA_BASE + 0x0060)
#define DMA_GDMA_SEC_EN           (CQ_DMA_BASE + 0x0058)

/*
 * Register Setting
 */

#define DMA_GDMA_LEN_MAX_MASK   (0x000FFFFF)

#define DMA_CON_DIR             (0x00000001)
#define DMA_CON_FPEN            (0x00000002)    /* Use fix pattern. */
#define DMA_CON_SLOW_EN         (0x00000004)
#define DMA_CON_DFIX            (0x00000008)
#define DMA_CON_SFIX            (0x00000010)
#define DMA_CON_WPEN            (0x00008000)
#define DMA_CON_WPSD            (0x00100000)
#define DMA_CON_WSIZE_1BYTE     (0x00000000)
#define DMA_CON_WSIZE_2BYTE     (0x01000000)
#define DMA_CON_WSIZE_4BYTE     (0x02000000)
#define DMA_CON_RSIZE_1BYTE     (0x00000000)
#define DMA_CON_RSIZE_2BYTE     (0x10000000)
#define DMA_CON_RSIZE_4BYTE     (0x20000000)
#define DMA_CON_BURST_MASK      (0x00070000)
#define DMA_CON_SLOW_OFFSET     (5)
#define DMA_CON_SLOW_MAX_MASK   (0x000003FF)

#define DMA_START_BIT           (0x00000001)
#define DMA_STOP_BIT            (0x00000000)
#define DMA_INT_FLAG_BIT        (0x00000001)
#define DMA_INT_FLAG_CLR_BIT    (0x00000000)
#define DMA_INT_EN_BIT          (0x00000001)
#define DMA_FLUSH_BIT           (0x00000001)
#define DMA_FLUSH_CLR_BIT       (0x00000000)
#define DMA_UART_RX_INT_EN_BIT  (0x00000003)
#define DMA_INT_EN_CLR_BIT      (0x00000000)
#define DMA_WARM_RST_BIT        (0x00000001)
#define DMA_HARD_RST_BIT        (0x00000002)
#define DMA_HARD_RST_CLR_BIT    (0x00000000)
#define DMA_READ_COHER_BIT      (0x00000010)
#define DMA_WRITE_COHER_BIT     (0x00100000)
#define DMA_GSEC_EN_BIT         (0x00000001)
#define DMA_SEC_EN_BIT          (0x00000001)



/*
 * Register Limitation
 */

#define MAX_TRANSFER_LEN1   (0xFFFFF)
#define MAX_TRANSFER_LEN2   (0xFFFFF)
#define MAX_SLOW_DOWN_CNTER (0x3FF)

/*
 * channel information structures
 */

struct dma_ctrl
{
    int in_use;
    void (*isr_cb)(void *);
    void *data;
};

/*
 * global variables
 */

static struct dma_ctrl dma_ctrl[NR_GDMA_CHANNEL];
static DEFINE_SPINLOCK(dma_drv_lock);

#define PDN_APDMA_MODULE_NAME ("CQDMA")
#define GDMA_WARM_RST_TIMEOUT   (100) // ms
volatile unsigned int DMA_INT_DONE;
/*
 * mt_req_gdma: request a general DMA.
 * @chan: specify a channel or not
 * Return channel number for success; return negative errot code for failure.
 */
int mt_req_gdma(DMA_CHAN chan)
{
    unsigned long flags;
    int i;

    spin_lock_irqsave(&dma_drv_lock, flags);

    if (chan == GDMA_ANY) {
        for (i = GDMA_START; i < NR_GDMA_CHANNEL; i++) {
            if (dma_ctrl[i].in_use) {
                continue;
            } else {
                dma_ctrl[i].in_use = 1;
                break;
            }
        }
    } else {
        if (dma_ctrl[chan].in_use) {
            i = NR_GDMA_CHANNEL;
        }
        else {
            i = chan;
            dma_ctrl[chan].in_use = 1;
        }
    }

    spin_unlock_irqrestore(&dma_drv_lock, flags);

    if (i < NR_GDMA_CHANNEL) {

        mt_reset_gdma_conf(i);

        return i;
    } else {
        return -DMA_ERR_NO_FREE_CH;
    }
}

EXPORT_SYMBOL(mt_req_gdma);

/*
 * mt_start_gdma: start the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to start
 * Return 0 for success; return negative errot code for failure.
 */
int mt_start_gdma(int channel)
{
    if ((channel < GDMA_START) || (channel >= (GDMA_START + NR_GDMA_CHANNEL))) {
        return -DMA_ERR_INVALID_CH;
    }else if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);
    mt_reg_sync_writel(DMA_START_BIT, DMA_START);

    return 0;
}

EXPORT_SYMBOL(mt_start_gdma);

/*
 * mt_polling_gdma: wait the DMA to finish for the specified GDMA channel
 * @channel: GDMA channel to polling
 * @timeout: polling timeout in ms
 * Return 0 for success; 
 * Return 1 for timeout
 * return negative errot code for failure.
 */
int mt_polling_gdma(int channel, unsigned long timeout)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    timeout = jiffies + ((HZ * timeout) / 1000); 

    do {
        if (time_after(jiffies, timeout)) {
            printk(KERN_ERR "GDMA_%d polling timeout !!\n", channel);
            mt_dump_gdma(channel);
            return 1;
        }
    } while (readl(DMA_START));

    return 0;
}

EXPORT_SYMBOL(mt_polling_gdma);

/*
 * mt_stop_gdma: stop the DMA stransfer for the specified GDMA channel
 * @channel: GDMA channel to stop
 * Return 0 for success; return negative errot code for failure.
 */
int mt_stop_gdma(int channel)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    mt_reg_sync_writel(DMA_FLUSH_BIT, DMA_FLUSH);
    while (readl(DMA_START));
    mt_reg_sync_writel(DMA_FLUSH_CLR_BIT, DMA_FLUSH);
    mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);

    return 0;
}

EXPORT_SYMBOL(mt_stop_gdma);

/*
 * mt_config_gdma: configure the given GDMA channel.
 * @channel: GDMA channel to configure
 * @config: pointer to the mt_gdma_conf structure in which the GDMA configurations store
 * @flag: ALL, SRC, DST, or SRC_AND_DST.
 * Return 0 for success; return negative errot code for failure.
 */
int mt_config_gdma(int channel, struct mt_gdma_conf *config, DMA_CONF_FLAG flag)
{
    unsigned int dma_con = 0x0, limiter = 0;

    if ((channel < GDMA_START) || (channel >= (GDMA_START + NR_GDMA_CHANNEL))) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    if (!config) {
        return -DMA_ERR_INV_CONFIG;
    }

//    if (!(config->sinc) && ((config->src) % 8)) {
//        printk("GDMA fixed address mode requires 8-bytes aligned address\n");
    if (config->sfix)
    {
        printk("GMDA fixed address mode doesn't support\n");
        return -DMA_ERR_INV_CONFIG;
    }

//    if (!(config->dinc) && ((config->dst) % 8)) {
//        printk("GDMA fixed address mode requires 8-bytes aligned address\n");
    if (config->dfix)
    {
        printk("GMDA fixed address mode doesn't support\n");
        return -DMA_ERR_INV_CONFIG;
    }

    if (config->count > MAX_TRANSFER_LEN1)
    {
        printk("GDMA transfer length cannot exceeed 0x%x.\n", MAX_TRANSFER_LEN1);
        return -DMA_ERR_INV_CONFIG;
    }

    if (config->limiter > MAX_SLOW_DOWN_CNTER)
    {
        printk("GDMA slow down counter cannot exceeed 0x%x.\n", MAX_SLOW_DOWN_CNTER);
        return -DMA_ERR_INV_CONFIG;
    }

    switch (flag) {
    case ALL:
        /* Control Register */
        mt_reg_sync_writel((u32)config->src, DMA_SRC);
        mt_reg_sync_writel((u32)config->dst, DMA_DST);
        mt_reg_sync_writel((config->wplen) & DMA_GDMA_LEN_MAX_MASK, DMA_LEN2);
        mt_reg_sync_writel(config->wpto, DMA_JUMP_ADDR);
        mt_reg_sync_writel((config->count) & DMA_GDMA_LEN_MAX_MASK, DMA_LEN1);

        /*setup coherence bus*/
        /*
        if (config->cohen){
            writel((DMA_READ_COHER_BIT|readl(DMA_AXIATTR)), DMA_AXIATTR);
            writel((DMA_WRITE_COHER_BIT|readl(DMA_AXIATTR)), DMA_AXIATTR);
        }
        */

        /*setup security channel */
        if (config->sec){
            printk("1:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
            mt_reg_sync_writel((DMA_SEC_EN_BIT|readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
            printk("2:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
        }
        else
        {
            printk("1:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
            mt_reg_sync_writel(((~DMA_SEC_EN_BIT)&readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
            printk("2:ChSEC:%x\n",readl(DMA_GDMA_SEC_EN));
        }

        /*setup domain_cfg */
        if (config->domain){
            printk("1:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
            mt_reg_sync_writel(((config->domain << 1) | readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
            printk("2:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
        }
        else
        {
            printk("1:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
            mt_reg_sync_writel((0x1 & readl(DMA_GDMA_SEC_EN)), DMA_GDMA_SEC_EN);
            printk("2:Domain_cfg:%x\n",readl(DMA_GDMA_SEC_EN));
        }


        if (config->wpen) {
            dma_con |= DMA_CON_WPEN;
        }

        if (config->wpsd) {
            dma_con |= DMA_CON_WPSD;
        }

        if (config->iten) {
            dma_ctrl[channel].isr_cb = config->isr_cb;
            dma_ctrl[channel].data = config->data;
            mt_reg_sync_writel(DMA_INT_EN_BIT, DMA_INT_EN);
        }else {
            dma_ctrl[channel].isr_cb = NULL;
            dma_ctrl[channel].data = NULL;
            mt_reg_sync_writel(DMA_INT_EN_CLR_BIT, DMA_INT_EN);
        }

        if (!(config->dfix) && !(config->sfix)) {
            dma_con |= (config->burst & DMA_CON_BURST_MASK);
        }else {
            if (config->dfix) {
                dma_con |= DMA_CON_DFIX;
                dma_con |= DMA_CON_WSIZE_1BYTE;
            }

            if (config->sfix) {
                dma_con |= DMA_CON_SFIX;
                dma_con |= DMA_CON_RSIZE_1BYTE;
            }

            // fixed src/dst mode only supports burst type SINGLE
            dma_con |= DMA_CON_BURST_SINGLE;
        }

        if (config->limiter) {
            limiter = (config->limiter) & DMA_CON_SLOW_MAX_MASK;
            dma_con |= limiter << DMA_CON_SLOW_OFFSET;
            dma_con |= DMA_CON_SLOW_EN;
        }

        mt_reg_sync_writel(dma_con, DMA_CON);
        break;

    case SRC:
        mt_reg_sync_writel((u32)config->src, DMA_SRC);

        break;
        
    case DST:
        mt_reg_sync_writel((u32)config->dst, DMA_DST);
        break;

    case SRC_AND_DST:
        mt_reg_sync_writel((u32)config->src, DMA_SRC);
        mt_reg_sync_writel((u32)config->dst, DMA_DST);
        break;

    default:
        break;
    }

    /* use the data synchronization barrier to ensure that all writes are completed */
    dsb();

    return 0;
}

EXPORT_SYMBOL(mt_config_gdma);

/*
 * mt_free_gdma: free a general DMA.
 * @channel: channel to free
 * Return 0 for success; return negative errot code for failure.
 */
int mt_free_gdma(int channel)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    mt_stop_gdma(channel);

    dma_ctrl[channel].isr_cb = NULL;
    dma_ctrl[channel].data = NULL;
    dma_ctrl[channel].in_use = 0;

    //disable_clock(MT_CG_PERI_AP_DMA, PDN_APDMA_MODULE_NAME);
    
    return 0;
}

EXPORT_SYMBOL(mt_free_gdma);

/*
 * mt_dump_gdma: dump registers for the specified GDMA channel
 * @channel: GDMA channel to dump registers
 * Return 0 for success; return negative errot code for failure.
 */
int mt_dump_gdma(int channel)
{
    unsigned int i;
    printk("Channel 0x%x\n",channel);
    for (i = 0; i < 96; i++)
    {
        printk("addr:0x%x, value:%x\n", CQ_DMA_BASE + i * 4, readl(CQ_DMA_BASE + i * 4));
    }

    return 0;
}

EXPORT_SYMBOL(mt_dump_gdma);

/*
 * mt_warm_reset_gdma: warm reset the specified GDMA channel
 * @channel: GDMA channel to warm reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_warm_reset_gdma(int channel)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    dbgmsg("GDMA_%d Warm Reset !!\n", channel);

    mt_reg_sync_writel(DMA_WARM_RST_BIT, DMA_RESET);
    
    if (mt_polling_gdma(channel, GDMA_WARM_RST_TIMEOUT) != 0)
        return 1;
    else
        return 0;
}

EXPORT_SYMBOL(mt_warm_reset_gdma);

/*
 * mt_hard_reset_gdma: hard reset the specified GDMA channel
 * @channel: GDMA channel to hard reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_hard_reset_gdma(int channel)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    printk(KERN_ERR "GDMA_%d Hard Reset !!\n", channel);

    mt_reg_sync_writel(DMA_HARD_RST_BIT, DMA_RESET);
    mt_reg_sync_writel(DMA_HARD_RST_CLR_BIT, DMA_RESET);
    
    return 0;
}

EXPORT_SYMBOL(mt_hard_reset_gdma);

/*
 * mt_reset_gdma: reset the specified GDMA channel
 * @channel: GDMA channel to reset
 * Return 0 for success; return negative errot code for failure.
 */
int mt_reset_gdma(int channel)
{
    if (channel < GDMA_START) {
        return -DMA_ERR_INVALID_CH;
    }

    if (channel >= (GDMA_START + NR_GDMA_CHANNEL)) {
        return -DMA_ERR_INVALID_CH;
    }

    if (dma_ctrl[channel].in_use == 0) {
        return -DMA_ERR_CH_FREE;
    }

    dbgmsg("GDMA_%d Reset !!\n", channel);

    if (mt_warm_reset_gdma(channel) != 0)
        mt_hard_reset_gdma(channel);
        
    return 0;
}

EXPORT_SYMBOL(mt_reset_gdma);

/*
 * gdma1_irq_handler: general DMA channel 1 interrupt service routine.
 * @irq: DMA IRQ number
 * @dev_id:
 * Return IRQ returned code.
 */
static irqreturn_t gdma1_irq_handler(int irq, void *dev_id)
{
    volatile unsigned glbsta = readl(DMA_INT_FLAG);

    dbgmsg(KERN_DEBUG"DMA Module - %s ISR Start\n", __func__);
    dbgmsg(KERN_DEBUG"DMA Module - GLBSTA = 0x%x\n", glbsta);
 
    if (glbsta & 0x1){
        if (dma_ctrl[G_DMA_1].isr_cb) {
            dma_ctrl[G_DMA_1].isr_cb(dma_ctrl[G_DMA_1].data);
        }

        mt_reg_sync_writel(DMA_INT_FLAG_CLR_BIT, DMA_INT_FLAG);
#if(DMA_DEBUG == 1)
        glbsta = readl(DMA_INT_FLAG);
        printk(KERN_DEBUG"DMA Module - GLBSTA after ack = 0x%x\n", glbsta);
#endif
    }
    else
    {
       printk("[CQDMA] discard interrupt\n");
       return IRQ_NONE;
    }

    dbgmsg(KERN_DEBUG"DMA Module - %s ISR END\n", __func__);
    
    return IRQ_HANDLED;
}

/*
 * mt_reset_gdma_conf: reset the config of the specified DMA channel
 * @iChannel: channel number of the DMA channel to reset
 */
void mt_reset_gdma_conf(const unsigned int iChannel)
{
    struct mt_gdma_conf conf;

    memset(&conf, 0, sizeof(struct mt_gdma_conf));

    if (mt_config_gdma(iChannel, &conf, ALL) != 0){
        return;
    }

    return;
}

#if defined(LDVT)

unsigned int *dma_dst_array_v;
unsigned int *dma_src_array_v;
dma_addr_t dma_dst_array_p;
dma_addr_t dma_src_array_p;

#define TEST_LEN 4000
#define LEN (TEST_LEN / sizeof(int))

void irq_dma_handler(void * data)
{
    int channel = (int)data;
    printk("irq_dma_handler called\n");
    int i = 0;
    for(i = 0; i < LEN; i++) {
        if(dma_dst_array_v[i] != dma_src_array_v[i]) {
            printk("DMA failed, src = %d, dst = %d, i = %d\n", dma_src_array_v[i], dma_dst_array_v[i], i);
            break;
        }
    }
    DMA_INT_DONE=1;
    if(i == LEN)
        printk("DMA verified ok\n");

    mt_free_gdma(channel);
}
void APDMA_test_transfer(int testcase)
{
    int i, channel;
    int start_dma;
    unsigned int cnt = 20;
    channel = mt_req_gdma(GDMA_ANY);

    printk("GDMA channel:%d\n",channel);
    if(channel < 0 ){
        printk("ERROR Register DMA\n");
        return;
    }

    mt_reset_gdma_conf(channel);
    
    dma_dst_array_v = dma_alloc_coherent(NULL, TEST_LEN, &dma_dst_array_p, GFP_KERNEL ); // 25 unsinged int
    dma_src_array_v = dma_alloc_coherent(NULL, TEST_LEN, &dma_src_array_p, GFP_KERNEL );
    struct mt_gdma_conf dma_conf = {
        .count = TEST_LEN,
        .src = dma_src_array_p,
        .dst = dma_dst_array_p,
        .iten = (testcase == 2) ? DMA_FALSE : DMA_TRUE,
        .isr_cb = (testcase == 2) ? NULL : irq_dma_handler,
        .data = channel,
        .burst = DMA_CON_BURST_SINGLE,
        .dfix = DMA_FALSE,
        .sfix = DMA_FALSE,
        //.cohen = DMA_TRUE, //enable coherence bus
        .sec = DMA_FALSE, // non-security channel
        .domain = 0, 
        .limiter = (testcase == 3 || testcase == 4) ? 0x3FF : 0,
    };

    for(i = 0; i < LEN; i++) {
        dma_dst_array_v[i] = 0;
        dma_src_array_v[i] = i;
    }
    
    if ( mt_config_gdma(channel, &dma_conf, ALL) != 0) {
        printk("ERROR set DMA\n");
        goto _exit;
        return;
    }
    
    /*
    unsigned int dma_src = readl(DMA_SRC(DMA_BASE_CH(channel)));
    unsigned int dma_dst = readl(DMA_DST);
    unsigned int len = readl(DMA_LEN1);
    printk("start dma channel %d src = 0x%x, dst = 0x%x, len = %d bytes\n", channel, dma_src, dma_dst, len);
    */
    start_dma=mt_start_gdma(channel);
#ifdef CONFIG_OF
    mt_irq_dump_status(cqdma_irq_num);
#else
    mt_irq_dump_status(CQ_DMA_IRQ_BIT_ID);
#endif
    printk("Start %d\n",start_dma);
    switch(testcase)
    {
        case 1:
               while(!DMA_INT_DONE);
#ifdef CONFIG_OF
               mt_irq_dump_status(cqdma_irq_num);
#else
               mt_irq_dump_status(CQ_DMA_IRQ_BIT_ID);
#endif
               DMA_INT_DONE=0;
               break;
        case 2:
            if (mt_polling_gdma(channel, GDMA_WARM_RST_TIMEOUT) != 0)
                printk("Polling transfer failed\n");    
            else
                printk("Polling succeeded\n");
            mt_free_gdma(channel);
            break;
        case 3:
            mt_warm_reset_gdma(channel);
            
            for(i = 0; i < LEN; i++) {
                if(dma_dst_array_v[i] != dma_src_array_v[i]) {
                    printk("Warm reset succeeded\n");
                    break;
                }
                mt_free_gdma(channel);
            }

            if(i == LEN)
                printk("Warm reset failed\n");
            break;
            
        case 4:
            mt_hard_reset_gdma(channel);
            
            for(i = 0; i < LEN; i++) {
                if(dma_dst_array_v[i] != dma_src_array_v[i]) {
                    printk("Hard reset succeeded\n");
                    break;
                }
                mt_free_gdma(channel);
            }
            if(i == LEN)
                printk("Hard reset failed\n");
            break;

        case 5:
               do {
	       	    mdelay(50);
               	    printk("GIC mask test cnt = %d\n", cnt);
		    if (cnt-- == 0) {
			mt_irq_unmask(irq_get_irq_data(cqdma_irq_num));
               		printk("GIC Mask PASS !!!\n");
			break;
		    }
	       }while(!DMA_INT_DONE);
#ifdef CONFIG_OF
               mt_irq_dump_status(cqdma_irq_num);
#else
               mt_irq_dump_status(CQ_DMA_IRQ_BIT_ID);
#endif
               DMA_INT_DONE=0;
               break;

        case 6:
               do {
	       	    mdelay(50);
               	    printk("GIC ack test cnt = %d\n", cnt);
		    if (cnt-- == 0) {
               		mt_irq_mask_restore(&mask);
               		printk("GIC Mask All PASS !!!\n");
			break;
		    }
	       }while(!DMA_INT_DONE);
#ifdef CONFIG_OF
               mt_irq_dump_status(cqdma_irq_num);
#else
               mt_irq_dump_status(CQ_DMA_IRQ_BIT_ID);
#endif
               DMA_INT_DONE=0;
               break;
        
        default:
            break;

    }

_exit:
    if(dma_dst_array_v){
        dma_free_coherent(NULL, TEST_LEN, dma_dst_array_v, dma_dst_array_p);
        dma_dst_array_v = dma_dst_array_p = NULL;
    }

    if(dma_src_array_v){
        dma_free_coherent(NULL, TEST_LEN, dma_src_array_v, dma_src_array_p);
        dma_src_array_v = dma_src_array_p = NULL;
    }

    return;
}

static ssize_t cqdma_dvt_show(struct device_driver *driver, char *buf)
{
   return snprintf(buf, PAGE_SIZE, "==CQDMA test==\n"
                                   "1.CQDMA transfer (interrupt mode)\n"
                                   "2.CQDMA transfer (polling mode)\n"
                                   "3.CQDMA warm reset\n"
                                   "4.CQDMA hard reset\n"
                                   "5.Mask CQDMA interrupt test\n"
                                   "6.Mask all CQDMA interrupt test\n"
   ); 
}

static ssize_t cqdma_dvt_store(struct device_driver *driver, const char *buf, size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);
        switch(num){
            /* Test APDMA Normal Function */
            case 1:
                APDMA_test_transfer(1);
                break;
            case 2:
                APDMA_test_transfer(2);
                break;
            case 3:
                APDMA_test_transfer(3);
                break;
            case 4:
                APDMA_test_transfer(4);
                break;
            case 5:
                mt_irq_mask(irq_get_irq_data(cqdma_irq_num));
                APDMA_test_transfer(5);
                break;
            case 6:
                mt_irq_mask_all(&mask);
                APDMA_test_transfer(6);
                break;
            default:
                break;
        }

	return count;
}

DRIVER_ATTR(cqdma_dvt, 0666, cqdma_dvt_show, cqdma_dvt_store);

#endif	//!LDVT

/*
 * Define Data Structure
 */
struct mt_cqdma_driver{
    struct device_driver driver;
    const struct platform_device_id *id_table;
};

/*
 * Define Global Variable
 */
static struct mt_cqdma_driver mt_cqdma_drv = {
    .driver = {
        .name = "cqdma",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
    .id_table= NULL,
};

/*
 * mt_init_dma: initialize DMA.
 * Always return 0.
 */
static int __init mt_init_dma(void)
{
    int i, ret;
#ifdef CONFIG_OF
    unsigned int dma_info[3] = {0, 0, 0};
    struct device_node *node;
#endif

    for (i = 0; i < NR_GDMA_CHANNEL; i++) {
     	mt_reset_gdma_conf(i);
    }

#ifdef CONFIG_OF
    node = of_find_compatible_node(NULL, NULL, "mediatek,CQDMA");
    if (!node)
        printk(KERN_ERR"find CQDMA node failed!!!\n");
    else {
        CQ_DMA_BASE = of_iomap(node, 0);
    	printk("[CQDMA] CQ_DMA_BASE = 0x%p\n", CQ_DMA_BASE);
        WARN(!CQ_DMA_BASE, "unable to map CQDMA base registers!!!\n");    
    }
    cqdma_irq_num = irq_of_parse_and_map(node, 0);
    printk("[CQDMA] cqdma_irq_num = %d\n", cqdma_irq_num);

    /* get the interrupt line behaviour */
    if (of_property_read_u32_array(node, "interrupts",
			dma_info, ARRAY_SIZE(dma_info))){
	printk("[CQDMA] get irq flags from DTS fail!!\n");
    }
#endif

#ifdef CONFIG_OF
    ret = request_irq(cqdma_irq_num, gdma1_irq_handler, dma_info[2] | IRQF_SHARED, "CQDMA", &dma_ctrl);
#else
    ret = request_irq(CQ_DMA_IRQ_BIT_ID, gdma1_irq_handler, IRQF_TRIGGER_LOW | IRQF_SHARED, "CQDMA", &dma_ctrl);
#endif
    if (ret > 0)
        printk(KERN_ERR"GDMA1 IRQ LINE NOT AVAILABLE,ret 0x%x!!\n",ret);

    ret = driver_register(&mt_cqdma_drv.driver);
    if (ret == 0)
        printk("CQDMA init done...\n");
    else
        printk(KERN_ERR"CQDMA init FAIL, ret 0x%x!!!\n", ret);
	
#ifdef LDVT
    ret = driver_create_file(&mt_cqdma_drv.driver, &driver_attr_cqdma_dvt);
    if(ret == 0)
        printk("CQDMA create sysfs file done...\n");
    else
        printk(KERN_ERR"CQDMA create sysfs file init FAIL, ret 0x%x!!!\n", ret);
#endif
  
#ifdef CONFIG_ARM_LPAE
    mt_reg_sync_writel(0x1, DMA_SRC_4G_SUPPORT);
    mt_reg_sync_writel(0x1, DMA_DST_4G_SUPPORT);
    mt_reg_sync_writel(0x1, DMA_JUMP_4G_SUPPORT);
#endif

    printk("[CQDMA] Init CQDMA OK\n");
    
    return 0;
}

late_initcall(mt_init_dma);
