#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/mmc/host.h>
#include <mach/board.h>
#include <linux/seq_file.h>
#include <mach/mt_gpt.h>
#include <asm/io.h>
//for fpga early porting
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/scatterlist.h>
#include <mach/mt_typedefs.h>
//end for fpga early porting
#include "dbg.h"
#ifndef FPGA_PLATFORM
#include <mach/mt_clkmgr.h>
#endif
#ifdef MTK_MSDC_BRINGUP_DEBUG
#include <mach/mt_pmic_wrap.h>
#endif

#ifdef MTK_IO_PERFORMANCE_DEBUG
unsigned int g_mtk_mmc_perf_dbg = 0;
unsigned int g_mtk_mmc_dbg_range = 0;
unsigned int g_dbg_range_start = 0;
unsigned int g_dbg_range_end = 0;
unsigned int g_mtk_mmc_dbg_flag = 0;
unsigned int g_dbg_req_count = 0;
unsigned int g_dbg_raw_count = 0;
unsigned int g_dbg_write_count = 0;
unsigned int g_dbg_raw_count_old = 0;
unsigned int g_mtk_mmc_clear = 0;
int g_check_read_write = 0;
int g_i = 0;
unsigned long long g_req_buf[4000][30] = {{0}};
unsigned long long g_req_write_buf[4000][30] = {{0}};
unsigned long long g_req_write_count[4000] = {0};

unsigned long long g_mmcqd_buf[400][300] = {{0}};
char *g_time_mark[] = {
    "--start fetch request",
    "--end fetch request",
    "--start dma map this request",
    "--end dma map this request",
    "--start request",
    "--DMA start",
    "--DMA transfer done",
    "--start dma unmap request",
    "--end dma unmap request",
    "--end of request",
};
char *g_time_mark_vfs_write[] = {"--in vfs_write",
    "--before generic_segment_checks",
    "--after generic_segment_checks",
    "--after vfs_check_frozen",
    "--after generic_write_checks",
    "--after file_remove_suid",
    "--after file_update_time",
    "--after generic_file_direct_write",
    "--after generic_file_buffered_write",
    "--after filemap_write_and_wait_range",
    "--after invalidate_mapping_pages",
    "--after 2nd generic_file_buffered_write",
    "--before generic_write_sync",
    "--after generic_write_sync",
    "--out vfs_write"
};

#endif

/* for get transfer time with each trunk size, default not open */
#ifdef MTK_MMC_PERFORMANCE_TEST
unsigned int g_mtk_mmc_perf_test = 0;
#endif

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
unsigned int g_err_tune_dbg_count = 0; 
unsigned int g_err_tune_dbg_host = 0; 
unsigned int g_err_tune_dbg_cmd = 0; 
unsigned int g_err_tune_dbg_arg = 0; 
unsigned int g_err_tune_dbg_error = MTK_MSDC_ERROR_NONE; 
#endif

typedef enum
{
    SDHC_HIGHSPEED = 0,       /* 0x1 Host supports HS mode */
    UHS_SDR12,            /* 0x2 Host supports UHS SDR12 mode */
    UHS_SDR25,            /* 0x3 Host supports UHS SDR25 mode */
    UHS_SDR50,            /* 0x4 Host supports UHS SDR50 mode */
    UHS_SDR104,           /* 0x5 Host supports UHS SDR104/EMMC HS200 mode */
    UHS_DDR50,            /* 0x6 Host supports UHS DDR50 mode */
    EMMC_HS400,           /* 0x7 Host supports EMMC HS400 mode */
    CAPS_SPEED_NULL, 
}HOST_CAPS_SPEED_MODE;

typedef enum
{
    DRIVER_TYPE_A = 0,        /* 0x7 Host supports Driver Type A */
    DRIVER_TYPE_B,        /* 0x8 Host supports Driver Type B */
    DRIVER_TYPE_C,        /* 0x9 Host supports Driver Type C */
    DRIVER_TYPE_D,        /* 0xA Host supports Driver Type D */
    CAPS_DRIVE_NULL, 
}HOST_CAPS_DRIVE_TYPE;

typedef enum
{
    MAX_CURRENT_200 = 0,      /* 0xB Host max current limit is 200mA */
    MAX_CURRENT_400,      /* 0xC Host max current limit is 400mA */
    MAX_CURRENT_600,      /* 0xD Host max current limit is 600mA */
    MAX_CURRENT_800,      /* 0xE Host max current limit is 800mA */
    CAPS_CURRENT_NULL, 
}HOST_CAPS_MAX_CURRENT;

typedef enum
{
    SDXC_NO_POWER_CONTROL = 0,/*0xF   Host not supports >150mA current at 3.3V /3.0V/1.8V*/
    SDXC_POWER_CONTROL,   /*0x10 Host supports >150mA current at 3.3V /3.0V/1.8V*/
    CAPS_POWER_NULL, 
}HOST_CAPS_POWER_CONTROL;

static char cmd_buf[256];

/* for debug zone */
unsigned int sd_debug_zone[HOST_MAX_NUM]={
    0,
    0,
    0,
    0,
};

/* mode select */
u32 dma_size[HOST_MAX_NUM]={
    512,
    512,
    512,
    512,
};
msdc_mode drv_mode[HOST_MAX_NUM]={
    MODE_SIZE_DEP, /* using DMA or not depend on the size */
    MODE_SIZE_DEP,
    MODE_SIZE_DEP,
    MODE_SIZE_DEP,
};
unsigned char msdc_clock_src[HOST_MAX_NUM]={
    0,
    0,
    0,
    0,
};

u32 msdc_host_mode[HOST_MAX_NUM]={
    0,
    0,
    0,
    0,
};

u32 msdc_host_mode2[HOST_MAX_NUM]={
    0,
    0,
    0,
    0,
};

drv_mod msdc_drv_mode[HOST_MAX_NUM];
int sdio_cd_result = 1;

/* for driver profile */
#define TICKS_ONE_MS  (13000)
u32 gpt_enable = 0;
u32 sdio_pro_enable = 0;   /* make sure gpt is enabled */
static unsigned long long sdio_pro_time = 30;     /* no more than 30s */
static unsigned long long sdio_profiling_start=0;
struct sdio_profile sdio_perfomance = {0};  

u32 sdio_enable_tune = 0;
u32 sdio_iocon_dspl = 0;
u32 sdio_iocon_w_dspl = 0;
u32 sdio_iocon_rspl = 0;
u32 sdio_pad_tune_rrdly = 0;
u32 sdio_pad_tune_rdly = 0;
u32 sdio_pad_tune_wrdly = 0;
u32 sdio_dat_rd_dly0_0 = 0;
u32 sdio_dat_rd_dly0_1 = 0;
u32 sdio_dat_rd_dly0_2 = 0;
u32 sdio_dat_rd_dly0_3 = 0;
u32 sdio_dat_rd_dly1_0 = 0;
u32 sdio_dat_rd_dly1_1 = 0;
u32 sdio_dat_rd_dly1_2 = 0;
u32 sdio_dat_rd_dly1_3 = 0;
u32 sdio_clk_drv = 0;
u32 sdio_cmd_drv= 0;
u32 sdio_data_drv =0;
u32 sdio_tune_flag =0;

extern void msdc_dump_register(struct msdc_host *host);
extern void msdc_dump_info(u32 id);
extern unsigned int msdc_dump_reg_level;
extern struct msdc_host *mtk_msdc_host[];
extern enum cg_clk_id msdc_cg_clk_id[];

#ifndef FPGA_PLATFORM
extern void msdc_set_driving(struct msdc_host* host,struct msdc_hw* hw,bool sd_18);
extern void msdc_set_sr(struct msdc_host *host,int clk,int cmd, int dat, int rst, int ds);
extern void msdc_set_smt(struct msdc_host *host,int set_smt);
extern void msdc_set_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 value);
extern void msdc_get_rdtdsel_dbg(struct msdc_host *host, bool rdsel, u32 *value);
#endif
extern int ettagent_init(void);
extern void ettagent_exit(void);

#ifdef MTK_MSDC_BRINGUP_DEBUG
extern void __iomem *apmixed_base1;
extern void __iomem *topckgen_base;
#endif

static void msdc_set_field(void __iomem *address, unsigned int start_bit,unsigned int len,unsigned int value)
{
    unsigned long field;
    if(start_bit > 31 || start_bit < 0|| len > 32 || len <= 0)
        printk("[****SD_Debug****]reg filed beyoned (0~31) or length beyoned (1~32)\n");
    else{
        field = ((1 << len) -1) << start_bit;
        value &= (1 << len) -1; 
        printk("[****SD_Debug****]Original:0x%p (0x%x)\n",address,sdr_read32(address));
        sdr_set_field(address,field, value);
        printk("[****SD_Debug****]Modified:0x%p (0x%x)\n",address,sdr_read32(address));
    }
}

static void msdc_get_field(void __iomem *address, unsigned int start_bit,unsigned int len,unsigned int value)
{
    unsigned long field;
    if(start_bit > 31 || start_bit < 0|| len > 32 || len <= 0)
        printk("[****SD_Debug****]reg filed beyoned (0~31) or length beyoned (1~32)\n");
    else{
        field = ((1 << len) -1) << start_bit;
        sdr_get_field(address,field,value);
        printk("[****SD_Debug****]Reg:0x%p start_bit(%d)len(%d)(0x%x)\n",address,start_bit,len,value);
    }
}

static void msdc_init_gpt(void)
{
#if 0
    GPT_CONFIG config;    
    
    config.num  = GPT6;
    config.mode = GPT_FREE_RUN;
    config.clkSrc = GPT_CLK_SRC_SYS;
    config.clkDiv = GPT_CLK_DIV_1;   /* 13MHz GPT6 */
            
    if (GPT_Config(config) == FALSE )
        return;                       
        
    GPT_Start(GPT6);    
#endif
}

u32 msdc_time_calc(u32 old_L32, u32 old_H32, u32 new_L32, u32 new_H32)
{
    u32 ret = 0; 
    
    if (new_H32 == old_H32) {
        ret = new_L32 - old_L32;
    } else if(new_H32 == (old_H32 + 1)) {
        if (new_L32 > old_L32) {    
            printk("msdc old_L<0x%x> new_L<0x%x>\n", old_L32, new_L32);    
        }
        ret = (0xffffffff - old_L32);            
        ret += new_L32; 
    } else {
        printk("msdc old_H<0x%x> new_H<0x%x>\n", old_H32, new_H32);        
    }

    return ret;            
}

void msdc_sdio_profile(struct sdio_profile* result)
{
    struct cmd_profile*  cmd;
    u32 i;     
    
    printk("sdio === performance dump ===\n");
    printk("sdio === total execute tick<%d> time<%dms> Tx<%dB> Rx<%dB>\n", 
                    result->total_tc, result->total_tc / TICKS_ONE_MS, 
                    result->total_tx_bytes, result->total_rx_bytes);    

    /* CMD52 Dump */
    cmd = &result->cmd52_rx; 
    printk("sdio === CMD52 Rx <%d>times tick<%d> Max<%d> Min<%d> Aver<%d>\n", cmd->count, cmd->tot_tc, 
                    cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count);     
    cmd = &result->cmd52_tx; 
    printk("sdio === CMD52 Tx <%d>times tick<%d> Max<%d> Min<%d> Aver<%d>\n", cmd->count, cmd->tot_tc, 
                    cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count);   
                    
    /* CMD53 Rx bytes + block mode */
    for (i=0; i<512; i++) {
        cmd = &result->cmd53_rx_byte[i];
        if (cmd->count) {
            printk("sdio<%6d><%3dB>_Rx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc, 
                             cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
                             cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));                            
        }    
    }  
    for (i=0; i<100; i++) {
        cmd = &result->cmd53_rx_blk[i];
        if (cmd->count) {
            printk("sdio<%6d><%3d>B_Rx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc, 
                             cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
                             cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));                            
        }    
    }

    /* CMD53 Tx bytes + block mode */
    for (i=0; i<512; i++) {
        cmd = &result->cmd53_tx_byte[i];
        if (cmd->count) {
            printk("sdio<%6d><%3dB>_Tx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc, 
                             cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
                             cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));                              
        }    
    }          
    for (i=0; i<100; i++) {
        cmd = &result->cmd53_tx_blk[i];
        if (cmd->count) {
            printk("sdio<%6d><%3d>B_Tx_<%9d><%9d><%6d><%6d>_<%9dB><%2dM>\n", cmd->count, i, cmd->tot_tc, 
                             cmd->max_tc, cmd->min_tc, cmd->tot_tc/cmd->count,
                             cmd->tot_bytes, (cmd->tot_bytes/10)*13 / (cmd->tot_tc/10));                                
        }    
    }     
    
    printk("sdio === performance dump done ===\n");      
}

//========= sdio command table ===========
void msdc_performance(u32 opcode, u32 sizes, u32 bRx, u32 ticks)
{
    struct sdio_profile* result = &sdio_perfomance; 
    struct cmd_profile*  cmd; 
    u32 block;         
    long long endtime;

    if (sdio_pro_enable == 0) {
        return;
    }

    if (opcode == 52) {
        cmd = bRx ?  &result->cmd52_rx : &result->cmd52_tx;       
    } else if (opcode == 53) {
        if (sizes < 512) {
            cmd = bRx ?  &result->cmd53_rx_byte[sizes] : &result->cmd53_tx_byte[sizes];        
        } else {
            block = sizes / 512; 
            if (block >= 99) {
               printk("cmd53 error blocks\n"); 
               while(1);    
            }
            cmd = bRx ?  &result->cmd53_rx_blk[block] : &result->cmd53_tx_blk[block];           
        }       
    } else {
        return;     
    }
        
    /* update the members */
    if (ticks > cmd->max_tc){
        cmd->max_tc = ticks;    
    }
    if (cmd->min_tc == 0 || ticks < cmd->min_tc) {
        cmd->min_tc = ticks;       
    }
    cmd->tot_tc += ticks;
    cmd->tot_bytes += sizes; 
    cmd->count ++; 
    
    if (bRx) {
        result->total_rx_bytes += sizes;        
    } else {
        result->total_tx_bytes += sizes;     
    }
    result->total_tc += ticks; 
#if 0    
    /* dump when total_tc > 30s */
    if (result->total_tc >= sdio_pro_time * TICKS_ONE_MS * 1000) {
        msdc_sdio_profile(result);       
        memset(result, 0 , sizeof(struct sdio_profile));                                             
    }
#endif



     endtime = sched_clock();
     if((endtime-sdio_profiling_start)>=  sdio_pro_time * 1000000000) {
     msdc_sdio_profile(result);       
     memset(result, 0 , sizeof(struct sdio_profile));                                              
     sdio_profiling_start = endtime;
     }    


}

#define COMPARE_ADDRESS_MMC   0x402000
#define COMPARE_ADDRESS_SD    0x2000
#define COMPARE_ADDRESS_SDIO  0x0
#define COMPARE_ADDRESS_SD_COMBO  0x2000

#define MSDC_MULTI_BUF_LEN  (4*1024)

static DEFINE_MUTEX(sd_lock);
static DEFINE_MUTEX(emmc_lock);

u32 msdc_multi_wbuf_sd[MSDC_MULTI_BUF_LEN + 1024];
u32 msdc_multi_rbuf_sd[MSDC_MULTI_BUF_LEN + 1024];

u32 msdc_multi_wbuf_emmc[MSDC_MULTI_BUF_LEN + 1024];
u32 msdc_multi_rbuf_emmc[MSDC_MULTI_BUF_LEN + 1024];
u8 read_write_state=0;  //0:stop, 1:read, 2:write
#define is_card_present(h)     (((struct msdc_host*)(h))->card_inserted)

extern int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd);
/*
  * @read, bit0: 1:read/0:write; bit1: 0:compare/1:not compare
*/
static int sd_multi_rw_compare_slave(int host_num, int read, uint address)
{
#ifdef MTK_EMMC_SUPPORT
    char l_buf[512];
#endif
    struct scatterlist msdc_sg;
    struct mmc_data  msdc_data;
    struct mmc_command msdc_cmd;
    struct mmc_command msdc_stop;
    
#ifdef MTK_MSDC_USE_CMD23
    struct mmc_command msdc_sbc;
#endif
    
    struct mmc_request  msdc_mrq;
    struct msdc_host *host_ctl;
    //struct msdc_host *host = mtk_msdc_host[host_num];
    int result = 0, forIndex = 0;
    u8 *wPtr;
    u8 wData[200]= {
            0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,    //worst1
            0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,
            0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00,

            0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,    //worst2
            0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
            0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
    
            0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff,    //worst3
            0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
            0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,

            0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,    //worst4
            0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
            0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,

            0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55,    //worst5
            0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f,
            0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
            0x04, 0xfb, 0x04, 0x04, 0x04, 0xfb, 0xfb, 0xfb,
            0x04, 0xfb, 0xfb, 0xfb, 0x02, 0x02, 0x02, 0xfd,
            0x02, 0x02, 0x02, 0xfd, 0xfd, 0xfd, 0x02, 0xfd,
            0xfd, 0xfd, 0x01, 0x01, 0x01, 0xfe, 0x01, 0x01,
            0x01, 0xfe, 0xfe, 0xfe, 0x01, 0xfe, 0xfe, 0xfe,
            0x80, 0x80, 0x80, 0x7f, 0x80, 0x80, 0x80, 0x7f,
            0x7f, 0x7f, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
            0x40, 0x40, 0x80, 0x7f, 0x7f, 0x7f, 0x40, 0x40,
            0x20, 0xdf, 0x20, 0x20, 0x20, 0xdf, 0xdf, 0xdf,
            0x10, 0x10, 0x10, 0xef, 0xef, 0x10, 0xef, 0xef,    
    }; 
    
        
    if(host_num >= HOST_MAX_NUM || host_num < 0){
        printk("[%s]:invalid host id: %d\n", __func__, host_num);
        return -1; 
    }
    
    host_ctl = mtk_msdc_host[host_num];
    if(!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
    {
        printk(" there is no card initialized in host[%d]\n",host_num);
        return -1; 
    }

    if(!is_card_present(host_ctl))
    {
        printk("  [%s]: card is removed!\n", __func__);
        return -1;
    }

    mmc_claim_host(host_ctl->mmc);

#ifdef MTK_EMMC_SUPPORT
    if (host_ctl->hw->host_function == MSDC_EMMC) {
        mmc_send_ext_csd(host_ctl->mmc->card, l_buf);

        /* make sure access partition is user data area */
        if (0 != (l_buf[179] & 0x7)){
            /* set back to access user area */
            printk("set back to user area\n");
            l_buf[179] &= ~0x7;
            l_buf[179] |= 0x0;
        
            mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
        }
    }
#endif

    memset(&msdc_data, 0, sizeof(struct mmc_data));
    memset(&msdc_mrq, 0, sizeof(struct mmc_request));
    memset(&msdc_cmd, 0, sizeof(struct mmc_command));
    memset(&msdc_stop, 0, sizeof(struct mmc_command));

#ifdef MTK_MSDC_USE_CMD23
    memset(&msdc_sbc, 0, sizeof(struct mmc_command));
#endif

    msdc_mrq.cmd = &msdc_cmd;
    msdc_mrq.data = &msdc_data;

    if ((read & 0x1) == 0x1){
        //init read command
        msdc_data.flags = MMC_DATA_READ;
        msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
        msdc_data.blocks = sizeof(msdc_multi_rbuf_sd) / 512;
        wPtr =(u8*)msdc_multi_rbuf_sd;
        //init read buffer
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN*4;forIndex++)
            *(wPtr + forIndex) = 0x0;                        
        //for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++)
        //printk("R_buffer[0x%x] \n",msdc_multi_rbuf_sd[forIndex]);
    } else {
        //init write command
        msdc_data.flags = MMC_DATA_WRITE;
        msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
        msdc_data.blocks = sizeof(msdc_multi_wbuf_sd) / 512;
        //init write buffer
        wPtr =(u8*)msdc_multi_wbuf_sd;
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN*4;forIndex++)
            *(wPtr + forIndex) = wData[forIndex%200];                
        //for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++)
        //printk("W_buffer[0x%x]\n",msdc_multi_wbuf_sd[forIndex]);
    }

    msdc_cmd.arg = address;

    BUG_ON(!host_ctl->mmc->card);

#ifdef MTK_MSDC_USE_CMD23
    if ((mmc_card_mmc(host_ctl->mmc->card) || (mmc_card_sd(host_ctl->mmc->card) && host_ctl->mmc->card->scr.cmds & SD_SCR_CMD23_SUPPORT)) && 
            !(host_ctl->mmc->card->quirks & MMC_QUIRK_BLK_NO_CMD23)){
        msdc_mrq.sbc = &msdc_sbc;
        msdc_mrq.sbc->opcode = MMC_SET_BLOCK_COUNT;
        msdc_mrq.sbc->arg = msdc_data.blocks;
        msdc_mrq.sbc->flags = MMC_RSP_R1 | MMC_CMD_AC;
    }
#else
    msdc_stop.opcode = MMC_STOP_TRANSMISSION;
    msdc_stop.arg = 0;
    msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
    msdc_data.stop = &msdc_stop;
#endif
    
    if (!mmc_card_blockaddr(host_ctl->mmc->card)){
        //printk("this device use byte address!!\n");
        msdc_cmd.arg <<= 9;
    }
    msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    msdc_data.blksz = 512;
    msdc_data.sg = &msdc_sg;
    msdc_data.sg_len = 1;

    if((read & 0x1) == 0x1)
        sg_init_one(&msdc_sg, msdc_multi_rbuf_sd, sizeof(msdc_multi_rbuf_sd));
    else
        sg_init_one(&msdc_sg, msdc_multi_wbuf_sd, sizeof(msdc_multi_wbuf_sd));
        
    mmc_set_data_timeout(&msdc_data, host_ctl->mmc->card);
    mmc_wait_for_req(host_ctl->mmc, &msdc_mrq);

    if (read == 0x1){
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++){                    
            //printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\t\n", forIndex, msdc_multi_wbuf_sd[forIndex], msdc_multi_rbuf_sd[forIndex]);
            if(msdc_multi_wbuf_sd[forIndex]!=msdc_multi_rbuf_sd[forIndex]){
                printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\tfailed\n", forIndex, msdc_multi_wbuf_sd[forIndex], msdc_multi_rbuf_sd[forIndex]);
                result =-1;
            }
        }
        /*if(result == 0)
            printk("pid[%d][%s]: data compare successed!!\n", current->pid, __func__);
        else                    
            printk("pid[%d][%s]: data compare failed!! \n", current->pid, __func__);*/
    }

    mmc_release_host(host_ctl->mmc); 
  
    if (msdc_cmd.error)
        result = msdc_cmd.error;

    if (msdc_data.error){
        result = msdc_data.error;
    } else {
        result = 0;
    }

    return result;
}

static int sd_multi_rw_compare(int host_num, uint address, int count)
{
    int i=0, j=0;
    int error = 0;
    
    if(host_num >= HOST_MAX_NUM || host_num < 0){
        printk("[%s]:invalid host id: %d\n", __func__, host_num);
        return -1; 
    }
        
    for(i=0; i<count; i++) 
    {
        //printk("============ cpu[%d] pid[%d]: start the %d time compare ============\n", task_cpu(current), current->pid, i);

        mutex_lock(&sd_lock);
        error = sd_multi_rw_compare_slave(host_num, 0, address); //write
        if(error)
        {
            printk("[%s]: failed to write data, error=%d\n", __func__, error);
            mutex_unlock(&sd_lock);
            break;
        }
    
        for(j=0; j<1; j++){
            error = sd_multi_rw_compare_slave(host_num, 1, address); //read
            if(error)
            {
                printk("[%s]: failed to read data, error=%d\n", __func__, error);
                break;
            }
        }
        if(error)
            printk("============ cpu[%d] pid[%d]: FAILED the %d time compare ============\n", task_cpu(current), current->pid, i);
        else
            printk("============ cpu[%d] pid[%d]: FINISH the %d time compare ============\n", task_cpu(current), current->pid, i);
    
        mutex_unlock(&sd_lock);
    }
    
    if(i == count)
        printk("pid[%d]: successed to compare data within %d times\n", current->pid, count);
        
    return error;
}

static int emmc_multi_rw_compare_slave(int host_num, int read, uint address)
{
#ifdef MTK_EMMC_SUPPORT
    char l_buf[512];
#endif
    struct scatterlist msdc_sg;
    struct mmc_data  msdc_data;
    struct mmc_command msdc_cmd;
    struct mmc_command msdc_stop;

#ifdef MTK_MSDC_USE_CMD23
    struct mmc_command msdc_sbc;
#endif

    struct mmc_request  msdc_mrq;
    struct msdc_host *host_ctl;
    //struct msdc_host *host = mtk_msdc_host[host_num];
    int result = 0, forIndex = 0;
    u8 *wPtr;
    u8 wData[16]= {
            0x67, 0x45, 0x23, 0x01,
            0xef, 0xcd, 0xab, 0x89,
            0xce, 0x8a, 0x46, 0x02,
            0xde, 0x9b, 0x57, 0x13 }; 
    
    host_ctl = mtk_msdc_host[host_num];
    if(!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
    {
        printk(" there is no card initialized in host[%d]\n",host_num);
        return -1; 
    }

    if(!is_card_present(host_ctl))
    {
        printk("  [%s]: card is removed!\n", __func__);
        return -1;
    }
 
    mmc_claim_host(host_ctl->mmc);

#ifdef MTK_EMMC_SUPPORT
    if (host_ctl->hw->host_function == MSDC_EMMC) {
        mmc_send_ext_csd(host_ctl->mmc->card, l_buf);

        /* make sure access partition is user data area */
        if (0 != (l_buf[179] & 0x7)){
            /* set back to access user area */
            printk("set back to user area\n");
            l_buf[179] &= ~0x7;
            l_buf[179] |= 0x0;
            
            mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
        }
    }
#endif

    memset(&msdc_data, 0, sizeof(struct mmc_data));
    memset(&msdc_mrq, 0, sizeof(struct mmc_request));
    memset(&msdc_cmd, 0, sizeof(struct mmc_command));
    memset(&msdc_stop, 0, sizeof(struct mmc_command));

#ifdef MTK_MSDC_USE_CMD23
    memset(&msdc_sbc, 0, sizeof(struct mmc_command));
#endif

    msdc_mrq.cmd = &msdc_cmd;
    msdc_mrq.data = &msdc_data;

    if (read){
        //init read command
        msdc_data.flags = MMC_DATA_READ;
        msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
        msdc_data.blocks = sizeof(msdc_multi_rbuf_emmc) / 512;
        wPtr =(u8*)msdc_multi_rbuf_emmc;
        //init read buffer
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN*4;forIndex++)
            *(wPtr + forIndex) = 0x0;                        
        //for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++)
        //printk("R_buffer[0x%x] \n",msdc_multi_rbuf_emmc[forIndex]);
    } else {
        //init write command
        msdc_data.flags = MMC_DATA_WRITE;
        msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
        msdc_data.blocks = sizeof(msdc_multi_wbuf_emmc) / 512;
        //init write buffer
        wPtr =(u8*)msdc_multi_wbuf_emmc;
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN*4;forIndex++)
            *(wPtr + forIndex) = wData[forIndex%16];                
        //for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++)
        //printk("W_buffer[0x%x]\n",msdc_multi_wbuf_emmc[forIndex]);
    }

    msdc_cmd.arg = address;

    BUG_ON(!host_ctl->mmc->card);
#ifdef MTK_MSDC_USE_CMD23
    if ((mmc_card_mmc(host_ctl->mmc->card) || (mmc_card_sd(host_ctl->mmc->card) && host_ctl->mmc->card->scr.cmds & SD_SCR_CMD23_SUPPORT)) && 
            !(host_ctl->mmc->card->quirks & MMC_QUIRK_BLK_NO_CMD23)){
        msdc_mrq.sbc = &msdc_sbc;
        msdc_mrq.sbc->opcode = MMC_SET_BLOCK_COUNT;
        msdc_mrq.sbc->arg = msdc_data.blocks;
        msdc_mrq.sbc->flags = MMC_RSP_R1 | MMC_CMD_AC;
    }
#else
    msdc_stop.opcode = MMC_STOP_TRANSMISSION;
    msdc_stop.arg = 0;
    msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
    msdc_data.stop = &msdc_stop;
#endif

    if (!mmc_card_blockaddr(host_ctl->mmc->card)){
        //printk("this device use byte address!!\n");
        msdc_cmd.arg <<= 9;
    }
    msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

    msdc_data.blksz = 512;
    msdc_data.sg = &msdc_sg;
    msdc_data.sg_len = 1;

    if(read)
        sg_init_one(&msdc_sg, msdc_multi_rbuf_emmc, sizeof(msdc_multi_rbuf_emmc));
    else
        sg_init_one(&msdc_sg, msdc_multi_wbuf_emmc, sizeof(msdc_multi_wbuf_emmc));
        
    mmc_set_data_timeout(&msdc_data, host_ctl->mmc->card);
    mmc_wait_for_req(host_ctl->mmc, &msdc_mrq);

    if (read){
        for(forIndex=0;forIndex<MSDC_MULTI_BUF_LEN;forIndex++){                    
            //printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\t\n", forIndex, msdc_multi_wbuf_emmc[forIndex], msdc_multi_rbuf_emmc[forIndex]);
            if(msdc_multi_wbuf_emmc[forIndex]!=msdc_multi_rbuf_emmc[forIndex]){
                printk("index[%d]\tW_buffer[0x%x]\tR_buffer[0x%x]\tfailed\n", forIndex, msdc_multi_wbuf_emmc[forIndex], msdc_multi_rbuf_emmc[forIndex]);
                result =-1;
            }
        }
        /*if(result == 0)
            printk("pid[%d][%s]: data compare successed!!\n", current->pid, __func__);
        else                    
            printk("pid[%d][%s]: data compare failed!! \n", current->pid, __func__);*/
    }

    mmc_release_host(host_ctl->mmc);
  
    if (msdc_cmd.error)
        result = msdc_cmd.error;

    if (msdc_data.error){
        result = msdc_data.error;
    } else {
        result = 0;
    }

    return result;
}

int emmc_multi_rw_compare(int host_num, uint address, int count)
{
    int i=0, j=0;
    int error = 0;
    
    for(i=0; i<count; i++) 
    {
        //printk("============ cpu[%d] pid[%d]: start the %d time compare ============\n", task_cpu(current), current->pid, i);

        mutex_lock(&emmc_lock);
        error = emmc_multi_rw_compare_slave(host_num, 0, address); //write
        if(error)
        {
            printk("[%s]: failed to write data, error=%d\n", __func__, error);
            mutex_unlock(&emmc_lock);
            break;
        }
    
        for(j=0; j<1; j++){
            error = emmc_multi_rw_compare_slave(host_num, 1, address); //read
            if(error)
            {
                printk("[%s]: failed to read data, error=%d\n", __func__, error);
                break;
            }
        }
        if(error)
           printk("============ cpu[%d] pid[%d]: FAILED the %d time compare ============\n", task_cpu(current), current->pid, i);
        else
           printk("============ cpu[%d] pid[%d]: FINISH the %d time compare ============\n", task_cpu(current), current->pid, i);
        
        mutex_unlock(&emmc_lock);
    }
    
    if(i == count)
        printk("pid[%d]: successed to compare data within %d times\n", current->pid, count);
        
    return error;
}



#define MAX_THREAD_NUM_FOR_SMP 20

/* make the test can run on 4GB card */
static uint smp_address_on_sd[MAX_THREAD_NUM_FOR_SMP] = 
{
  0x2000,
  0x80000,
  0x100000,
  0x180000,
  0x200000,  // 1GB
  0x202000,
  0x280000,
  0x300000,
  0x380000,
  0x400000,  // 2GB
  0x402000,
  0x480000,
  0x500000,
  0x580000,
  0x600000,
  0x602000,  //3GB
  0x660000,  // the real total size of 4GB sd card is below 4GB
  0x680000,
  0x6a0000,
  0x6b0000,
};

/* cause the system run on the emmc storage, 
 * so do not to access the first 2GB region */
static uint smp_address_on_mmc[MAX_THREAD_NUM_FOR_SMP] = 
{
  0x402000,
  0x410000,
  0x520000,
  0x530000,
  0x640000,  
  0x452000,
  0x460000,
  0x470000,
  0x480000,
  0x490000,  
  0x4a2000,
  0x4b0000,
  0x5c0000,
  0x5d0000,
  0x6e0000,
  0x602000,  
  0x660000,  // the real total size of 4GB sd card is below 4GB
  0x680000,
  0x6a0000,
  0x6b0000,
};

static uint smp_address_on_sd_combo[MAX_THREAD_NUM_FOR_SMP] = 
{
  0x2000,
  0x20000, 
  0x200000,
  0x2000000,
  0x2200000,
  0x2400000,
  0x2800000,
  0x2c00000,
  0x4000000,
  0x4200000,
  0x4400000,
  0x4800000,
  0x4c00000,
  0x8000000,
  0x8200000,
  0x8400000,
  0x8800000,
  0x8c00000,
  0xc000000,
  0xc200000
};
struct write_read_data{
    int host_id;        //the target host you want to do SMP test on.  
    uint start_address; //where you want to do write/read of the memory card 
    int count;          //how many times you want to do read after write bit by bit comparison 
};

static struct write_read_data wr_data[HOST_MAX_NUM][MAX_THREAD_NUM_FOR_SMP]; 
/*
 * 2012-03-25
 * the SMP thread function
 * do read after write the memory card, and bit by bit comparison
 */
static int write_read_thread(void* ptr)
{
    struct write_read_data* data = (struct write_read_data*)ptr;
    
    if (1 == data->host_id){
        printk("sd thread\n");
        sd_multi_rw_compare(data->host_id, data->start_address, data->count); 
    } else if(0 == data->host_id){
        printk("emmc thread\n");
        emmc_multi_rw_compare(data->host_id, data->start_address, data->count); 
    }
    return 0; 
}

/* 
 * 2012-03-25
 * function:         do SMP test on the same one MSDC host
 * thread_num:       the number of thread you want to trigger on this host.
 * host_id:          the target host you want to do SMP test on.
 * count:            how many times you want to do read after write bit by bit comparison in each thread.
 * multi_address:    whether do read/write the same/different address of the memory card in each thread.
 */
static int smp_test_on_one_host(int thread_num, int host_id, int count, int multi_address)
{
    int i=0, ret=0;
    char thread_name[128];
    struct msdc_host *host_ctl; 

    printk("============================[%s] start ================================\n\n", __func__);
    printk(" host %d run %d thread, each thread run %d RW comparison\n", host_id, thread_num, count);
    if(host_id >= HOST_MAX_NUM || host_id < 0)
    {
        printk(" bad host id: %d\n", host_id);
        ret = -1;
        goto out; 
    }
    if(thread_num > MAX_THREAD_NUM_FOR_SMP)// && (multi_address != 0))
    {
        printk(" too much thread for SMP test, thread_num=%d\n", thread_num);
        ret = -1;
        goto out; 
    }

    host_ctl = mtk_msdc_host[host_id];
    if(!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
    {
        printk(" there is no card initialized in host[%d]\n",host_id);
        ret = -1;
        goto out; 
    }


    for(i=0; i<thread_num; i++)
    {
        switch(host_ctl->mmc->card->type)
        {
            case MMC_TYPE_MMC:
                if(!multi_address)
                    wr_data[host_id][i].start_address = COMPARE_ADDRESS_MMC;
                else
                    wr_data[host_id][i].start_address = smp_address_on_mmc[i];
                if(i == 0)
                    printk(" MSDC[%d], MMC:\n", host_id);
                break;
            case MMC_TYPE_SD:
                if(!multi_address)
                    wr_data[host_id][i].start_address = COMPARE_ADDRESS_SD;
                else
                    wr_data[host_id][i].start_address = smp_address_on_sd[i];
                if(i == 0)
                    printk(" MSDC[%d], SD:\n", host_id);
                break;
            case MMC_TYPE_SDIO:
                if(i == 0)
                {
                    printk(" MSDC[%d], SDIO:\n", host_id);
                    printk("    please manually trigger wifi application instead of write/read something on SDIO card\n");
                }
                ret = -1;
                goto out;
            case MMC_TYPE_SD_COMBO:
                if(!multi_address)
                    wr_data[host_id][i].start_address = COMPARE_ADDRESS_SD_COMBO;
                else
                    wr_data[host_id][i].start_address = smp_address_on_sd_combo[i];
                if(i == 0)
                    printk(" MSDC[%d], SD_COMBO:\n", host_id);
                break;
            default:
                if(i == 0)
                    printk(" MSDC[%d], cannot recognize this card\n", host_id);
                ret = -1;
                goto out;
         }
         wr_data[host_id][i].host_id = host_id;
         wr_data[host_id][i].count = count;
         sprintf(thread_name, "msdc_H%d_T%d", host_id, i); 
         kthread_run(write_read_thread, &wr_data[host_id][i], thread_name);
         printk("    start thread: %s, at address 0x%x\n", thread_name, wr_data[host_id][i].start_address);
    }
out: 
    printk("============================[%s] end ================================\n\n", __func__);
    return ret;
} 
  
/* 
 * 2012-03-25
 * function:         do SMP test on all MSDC hosts
 * thread_num:       the number of thread you want to trigger on this host.
 * count:            how many times you want to do read after write bit by bit comparison in each thread.
 * multi_address:    whether do read/write the same/different address of the memory card in each thread.
 */
static int smp_test_on_all_host(int thread_num, int count, int multi_address)
{
    int i=0;
    int j=0;
    int ret=0;
    char thread_name[128];
    struct msdc_host *host_ctl;
    
    printk("============================[%s] start ================================\n\n", __func__);
    printk(" each host run %d thread, each thread run %d RW comparison\n", thread_num, count);
    if(thread_num > MAX_THREAD_NUM_FOR_SMP) //&& (multi_address != 0))
    {
        printk(" too much thread for SMP test, thread_num=%d\n", thread_num);
        ret = -1;
        goto out;
    }

    for(i=0; i<HOST_MAX_NUM; i++)
    {
        host_ctl = mtk_msdc_host[i];
        if(!host_ctl || !host_ctl->mmc || !host_ctl->mmc->card)
        {
            printk(" MSDC[%d], no card is initialized\n", i);
            continue;
        }
        if(host_ctl->mmc->card->type == MMC_TYPE_SDIO)
        {
            printk(" MSDC[%d], SDIO, please manually trigger wifi application instead of write/read something on SDIO card\n", i);
            continue;
        }
        for(j=0; j<thread_num; j++)
        {
            wr_data[i][j].host_id = i;
            wr_data[i][j].count = count;
            switch(host_ctl->mmc->card->type)
            {
                case MMC_TYPE_MMC:
                    if(!multi_address)
                        wr_data[i][j].start_address = COMPARE_ADDRESS_MMC;
                    else
                        wr_data[i][j].start_address = smp_address_on_mmc[i];
                    if(j == 0)
                        printk(" MSDC[%d], MMC:\n ", i);
                    break;
                case MMC_TYPE_SD:
                    if(!multi_address)
                        wr_data[i][j].start_address = COMPARE_ADDRESS_SD;
                    else
                        wr_data[i][j].start_address = smp_address_on_sd[i];
                    if(j == 0)
                        printk(" MSDC[%d], SD:\n", i);
                    break;
                case MMC_TYPE_SDIO:
                    if(j == 0)
                    {
                        printk(" MSDC[%d], SDIO:\n", i);
                        printk("    please manually trigger wifi application instead of write/read something on SDIO card\n");
                    }
                    ret = -1;
                    goto out;
                case MMC_TYPE_SD_COMBO:
                    if(!multi_address)
                        wr_data[i][j].start_address = COMPARE_ADDRESS_SD_COMBO;
                    else
                        wr_data[i][j].start_address = smp_address_on_sd_combo[i];
                    if(j == 0)
                        printk(" MSDC[%d], SD_COMBO:\n", i);
                    break;
                default:
                    if(j == 0)
                        printk(" MSDC[%d], cannot recognize this card\n", i);
                    ret = -1;
                    goto out;
            }
            sprintf(thread_name, "msdc_H%d_T%d", i, j); 
            kthread_run(write_read_thread, &wr_data[i][j], thread_name);
            printk("    start thread: %s, at address: 0x%x\n", thread_name, wr_data[i][j].start_address);
        }
    }
out:
    printk("============================[%s] end ================================\n\n", __func__);
    return ret;
}


static int msdc_help_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "\n====================[msdc_help]=====================\n");

    seq_printf(m, "\n   LOG control:           echo %x [host_id] [debug_zone] > msdc_debug\n", SD_TOOL_ZONE);
    seq_printf(m, "          [debug_zone]       DMA:0x%x,  CMD:0x%x,  RSP:0x%x,   INT:0x%x,   CFG:0x%x,  FUC:0x%x,\n", 
                                                  DBG_EVT_DMA, DBG_EVT_CMD, DBG_EVT_RSP, DBG_EVT_INT, DBG_EVT_CFG, DBG_EVT_FUC);
    seq_printf(m, "                             OPS:0x%x, FIO:0x%x, WRN:0x%x, PWR:0x%x, CLK:0x%x, RW:0x%x, NRW:0x%x, CHE:0x%x\n", 
                                                  DBG_EVT_OPS, DBG_EVT_FIO, DBG_EVT_WRN, DBG_EVT_PWR, DBG_EVT_CLK, DBG_EVT_RW, DBG_EVT_NRW, DBG_EVT_CHE);
    seq_printf(m, "\n   DMA mode:\n");
    seq_printf(m, "          set DMA mode:      echo %x 0 [host_id] [dma_mode] [dma_size] > msdc_debug\n", SD_TOOL_DMA_SIZE);
    seq_printf(m, "          get DMA mode:      echo %x 1 [host_id] > msdc_debug\n", SD_TOOL_DMA_SIZE);
    seq_printf(m, "            [dma_mode]       0:PIO, 1:DMA, 2:SIZE_DEP\n");
    seq_printf(m, "            [dma_size]       valid for SIZE_DEP mode, the min size can trigger the DMA mode\n");
    seq_printf(m, "\n   SDIO profile:          echo %x [enable] [time] > msdc_debug\n", SD_TOOL_SDIO_PROFILE);
    seq_printf(m, "\n   CLOCK control:  \n");
    seq_printf(m, "          set clk src:       echo %x 0 [host_id] [clk_src] > msdc_debug\n", SD_TOOL_CLK_SRC_SELECT);
    seq_printf(m, "          get clk src:       echo %x 1 [host_id] > msdc_debug\n", SD_TOOL_CLK_SRC_SELECT);
    seq_printf(m, "            [clk_src]        0:SYS PLL(26Mhz), 1:3G PLL(197Mhz), 2:AUD PLL(208Mhz)\n");
    seq_printf(m, "\n   REGISTER control:\n");
    seq_printf(m, "          write register:    echo %x 0 [host_id] [register_offset] [value] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          read register:     echo %x 1 [host_id] [register_offset] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          write mask:        echo %x 2 [host_id] [register_offset] [start_bit] [len] [value] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          read mask:         echo %x 3 [host_id] [register_offset] [start_bit] [len] > msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          dump all internal: echo %x 4 [host_id]> msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "          dump all:          echo %x 5 [host_id]> msdc_debug\n", SD_TOOL_REG_ACCESS);
    seq_printf(m, "\n   DRVING control: \n"); 
    seq_printf(m, "          set driving:       echo %x [host_id] [clk_drv] [cmd_drv] [dat_drv] [rst_drv] [ds_drv] [voltage] > msdc_debug\n", SD_TOOL_SET_DRIVING);
    seq_printf(m, "            [voltage]        0x18:18v, 0x33:33v\n");
    seq_printf(m, "\n   DESENSE control: \n");
    seq_printf(m, "          write register:    echo %x 0 [value] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          read register:     echo %x 1 > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          write mask:        echo %x 2 [start_bit] [len] [value] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "          read mask:         echo %x 3 [start_bit] [len] > msdc_debug\n", SD_TOOL_DESENSE);
    seq_printf(m, "\n   RW_COMPARE test:       echo %x [host_id] [compare_count] > msdc_debug\n", RW_BIT_BY_BIT_COMPARE);
    seq_printf(m, "          [compare_count]    how many time you want to \"write=>read=>compare\"\n");
    seq_printf(m, "\n   SMP_ON_ONE_HOST test:  echo %x [host_id] [thread_num] [compare_count] [multi_address] > msdc_debug\n", SMP_TEST_ON_ONE_HOST);
    seq_printf(m, "          [thread_num]       how many R/W comparision thread you want to run at host_id\n");
    seq_printf(m, "          [compare_count]    how many time you want to \"write=>read=>compare\" in each thread\n");
    seq_printf(m, "          [multi_address]    whether read/write different address in each thread, 0:No, 1:Yes\n");
    seq_printf(m, "\n   SMP_ON_ALL_HOST test:  echo %x [thread_num] [compare_count] [multi_address] > msdc_debug\n", SMP_TEST_ON_ALL_HOST);
    seq_printf(m, "          [thread_num]       how many R/W comparision thread you want to run at each host\n");
    seq_printf(m, "          [compare_count]    how many time you want to \"write=>read=>compare\" in each thread\n");
    seq_printf(m, "          [multi_address]    whether read/write different address in each thread, 0:No, 1:Yes\n");
    seq_printf(m, "\n   SPEED_MODE control:\n"); 
    seq_printf(m, "          set speed mode:    echo %x 0 [host_id] [speed_mode] [driver_type] [max_current] [power_control] > msdc_debug\n", SD_TOOL_MSDC_HOST_MODE);
    seq_printf(m, "          get speed mode:    echo %x 1 [host_id]\n", SD_TOOL_MSDC_HOST_MODE);
    seq_printf(m, "            [speed_mode]       ff:N/A,  0:HS,      1:SDR12,   2:SDR25,   3:SDR:50,  4:SDR104,  5:DDR, 6:HS400\n");
    seq_printf(m, "            [driver_type]      ff:N/A,  0: type A, 1:type B,  2:type C,  3:type D\n");
    seq_printf(m, "            [max_current]      ff:N/A,  0:200mA,   1:400mA,   2:600mA,   3:800mA\n");
    seq_printf(m, "            [power_control]    ff:N/A,  0:disable, 1:enable\n");
    seq_printf(m, "\n   DMA viloation:         echo %x [host_id] [ops]> msdc_debug\n", SD_TOOL_DMA_STATUS);
    seq_printf(m, "          [ops]              0:get latest dma address,  1:start violation test\n");
    seq_printf(m, "\n   SET Slew Rate:         echo %x [host_id] [clk] [cmd] [dat] [rst] [ds]> msdc_debug\n", SD_TOOL_ENABLE_SLEW_RATE);
    seq_printf(m, "\n   TD/RD SEL:\n"); 
    seq_printf(m, "          set rdsel:             echo %x [host_id] 0 [value] > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_printf(m, "          set tdsel:             echo %x [host_id] 1 [value] > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_printf(m, "          get tdsel/rdsel:       echo %x [host_id] 2 > msdc_debug\n", SD_TOOL_SET_RDTDSEL);
    seq_printf(m, "            [value]              rdsel: 0x0<<4 ~ 0x3f<<4,    tdsel: 0x0~0xf\n");
    seq_printf(m, "\n   EMMC/SD RW test:       echo %x [host_id] [mode] > msdc_debug\n", MSDC_READ_WRITE);
    seq_printf(m, "          [mode]               mode 0:stop, 1:read, 2:write \n");
    seq_printf(m, "\n   Error tune debug:      echo %x [host_id] [cmd_id] [arg] [error_type] [count] > msdc_debug\n",MMC_ERROR_TUNE);  
    seq_printf(m, "            [cmd_id]           0: CMD0, 1: CMD1, 2: CMD2......\n");
    seq_printf(m, "            [arg]              for CMD6, arg means ext_csd index......\n");
    seq_printf(m, "            [error]            0: disable error tune debug,  1: cmd timeout, 2: cmd crc, 4: dat timeout, 8: dat crc, 16: acmd timeout, 32: acmd crc\n"); 
    seq_printf(m, "            [count]            error count\n");
#if MTK_MSDC_USE_EDC_EMMC_CACHE
    seq_printf(m, "\n   eMMC Cache Control:    echo %x [host_id] [action_id] > /proc/msdc_debug\n", MMC_EDC_EMMC_CACHE);
    seq_printf(m, "\n          [action_id]        Action_id 0:Disable cache 1:Enable cache 2:check cache status\n");
#endif
    seq_printf(m, "\n   NOTE: All input data is Hex number! \n");

    seq_printf(m, "\n======================================================\n\n");

    return 0;    
}

//========== driver proc interface ===========
static int msdc_debug_proc_show(struct seq_file *m, void *v)
{

    seq_printf(m, "\n=========================================\n");

    seq_printf(m, "Index<0> + Id + Zone\n");   
    seq_printf(m, "-> PWR<9> WRN<8> | FIO<7> OPS<6> FUN<5> CFG<4> | INT<3> RSP<2> CMD<1> DMA<0>\n");        
    seq_printf(m, "-> echo 0 3 0x3ff >msdc_bebug -> host[3] debug zone set to 0x3ff\n");              
    seq_printf(m, "-> MSDC[0] Zone: 0x%.8x\n", sd_debug_zone[0]);
    seq_printf(m, "-> MSDC[1] Zone: 0x%.8x\n", sd_debug_zone[1]);
    
    seq_printf(m, "-> MSDC[2] Zone: 0x%.8x\n", sd_debug_zone[2]);
    seq_printf(m, "-> MSDC[3] Zone: 0x%.8x\n", sd_debug_zone[3]);

    seq_printf(m, "Index<1> + ID:4|Mode:4 + DMA_SIZE\n");  
    seq_printf(m, "-> 0)PIO 1)DMA 2)SIZE\n");      
    seq_printf(m, "-> echo 1 22 0x200 >msdc_bebug -> host[2] size mode, dma when >= 512\n");        
    seq_printf(m, "-> MSDC[0] mode<%d> size<%d>\n", drv_mode[0], dma_size[0]);
    seq_printf(m, "-> MSDC[1] mode<%d> size<%d>\n", drv_mode[1], dma_size[1]);    
    
    seq_printf(m, "-> MSDC[2] mode<%d> size<%d>\n", drv_mode[2], dma_size[2]);
    seq_printf(m, "-> MSDC[3] mode<%d> size<%d>\n", drv_mode[3], dma_size[3]);

    seq_printf(m, "Index<3> + SDIO_PROFILE + TIME\n"); 
    seq_printf(m, "-> echo 3 1 0x1E >msdc_bebug -> enable sdio_profile, 30s\n"); 
    seq_printf(m, "-> SDIO_PROFILE<%d> TIME<%llu s>\n", sdio_pro_enable, sdio_pro_time); 
    seq_printf(m, "-> Clokc SRC selection Host[0]<%d>\n", msdc_clock_src[0]); 
    seq_printf(m, "-> Clokc SRC selection Host[1]<%d>\n", msdc_clock_src[1]); 
    seq_printf(m, "-> Clokc SRC selection Host[2]<%d>\n", msdc_clock_src[2]); 
    seq_printf(m, "-> Clokc SRC selection Host[3]<%d>\n", msdc_clock_src[3]); 
    seq_printf(m, "=========================================\n\n");
    
    return 0;
} 

/*
  *data: bit0~4:id, bit4~7: mode
*/
static int rwThread(void *data)
{
    int error, i = 0;
    ulong p = (ulong) data;
    int id = p & 0x3;
    int mode = (p >> 4) & 0x3;

    printk("[****SD_rwThread****]id=%d, mode=%d.\n", id, mode);

    while (read_write_state != 0) {
        if (read_write_state==1) {
            p=0x3;
        }else if (read_write_state==2) {
            p=0;
        }
      #ifdef MTK_EMMC_SUPPORT
        if (id==0) {
            if(mode == 1)
                error = emmc_multi_rw_compare_slave(id, 1, COMPARE_ADDRESS_MMC);
            else if(mode == 2)
                error = emmc_multi_rw_compare_slave(id, 0, COMPARE_ADDRESS_MMC);
                
            if(error) {
                printk("[%s]: failed data id0, error=%d\n", __func__, error);
                break;
            }
        } 
        else 
      #endif
        if (id < HOST_MAX_NUM) {
            if(mode == 1)
                error = sd_multi_rw_compare_slave(id, 1, COMPARE_ADDRESS_SD);
            else if(mode == 2)
                error = sd_multi_rw_compare_slave(id, 0, COMPARE_ADDRESS_SD);
                
            if(error) {
                printk("[%s]: failed data id1, error=%d\n", __func__, error);
                break;
            }
        }
        i++;
        if (i >= 500){
            printk("[***rwThread %s***] \n", read_write_state==1 ? "read":"write");
            i = 0;
        }
    }
    printk("[****SD_Debug****]rwThread exit.\n");
    return 0;
}

#if MTK_MSDC_USE_EDC_EMMC_CACHE
extern void msdc_get_cache_region_func(struct msdc_host   *host);
static int msdc_check_emmc_cache_status(struct msdc_host* host)
{
    struct mmc_card *card = host->mmc->card;
    msdc_get_cache_region_func(host);
    mmc_claim_host(host->mmc);
    if(card && !mmc_card_mmc(card))
    {
        printk("host:%d is not a eMMC card...\n",host->id);
        goto exit;
    }
    else
    {
        if(0 == host->mmc->card->ext_csd.cache_size)
        {
            printk("card don't support cache feature...\n");
            goto unsupport;
        }
        else
        {
            printk("card cache size:%dKB...\n",host->mmc->card->ext_csd.cache_size/8);
        }
    }
    if(host->mmc->card->ext_csd.cache_ctrl)
        printk("Current Cache status: Enable...\n");
    else
        printk("Current Cache status: Disable...\n");
    mmc_release_host(host->mmc);

    return (host->mmc->card->ext_csd.cache_ctrl);

exit:
    return -2;
unsupport:
    return -1;
}
static int msdc_enable_emmc_cache(struct msdc_host* host,int enable)
{
    u32 err;
    u8 c_ctrl;
    struct mmc_card *card = host->mmc->card;
    mmc_claim_host(host->mmc);
    if(card && !mmc_card_mmc(card))
    {
        printk("host:%d is not a eMMC card...\n",host->id);
        goto exit;
    }
    msdc_get_cache_region_func(host);

    err = msdc_check_emmc_cache_status(host);

    if(err < 0)
        goto exit;
    c_ctrl = host->mmc->card->ext_csd.cache_ctrl;
	
    if(c_ctrl && enable)
    {
        printk("cache has already been in enable status, don't need enable it...\n");
    }
    else if(c_ctrl && !enable)
    {
        err = mmc_cache_ctrl(host->mmc,enable);
        if (err) 
        {
            pr_warning("%s: Cache is supported, but failed to turn off (%d)\n",
                mmc_hostname(host->mmc), err);
        } 
        else 
        {
            printk("disable cache successfully...\n");
            host->mmc->caps2 &= ~MMC_CAP2_CACHE_CTRL;
        }
    }
    else if(!c_ctrl && enable)
    {
		host->mmc->caps2 |= MMC_CAP2_CACHE_CTRL;
		err = mmc_cache_ctrl(host->mmc,enable);
        if (err) 
        {
            pr_warning("%s: Cache is supported, but failed to turn on (%d)\n",
            mmc_hostname(host->mmc), err);
        } 
        else 
        {
            printk("enable cache successfully...\n");
        }
    }
    else if(!c_ctrl && !enable)
    {
        printk("cache has already been in disable status, don't need disable it...\n");
    }

    mmc_release_host(host->mmc);

    return 0;
exit:
    return (-1);
}
#endif

extern unsigned int msdc_dump_reg_level;
extern void msdc_dump_register(struct msdc_host *host);
extern void msdc_dump_info(u32 id);
static ssize_t msdc_debug_proc_write(struct file *file, const char *buf, size_t count, loff_t *data)
{
    int ret;
    int cmd, p1, p2, p3, p4, p5, p6, p7 = 0;   
    int id, zone;
    int mode, size;  
    int thread_num, compare_count, multi_address;
    void __iomem *base;
    unsigned int offset = 0;
    unsigned int reg_value;    
    HOST_CAPS_SPEED_MODE spd_mode = CAPS_SPEED_NULL;
    HOST_CAPS_DRIVE_TYPE drv_type = CAPS_DRIVE_NULL;
    HOST_CAPS_MAX_CURRENT current_limit = CAPS_CURRENT_NULL;
    HOST_CAPS_POWER_CONTROL pw_cr = CAPS_POWER_NULL;
    struct msdc_host *host = NULL;
    struct dma_addr *dma_address, *p_dma_address;
    int dma_status;
    struct task_struct *rw_thread = NULL;
    unsigned int msdc_dump_reg_level_orig;

    if (count == 0)return -1;
    if(count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;
    
    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%x %x %x %x %x %x %x %x", &cmd, &p1, &p2, &p3, &p4, &p5, &p6, &p7);

    if(cmd == SD_TOOL_ZONE) {
        id = p1; zone = p2; //zone &= 0x3ff;        
        printk("[****SD_Debug****]msdc host_id<%d> zone<0x%.8x>\n", id, zone);
        if(id >=0 && id<=HOST_MAX_NUM-1){
            sd_debug_zone[id] = zone;
        }
        else if(id == HOST_MAX_NUM){
            sd_debug_zone[0] = sd_debug_zone[1] = zone;
            sd_debug_zone[2] = zone;
            sd_debug_zone[3] = zone;
        }
        else{
            printk("[****SD_Debug****]msdc host_id error when set debug zone\n");
        }
    } else if (cmd == SD_TOOL_DMA_SIZE) {
        id = p2;  mode = p3; size = p4; 
        if(id >=0 && id<=HOST_MAX_NUM-1){
            if(p1==0){
              drv_mode[id] = mode;
              dma_size[id] = size; 
            }else{
              printk("-> MSDC[%d] mode<%d> size<%d>\n", id, drv_mode[id], dma_size[id]);
            }
        }
        else{
            printk("[****SD_Debug****]msdc host_id error when select mode\n");
        }    
    } else if (cmd == SD_TOOL_SDIO_PROFILE) {
        if (p1 == 1) { /* enable profile */
            if (gpt_enable == 0) {
                msdc_init_gpt();
                gpt_enable = 1;
            } 
            sdio_pro_enable = 1;
            if (p2 == 0) p2 = 1; if (p2 >= 30) p2 = 30;                 
            sdio_pro_time = p2 ; 
        }    else if (p1 == 0) {
            /* todo */
            sdio_pro_enable = 0;
        }            
    }else if (cmd == SD_TOOL_CLK_SRC_SELECT){
        id = p2;
        if(id>=0 && id < HOST_MAX_NUM){ 
            if(p1 == 0){
               if(p3 >= 0 && p3< CLK_SRC_MAX_NUM){
                  msdc_clock_src[id] = p3;
                  printk("[****SD_Debug****]msdc%d's clk source changed to %d\n", id, msdc_clock_src[id]);
                  printk("[****SD_Debug****]to enable the above settings, please suspend and resume the phone again\n");
               }else {
                  printk("[****SD_Debug****] invalide clock src id:%d, check /proc/msdc_help\n", p3);
               }
            }
            else if(p1 == 1){
                switch(id)
                {
                    case 0:
                        printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
                        break;
                    case 1:
                        printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
                        break;
                    case 2:
                        printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
                        break;
                    case 3:
                        printk("[****SD_Debug****]msdc%d's pll source is %d\n", id, msdc_clock_src[id]);
                        break;
                 }                
            }
        }
        else
            printk("[****SD_Debug****]msdc host_id error when select clock source\n");
    }else if (cmd == SD_TOOL_REG_ACCESS){
        id = p2;
        offset = (unsigned int)p3;
        
        if(id >= HOST_MAX_NUM || id < 0) {
            printk("[****SD_Debug****]msdc host_id error when modify msdc reg\n");
        } else {
            base = mtk_msdc_host[id]->base;

            if((offset == 0x18 || offset == 0x1C) && p1 != 4) {
                printk("[****SD_Debug****]Err: Accessing TXDATA and RXDATA is forbidden\n");
                return count;
            }
#ifndef FPGA_PLATFORM
            enable_clock(msdc_cg_clk_id[id], "SD"); 
#endif                
            if(p1 == 0){
                reg_value = p4;
                if(offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
                    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
                } else {
                    printk("[****SD_Debug****][MSDC Reg]Original:0x%p+0x%x (0x%x)\n",base,offset,sdr_read32(base+offset));
                    sdr_write32(base+offset,reg_value );
                    printk("[****SD_Debug****][MSDC Reg]Modified:0x%p+0x%x (0x%x)\n",base,offset,sdr_read32(base+offset));
                }                
            }
            else if(p1 == 1)
                if(offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
                    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
                } else {
                    printk("[****SD_Debug****][MSDC Reg]Reg:0x%p+0x%x (0x%x)\n",base,offset,sdr_read32(base+offset));
                }
            else if(p1 == 2){
                if(offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
                    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
                } else {
                    msdc_set_field(base+offset,p4,p5,p6);
                }
            }
            else if(p1 == 3) {
                if(offset == 0xE0 || offset == 0xE4 || offset == 0xE8) {
                    printk("[****SD_Debug****]Err: Bypass PAD_CTL\n");
                } else {
                    msdc_get_field(base+offset,p4,p5,p6);
                }
            } else if(p1 == 4) {
                msdc_dump_reg_level_orig=msdc_dump_reg_level;
                msdc_dump_reg_level=1;
                msdc_dump_register(mtk_msdc_host[id]);
                msdc_dump_reg_level=msdc_dump_reg_level_orig;
            } else if(p1 == 5) {
                msdc_dump_reg_level_orig=msdc_dump_reg_level;
                msdc_dump_reg_level=2;
                msdc_dump_info(host->id);
                msdc_dump_reg_level=msdc_dump_reg_level_orig;
            }
#ifndef FPGA_PLATFORM
            disable_clock(msdc_cg_clk_id[id], "SD");
#endif
        }
            
    } 
    else if(cmd == SD_TOOL_SET_DRIVING){
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]msdc host_id error when modify msdc driving\n");
        else{
            host = mtk_msdc_host[id];
            if((unsigned char)p2 > 7 || (unsigned char)p3 > 7 || (unsigned char)p4 > 7 ||(unsigned char)p5 > 7 ||(unsigned char)p6 > 7)
                printk("[****SD_Debug****]Some drving value was not right(correct:0~7)\n");
            else{
 #ifndef FPGA_PLATFORM                            
                if(p7 == 0x33){
                        host->hw->clk_drv = (unsigned char)p2;
                        host->hw->cmd_drv = (unsigned char)p3;
                        host->hw->dat_drv = (unsigned char)p4;
                        host->hw->rst_drv = (unsigned char)p5;
                        host->hw->ds_drv = (unsigned char)p6; 
                        msdc_set_driving(host,host->hw,0);
                }
                else if(p7 == 0x18){
                    host->hw->clk_drv_sd_18 = (unsigned char)p2;
                    host->hw->cmd_drv_sd_18 = (unsigned char)p3;
                    host->hw->dat_drv_sd_18 = (unsigned char)p4;
                    msdc_set_driving(host,host->hw,1);
                }
#endif
                printk("[****SD_Debug****]clk_drv=%d, cmd_drv=%d, dat_drv=%d, rst_drv=%d, ds_drv=%d\n", p2, p3, p4, p5, p6);
            }
        }
    }
    else if(cmd == SD_TOOL_ENABLE_SLEW_RATE){
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]invalid host id: %d\n", id);
        else{  
            host = mtk_msdc_host[id];          
            if((unsigned char)p2 > 1 || (unsigned char)p3 > 1 ||(unsigned char)p4 > 1 || (unsigned char)p5 > 1 ||(unsigned char)p6 > 1)
                printk("[****SD_Debug****]Some sr value was not right(correct:0(disable),1(enable))\n");
            else{
#ifndef FPGA_PLATFORM
                msdc_set_sr(host,p2,p3,p4,p5,p6);
#endif
               printk("[****SD_Debug****]msdc%d, clk_sr=%d, cmd_sr=%d, dat_sr=%d, rst_sr=%d, ds_sr=%d\n", id, p2, p3, p4, p5, p6);
           }
        }
    }
    else if(cmd == SD_TOOL_SET_RDTDSEL){
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]msdc host_id error when modify msdc sr\n");
        else{
            host = mtk_msdc_host[id];
            if((p2 < 0) || (p2 > 2))
                printk("[****SD_Debug****]invalid option ( set rd:0, set td:1, get td/rd: 2)\n");
            else if((p2 == 0 && (unsigned char)p3 > 0x3F)|| (p2 == 1 && (unsigned char)p3 > 0xF))
                printk("[****SD_Debug****]Some rd/td value was not right(rd mask:(0x3F << 4),td mask:(0xF << 0))\n");
            else{
                #ifndef FPGA_PLATFORM
                if (p2 == 0){ /* set rd:0 */
                    msdc_set_rdtdsel_dbg(host, 1, p3);; 
                    printk("[****SD_Debug****]msdc%d, set rd=%d\n", id, p3);
                } else if (p2 == 1){ /* set td:1 */
                    msdc_set_rdtdsel_dbg(host, 0, p3);
                    printk("[****SD_Debug****]msdc%d, set td=%d\n", id, p3);
                } else if (p2 == 2){ /* get td/rd:2 */
                    msdc_get_rdtdsel_dbg(host, 1, &p3); /* get rd */
                    msdc_get_rdtdsel_dbg(host, 0, &p4); /* get td */
                    printk("[****SD_Debug****]msdc%d, rd : 0x%x, td : 0x%x \n", id, p3, p4);                   
                }
              #endif
            }
        }
    }
    else if(cmd == SD_TOOL_ENABLE_SMT){
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]msdc host_id error when enable/disable msdc smt\n");
        else{
            host = mtk_msdc_host[id];
#ifndef FPGA_PLATFORM
             msdc_set_smt(host, p2);
#endif
             printk("[****SD_Debug****]smt=%d\n",p2);
        }    
    }
    else if(cmd == SD_TOOL_DESENSE){
        if(p1 == 0){
            reg_value = p2;
            //printk("[****SD_Debug****][De-Sense Reg]Original:0x%x(0x%x)\n",MSDC_DESENSE_REG,sdr_read32(MSDC_DESENSE_REG));
            //sdr_write32(MSDC_DESENSE_REG,reg_value);
            //printk("[****SD_Debug****][De-Sense Reg]Modified:0x%x(0x%x)\n",MSDC_DESENSE_REG,sdr_read32(MSDC_DESENSE_REG));
        }
        else if(p1 == 1){
            //printk("[****SD_Debug****][De-Sense Reg]Reg:0x%x(0x%x)\n",MSDC_DESENSE_REG,sdr_read32(MSDC_DESENSE_REG));
        }else if(p1 == 2){
            //msdc_set_field(MSDC_DESENSE_REG,p2,p3,p4);
        }else if(p1 == 3){
            //msdc_get_field(MSDC_DESENSE_REG,p2,p3,p4);
        }
    }else if(cmd == RW_BIT_BY_BIT_COMPARE){
        id = p1;
        compare_count = p2;
        if(id >= HOST_MAX_NUM || id < 0)
        {
             printk("[****SD_Debug****]: bad host id: %d\n", id);
             return count;
        }
        if(compare_count < 0)
        {
             printk("[****SD_Debug****]: bad compare count: %d\n", compare_count);
             return count;
        }
               
        if(id == 0) //for msdc0
        {
#ifdef MTK_EMMC_SUPPORT
           emmc_multi_rw_compare(0, COMPARE_ADDRESS_MMC, compare_count);//test the address 0 of eMMC card, since there a little memory.
#else
           sd_multi_rw_compare(0, COMPARE_ADDRESS_SD, compare_count); //test a larger address of SD card
#endif
        }else {
             sd_multi_rw_compare(id, COMPARE_ADDRESS_SD, compare_count);
        }
    }
    else if (cmd == MSDC_READ_WRITE){
        id = p1;
        mode = p2;  //0:stop, 1:read, 2:write
        if(id >= HOST_MAX_NUM || id < 0 || mode>2 || mode<0)
        {
             printk("[****SD_Debug****]: bad host id: %d, mode: %d\n", id, mode);
             return count;
        }
        if (mode == read_write_state) {
            printk("[****SD_Debug****]: same operation mode=%d.\n", read_write_state);
            return count;
        }
        if (mode == 1 && read_write_state==2) {
            printk("[****SD_Debug****]: cannot read in wirte state, please stop first.\n");
            return count;
        }
        if (mode == 2 && read_write_state==1) {
            printk("[****SD_Debug****]: cannot write in read state, please stop first.\n");
            return count;
        }
        read_write_state = mode;
        
        printk("[****SD_Debug****]: host id: %d, mode: %d.\n", id, mode);
        if (mode == 0) {
            if (rw_thread) {
                kthread_stop(rw_thread);
                printk("[****SD_Debug****]: stop read/write thread.\n");
            }
        } else {
            ulong data = ((id & 0x3) | ((mode & 0x3) << 4));
            printk("[****SD_Debug****]: start read/write thread.\n");
            rw_thread = kthread_create(rwThread, (void *) data, "msdc_rw_thread");
            wake_up_process(rw_thread);
        }
    }
    else if (cmd == SMP_TEST_ON_ONE_HOST){
        id = p1;
        thread_num = p2;
        compare_count = p3;
        multi_address = p4;
        smp_test_on_one_host(thread_num, id, compare_count, multi_address); 
    }
    else if (cmd == SMP_TEST_ON_ALL_HOST){
        thread_num = p1;
        compare_count = p2;
        multi_address = p3;
        smp_test_on_all_host(thread_num, compare_count, multi_address); 
    }
    else if(cmd == SD_TOOL_MSDC_HOST_MODE){
        id = p2;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]msdc host_id error when modify msdc host mode\n");
        else {
            if(p1 == 0){
                if(p3 <= UHS_DDR50 && p3 >= SDHC_HIGHSPEED)
                    spd_mode = p3;
                if(p4 <= DRIVER_TYPE_D && p4 >= DRIVER_TYPE_A)
                    drv_type = p4;
                if(p5 <= MAX_CURRENT_800 && p5 >= MAX_CURRENT_200 )
                    current_limit = p5;
                if(p6 <= SDXC_POWER_CONTROL && p6 >= SDXC_NO_POWER_CONTROL)
                    pw_cr = p6;
                if(spd_mode != CAPS_SPEED_NULL){
                    switch(spd_mode){
                        case SDHC_HIGHSPEED: 
                            msdc_host_mode[id] |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
                            msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR12)&(~MMC_CAP_UHS_SDR25)&(~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
                            printk("[****SD_Debug****]host will support Highspeed\n");
                            break;
                        case UHS_SDR12: 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12;
                            msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR25)&(~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
                            printk("[****SD_Debug****]host will support UHS-SDR12\n");
                            break;
                        case UHS_SDR25: 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25;
                            msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR50)&(~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
                            printk("[****SD_Debug****]host will support UHS-SDR25\n");
                            break;
                        case UHS_SDR50  : 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50;
                            msdc_host_mode[id] &= (~MMC_CAP_UHS_DDR50)&(~MMC_CAP_1_8V_DDR)&(~MMC_CAP_UHS_SDR104);
#ifdef CONFIG_EMMC_50_FEATURE
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
                            printk("[****SD_Debug****]host will support UHS-SDR50\n");
                            break;
                        case UHS_SDR104 : 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50|MMC_CAP_UHS_SDR104;
                            msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR;
#ifdef CONFIG_EMMC_50_FEATURE
                            msdc_host_mode2[id] &= (~MMC_CAP2_HS400_1_8V_DDR);
#endif
                            printk("[****SD_Debug****]host will support UHS-SDR104\n");
                            break;
                        case UHS_DDR50  : 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_DDR50|MMC_CAP_1_8V_DDR;
                            printk("[****SD_Debug****]host will support UHS-DDR50\n");
                            break;
#ifdef CONFIG_EMMC_50_FEATURE
                        case EMMC_HS400: 
                            msdc_host_mode[id] |= MMC_CAP_UHS_SDR12|MMC_CAP_UHS_SDR25|MMC_CAP_UHS_SDR50|MMC_CAP_UHS_DDR50|MMC_CAP_1_8V_DDR|MMC_CAP_UHS_SDR104;
                            msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_1_8V_DDR;
                            printk("[****SD_Debug****]host will support EMMC_HS400\n");
                            break;
#endif
                        default             : 
                            printk("[****SD_Debug****]invalid sd30_mode:%d\n", spd_mode); 
                            break;
                    }
                }
            if(drv_type != CAPS_DRIVE_NULL){
                switch(drv_type){
                    case DRIVER_TYPE_A : 
                        msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_A;
                        msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_C)&(~MMC_CAP_DRIVER_TYPE_D);
                        printk("[****SD_Debug****]host will support DRIVING TYPE A\n");
                        break;
                    case DRIVER_TYPE_B : 
                        msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_C)&(~MMC_CAP_DRIVER_TYPE_D);
                        printk("[****SD_Debug****]host will support DRIVING TYPE B\n");
                        break;
                    case DRIVER_TYPE_C : 
                        msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_C;
                        msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_D);
                        printk("[****SD_Debug****]host will support DRIVING TYPE C\n");
                        break;
                    case DRIVER_TYPE_D : 
                        msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_D;
                        msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A)&(~MMC_CAP_DRIVER_TYPE_C);
                        printk("[****SD_Debug****]host will support DRIVING TYPE D\n");
                        break;
                    default: 
                        printk("[****SD_Debug****]invalid drv_type:%d\n", drv_type); 
                        break;
                }
            }
            if(current_limit != CAPS_CURRENT_NULL){
#if 0 /* cause MMC_CAP_MAX??? and MMC_CAP_SET??? removed from linux3.6 */
                switch(current_limit){
                    case MAX_CURRENT_200 : 
                        msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200;
                        msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_400)&(~MMC_CAP_MAX_CURRENT_600)&(~MMC_CAP_MAX_CURRENT_800);
                        printk("[****SD_Debug****]host will support MAX_CURRENT_200\n");
                        break;
                    case MAX_CURRENT_400 : 
                        msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400;
                        msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_600)&(~MMC_CAP_MAX_CURRENT_800);
                        printk("[****SD_Debug****]host will support MAX_CURRENT_400\n");
                        break;
                    case MAX_CURRENT_600 : 
                        msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400|MMC_CAP_MAX_CURRENT_600;
                        msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_800);
                        printk("[****SD_Debug****]host will support MAX_CURRENT_600\n");
                        break;
                    case MAX_CURRENT_800 : 
                        msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400|MMC_CAP_MAX_CURRENT_600|MMC_CAP_MAX_CURRENT_800;
                        printk("[****SD_Debug****]host will support MAX_CURRENT_800\n");
                        break;
                    default :
                        printk("[****SD_Debug****]invalid current_limit:%d\n", current_limit); 
                        break;
                }
#endif
            }
            if(pw_cr != CAPS_POWER_NULL)
#if 0
                switch(pw_cr){
                    case SDXC_NO_POWER_CONTROL: 
                        msdc_host_mode[id] &= (~MMC_CAP_SET_XPC_330) & (~MMC_CAP_SET_XPC_300) & (~MMC_CAP_SET_XPC_180);
                        printk("[****SD_Debug****]host will not support SDXC power control\n");
                        break;
                    case SDXC_POWER_CONTROL: 
                        msdc_host_mode[id] |= MMC_CAP_SET_XPC_330 | MMC_CAP_SET_XPC_300 | MMC_CAP_SET_XPC_180;
                        printk("[****SD_Debug****]host will support SDXC power control\n");
                        break;
                    default: 
                        printk("[****SD_Debug****]invalid pw_cr:%d\n", pw_cr); 
                        break;
                }
#endif
                printk("[****SD_Debug****]to enable the above settings, please suspend and resume the phone again\n");
            }else {
                printk("[****SD_Debug****]msdc[%d] supports: \n", id); 
                {
                    printk("[****SD_Debug****]      speed mode: ");
                    if((msdc_host_mode[id] & MMC_CAP_MMC_HIGHSPEED) || (msdc_host_mode[id] & MMC_CAP_SD_HIGHSPEED)) printk("HS, ");
                    if(msdc_host_mode[id] & MMC_CAP_UHS_SDR12) printk("SDR12, ");
                    if(msdc_host_mode[id] & MMC_CAP_UHS_SDR25) printk("SDR25, ");
                    if(msdc_host_mode[id] & MMC_CAP_UHS_SDR50) printk("SDR50, ");
                    if(msdc_host_mode[id] & MMC_CAP_UHS_SDR104) printk("SDR104, ");
                    if(msdc_host_mode[id] & MMC_CAP_UHS_DDR50) printk("DDR50 ");
                    if(!(msdc_host_mode[id] & (MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED | MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104 | MMC_CAP_UHS_DDR50))) printk("N/A");
                    printk("\n");
                }
                {            
                    printk("[****SD_Debug****]      driver_type: ");
                    if(msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_A) printk("A, ");
                    printk("B, ");
                    if(msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_C) printk("C, ");
                    if(msdc_host_mode[id] & MMC_CAP_DRIVER_TYPE_D) printk("D, ");
                    printk("\n");
                }
                {
#if 0                 
                    printk("[****SD_Debug****]      current limit: ");
                    if(msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_200) printk("200mA, ");
                    if(msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_400) printk("400mA, ");
                    if(msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_600) printk("600mA, ");
                    if(msdc_host_mode[id] & MMC_CAP_MAX_CURRENT_800) printk("800mA, ");
                    if(!(msdc_host_mode[id] & (MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400 | MMC_CAP_MAX_CURRENT_600 | MMC_CAP_MAX_CURRENT_800))) printk("N/A");
                    printk("\n");
#endif
                }
                {
#if 0                
                    printk("[****SD_Debug****]      power control: ");
                    if(msdc_host_mode[id] & MMC_CAP_SET_XPC_330) printk("3.3v ");
                    if(msdc_host_mode[id] & MMC_CAP_SET_XPC_300) printk("3v ");
                    if(msdc_host_mode[id] & MMC_CAP_SET_XPC_180) printk("1.8v ");
                    if(!(msdc_host_mode[id] & (MMC_CAP_SET_XPC_330 | MMC_CAP_SET_XPC_300 | MMC_CAP_SET_XPC_180))) printk("N/A");
                    printk("\n");
#endif
                }
            }
        }
    }
    else if(cmd == SD_TOOL_DMA_STATUS)
    {
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****SD_Debug****]invalid host id: %d\n", id);
        else{  
            if(p2 == 0){
                dma_status = msdc_get_dma_status(id);
                printk(">>>> msdc%d: dma_status=%d, ", id, dma_status);
                if(dma_status == 0){
                    printk("DMA mode is disabled Now\n");
                }else if (dma_status == 1){
                    printk("Write data from SD to DRAM within DMA mode\n");
                }else if (dma_status == 2){
                    printk("Write data from DRAM to SD within DMA mode\n");
                }else if (dma_status == -1){
                    printk("No data transaction or the device is not present until now\n");
                }
			
                if(dma_status > 0)
                {
                    dma_address = msdc_get_dma_address(id); 
                    if(dma_address){
                        printk(">>>> msdc%d: \n", id);
                        p_dma_address = dma_address;
                        while (p_dma_address){
                            printk(">>>>     addr=0x%x, size=%d\n", p_dma_address->start_address, p_dma_address->size);
                            if(p_dma_address->end)
                                break;
                            p_dma_address = p_dma_address->next;
                        }
                    }else {
                        printk(">>>> msdc%d: BD count=0\n", id);
                    }
                }
            }else if(p2 == 1){
                printk(">>>> msdc%d: start dma violation test\n", id);
                g_dma_debug[id] = 1;
                sd_multi_rw_compare(id, COMPARE_ADDRESS_SD, 3);
            }
        }
    }
    else if (cmd == MMC_REGISTER_READ){
        printk("p1 = 0x%x\n", p1);

        /* get the regster value, must know some register can't be read */
        printk("regiser: 0x%x = 0x%x\n", p1, sdr_read32((ulong) p1));
    }
#ifdef MTK_IO_PERFORMANCE_DEBUG
    else if (cmd == MMC_PERF_DEBUG){
        /* 1 enable; 0 disable */
        g_mtk_mmc_perf_dbg = p1;
        g_mtk_mmc_dbg_range = p2;

        if (2 == g_mtk_mmc_dbg_range){
            g_dbg_range_start = p3;
            g_dbg_range_end   = p3 + p4;
            g_check_read_write = p5;
        }
        printk("g_mtk_mmc_perf_dbg = 0x%x, g_mtk_mmc_dbg_range = 0x%x, start = 0x%x, end = 0x%x\n", g_mtk_mmc_perf_dbg, g_mtk_mmc_dbg_range, g_dbg_range_start, g_dbg_range_end);
    }  else if (cmd == MMC_PERF_DEBUG_PRINT){
        int i, j, k, num = 0;

        if (p1 == 0){
            g_mtk_mmc_clear = 0;
            return count;
        }
        printk(KERN_ERR "msdc g_dbg_req_count<%d>\n",g_dbg_req_count);
        for (i = 1; i <= g_dbg_req_count; i++){
            printk("anslysis: %s 0x%x %d block, PGh %d\n", (g_check_read_write == 18 ? "read" : "write"),(unsigned int)g_mmcqd_buf[i][298], (unsigned int)g_mmcqd_buf[i][299], (unsigned int)(g_mmcqd_buf[i][297] * 2));
            if(g_check_read_write == 18){
                for (j = 1; j <= g_mmcqd_buf[i][296] * 2; j++){
                    printk("page %d:\n", num+1);
                    for (k = 0; k < 5; k++){
                        printk("%d %llu\n", k, g_req_buf[num][k]);
                    }
                    num += 1;
                }
            }
            printk(KERN_ERR "-------------------------------------------\n");
            for (j = 0; j < sizeof(g_time_mark)/sizeof(char*); j++){
                printk(KERN_ERR "%d. %llu %s\n",j,g_mmcqd_buf[i][j],g_time_mark[j]);
            }
            printk(KERN_ERR "===========================================\n");
        }
        if(g_check_read_write == 25){
            printk(KERN_ERR "msdc g_dbg_write_count<%d>\n",g_dbg_write_count);
            for(i = 1;i<=g_dbg_write_count;i++){
                printk(KERN_ERR "********************************************\n");
                printk(KERN_ERR "write count: %llu\n",g_req_write_count[i]);
                for (j = 0; j < sizeof(g_time_mark_vfs_write)/sizeof(char*); j++)
                    printk(KERN_ERR "%d. %llu %s\n",j,g_req_write_buf[i][j],g_time_mark_vfs_write[j]);
            }
            printk(KERN_ERR "********************************************\n");
        }
        g_mtk_mmc_clear = 0;
    }
#endif

#ifdef MTK_MMC_PERFORMANCE_TEST
    else if (cmd == MMC_PERF_TEST){
        /* 1 enable; 0 disable */
        g_mtk_mmc_perf_test = p1;
    }
#endif

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
    else if(cmd == MMC_ERROR_TUNE) {
        g_err_tune_dbg_host = p1; 
        g_err_tune_dbg_cmd = p2; 
        g_err_tune_dbg_arg = p3; 
        g_err_tune_dbg_error = p4; 
        g_err_tune_dbg_count = p5; 
        if(g_err_tune_dbg_count && (g_err_tune_dbg_error != MTK_MSDC_ERROR_NONE)){
            printk(KERN_ERR "==========================MSDC error debug start ============================\n"); 
            printk(KERN_ERR "host:%d, cmd=%d, arg=%d, error=%d, count=%d\n", 
                    g_err_tune_dbg_host, g_err_tune_dbg_cmd, g_err_tune_dbg_arg, g_err_tune_dbg_error, g_err_tune_dbg_count); 
        }else {
            g_err_tune_dbg_host = 0; 
            g_err_tune_dbg_cmd = 0; 
            g_err_tune_dbg_arg = 0; 
            g_err_tune_dbg_error = MTK_MSDC_ERROR_NONE; 
            g_err_tune_dbg_count = 0; 
            printk(KERN_ERR "host:%d, cmd=%d, arg=%d, error=%d, count=%d\n", 
                    g_err_tune_dbg_host, g_err_tune_dbg_cmd, g_err_tune_dbg_arg, g_err_tune_dbg_error, g_err_tune_dbg_count); 
            printk(KERN_ERR "==========================MSDC error debug end ============================\n"); 
        }
    }
#endif

#if MTK_MSDC_USE_EDC_EMMC_CACHE
    else if(cmd == MMC_EDC_EMMC_CACHE)
    {
        printk(KERN_ERR "==========================MSDC Cache Feature Test ==============================\n"); 
        id = p1;
        if(id >= HOST_MAX_NUM || id < 0)
            printk("[****MSDC Cache Feature Test****]invalid host id: %d\n", id);
        else
        {
            host = mtk_msdc_host[id];
            switch(p2)
            {
                case 0:
                    msdc_enable_emmc_cache(host,0);
                    break;
                case 1:
                    msdc_enable_emmc_cache(host,1);
                    break;
                case 2:
                    msdc_check_emmc_cache_status(host);
                    break;
                default:
                    printk(KERN_ERR "ERROR:3rd parameter is wrong, please see the msdc_help\n");
                    break;
            }
        }
    }	
#endif

    return count;
}
static int msdc_tune_flag_proc_read_show(struct seq_file *m, void *data)
{
    seq_printf(m, "0x%X\n", sdio_tune_flag);
    return 0;
}

static int msdc_debug_proc_read_FT_show(struct seq_file *m, void *data)
{
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
    int msdc_id =0;
    void __iomem *base;
    unsigned char  cmd_edge;
    unsigned char  data_edge;
    unsigned char  clk_drv1 = 0,clk_drv2 = 0,cmd_drv1 = 0,cmd_drv2 = 0,dat_drv1 = 0,dat_drv2 = 0;
    u32 cur_rxdly0;
    u8 u8_dat0, u8_dat1, u8_dat2, u8_dat3;
    u8 u8_wdat, u8_cmddat;
    u8 u8_DDLSEL;

    if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
    {
#if defined(CFG_DEV_MSDC0)
        base = mtk_msdc_host[0]->base;
        msdc_id = 0;
#endif
    }
    else if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
    {
#if defined(CFG_DEV_MSDC1)
        base = mtk_msdc_host[1]->base;
        msdc_id = 1;
#endif
    }
    else if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
    {
#if defined(CFG_DEV_MSDC2)
        base = mtk_msdc_host[2]->base;
        msdc_id = 2;
#endif
        }
    else if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
    {
#if defined(CFG_DEV_MSDC3)
        base = mtk_msdc_host[3]->base;     
        msdc_id = 3;
#endif
    }

#ifndef FPGA_PLATFORM
    enable_clock(msdc_cg_clk_id[msdc_id], "SD");
#endif    
    sdr_get_field((base+0x04), MSDC_IOCON_RSPL, cmd_edge);
    sdr_get_field((base+0x04), MSDC_IOCON_R_D_SMPL, data_edge);
/*
    sdr_get_field((base + 0xe0), MSDC_PAD_CTL0_CLKDRVN, clk_drv1);
    sdr_get_field((base + 0xe0), MSDC_PAD_CTL0_CLKDRVP, clk_drv2);
    
    sdr_get_field((base + 0xe4), MSDC_PAD_CTL1_CMDDRVN, cmd_drv1);
    sdr_get_field((base + 0xe4), MSDC_PAD_CTL1_CMDDRVP, cmd_drv2);

    sdr_get_field((base + 0xe8), MSDC_PAD_CTL2_DATDRVN, dat_drv1);
    sdr_get_field((base + 0xe8), MSDC_PAD_CTL2_DATDRVP, dat_drv2);*/
    
    sdr_get_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, u8_DDLSEL);
    cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
    if (sdr_read32(MSDC_ECO_VER) >= 4) 
    {
          u8_dat0 = (cur_rxdly0 >> 24) & 0x1F;
          u8_dat1 = (cur_rxdly0 >> 16) & 0x1F;
          u8_dat2 = (cur_rxdly0 >>  8) & 0x1F;
          u8_dat3 = (cur_rxdly0 >>  0) & 0x1F;
    }
    else 
    {
          u8_dat0 = (cur_rxdly0 >>  0) & 0x1F;
          u8_dat1 = (cur_rxdly0 >>  8) & 0x1F;
          u8_dat2 = (cur_rxdly0 >> 16) & 0x1F;
          u8_dat3 = (cur_rxdly0 >> 24) & 0x1F;
    }
    
    sdr_get_field((base + 0xec), MSDC_PAD_TUNE_DATWRDLY, u8_wdat);        
    sdr_get_field((base + 0xec), MSDC_PAD_TUNE_CMDRRDLY, u8_cmddat);

    seq_printf(m, "\n=========================================\n");

#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
    seq_printf(m, "(1) WCN SDIO SLOT is at msdc<%d>\n",CONFIG_MTK_WCN_CMB_SDIO_SLOT); 
#endif
    
    seq_printf(m, "-----------------------------------------\n"); 
    seq_printf(m, "(2) clk settings \n"); 
    seq_printf(m, "mt6589 only using internal clock\n"); 

    seq_printf(m, "-----------------------------------------\n"); 
    seq_printf(m, "(3) settings of driving current \n"); 
    if ((clk_drv1==clk_drv2) && (cmd_drv1==cmd_drv2) && (dat_drv1==dat_drv2) && (clk_drv2==cmd_drv1) && (cmd_drv2==dat_drv1))
    seq_printf(m, "driving current is <%d>\n", clk_drv1); 
    else
    {
        seq_printf(m, "clk_drv1<%d>  clk_drv2<%d>  cmd_drv1<%d>  cmd_drv2<%d>  dat_drv1<%d>  dat_drv2<%d>\n", clk_drv1, clk_drv2, cmd_drv1, cmd_drv2, dat_drv1, dat_drv2);  
    }

    seq_printf(m, "-----------------------------------------\n"); 
    seq_printf(m, "(4) edge settings \n"); 
    if (cmd_edge)
        seq_printf(m, "cmd_edge is falling \n");    
    else
        seq_printf(m, "cmd_edge is rising \n");  
    if (data_edge)
        seq_printf(m, "data_edge is falling \n");  
    else
        seq_printf(m, "data_edge is rising \n");    

    seq_printf(m, "-----------------------------------------\n"); 
    seq_printf(m, "(5) data delay info\n"); 
    seq_printf(m, "Read (MSDC_DAT_RDDLY0) is <0x%x> and (MSDC_IOCON_DDLSEL) is <0x%x>\n", cur_rxdly0, u8_DDLSEL); 
    seq_printf(m, "data0<0x%x>  data1<0x%x>  data2<0x%x>  data3<0x%x>\n", u8_dat0, u8_dat1, u8_dat2, u8_dat3); 
    seq_printf(m, "Write is <0x%x>\n", u8_wdat); 
    seq_printf(m, "Cmd is <0x%x>\n", u8_cmddat); 
    seq_printf(m, "=========================================\n\n");
    
    return 0; 
    
#else
    seq_printf(m, "\n=========================================\n");
    seq_printf(m, "There is no WCN SDIO SLOT. \n"); 
    seq_printf(m, "=========================================\n\n");
    return 0;
#endif
}

static ssize_t msdc_debug_proc_write_FT(struct file *file, const char __user* buf, size_t count, loff_t *data)
{
    int ret;

    int i_case=0, i_par1=-1, i_par2=-1, i_clk=0, i_driving=0, i_edge=0, i_data=0, i_delay=0;   
    u32 cur_rxdly0;
    u8 u8_dat0, u8_dat1, u8_dat2, u8_dat3;
    void __iomem *base;
    if (count == 0)return -1;
    if(count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;
    
    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%d %d %d ", &i_case, &i_par1, &i_par2);

    if (i_par2 == -1)
        return -1;

    printk("i_case=%d i_par1=%d i_par2=%d\n", i_case, i_par1, i_par2);
        
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
#if defined(CFG_DEV_MSDC0)
    if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
        base = mtk_msdc_host[0]->base;
#endif
#if defined(CFG_DEV_MSDC1)
    if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
        base = mtk_msdc_host[1]->base;
#endif
#if defined(CFG_DEV_MSDC2)
    if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
        base = mtk_msdc_host[2]->base;
#endif
#if defined(CFG_DEV_MSDC3)
    if(CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
        base = mtk_msdc_host[3]->base;
#endif     
#else
    return -1;
#endif

    if (i_case==1)  //set clk
    {
        if (!((i_par1==0) || (i_par1==1)))
            return -1;
        i_clk = i_par1;

        //sdr_set_field(MSDC_PATCH_BIT0, MSDC_PATCH_BIT_CKGEN_CK, i_clk);
        
        printk("i_clk=%d \n", i_clk);
    }
    else if (i_case==2)//set driving current
    {
        if (!((i_par1>=0) && (i_par1<=7)))
            return -1;
        i_driving = i_par1;
/*
        sdr_set_field((base + 0xe0), MSDC_PAD_CTL0_CLKDRVN, i_driving);
        sdr_set_field((base + 0xe0), MSDC_PAD_CTL0_CLKDRVP, i_driving);
    
        sdr_set_field((base + 0xe4), MSDC_PAD_CTL1_CMDDRVN, i_driving);
        sdr_set_field((base + 0xe4), MSDC_PAD_CTL1_CMDDRVP, i_driving);

        sdr_set_field((base + 0xe8), MSDC_PAD_CTL2_DATDRVN, i_driving);
        sdr_set_field((base + 0xe8), MSDC_PAD_CTL2_DATDRVP, i_driving);*/
        
        printk("i_driving=%d \n", i_driving);
    }
    else if (i_case==3)//set data delay
    {    
        if (!((i_par1>=0) && (i_par1<=3)))
            return -1;
        if (!((i_par2>=0) && (i_par2<=31)))
            return -1;
        i_data = i_par1;
        i_delay = i_par2;

        cur_rxdly0 = sdr_read32(MSDC_DAT_RDDLY0);
        if (sdr_read32(MSDC_ECO_VER) >= 4) 
        {
            u8_dat0 = (cur_rxdly0 >> 24) & 0x1F;
            u8_dat1 = (cur_rxdly0 >> 16) & 0x1F;
            u8_dat2 = (cur_rxdly0 >>  8) & 0x1F;
            u8_dat3 = (cur_rxdly0 >>  0) & 0x1F;
        }
        else 
        {
            u8_dat0 = (cur_rxdly0 >>  0) & 0x1F;
            u8_dat1 = (cur_rxdly0 >>  8) & 0x1F;
            u8_dat2 = (cur_rxdly0 >> 16) & 0x1F;
            u8_dat3 = (cur_rxdly0 >> 24) & 0x1F;
        }

        if (i_data==0)
            u8_dat0 = i_delay;
        else if (i_data==1)
            u8_dat1 = i_delay;
        else if (i_data==2)
            u8_dat2 = i_delay;
        else if (i_data==3)
            u8_dat3 = i_delay;
        else if (i_data==4)  //write data
        {
            sdr_set_field((base + 0xec), MSDC_PAD_TUNE_DATWRDLY, i_delay);
        }        
        else if (i_data==5)  //cmd data
        {
            sdr_set_field((base + 0xec), MSDC_PAD_TUNE_CMDRRDLY, i_delay);
        }
        else
            return -1;
        
        if (sdr_read32(MSDC_ECO_VER) >= 4) 
        {
            cur_rxdly0 = ((u8_dat0 & 0x1F) << 24) | ((u8_dat1 & 0x1F) << 16) |
                            ((u8_dat2 & 0x1F) << 8)  | ((u8_dat3 & 0x1F) << 0);
        }
        else
        {
            cur_rxdly0 = ((u8_dat3 & 0x1F) << 24) | ((u8_dat2 & 0x1F) << 16) |
                           ((u8_dat1 & 0x1F) << 8)  | ((u8_dat0 & 0x1F) << 0);
        }
        sdr_set_field(MSDC_IOCON, MSDC_IOCON_DDLSEL, 1);
        sdr_write32(MSDC_DAT_RDDLY0, cur_rxdly0);
        
        printk("i_data=%d i_delay=%d \n", i_data, i_delay);
    }
    else if (i_case==4)//set edge
    {
        if (!((i_par1==0) || (i_par1==1)))
            return -1;
        i_edge = i_par1;

        sdr_set_field((base+0x04), MSDC_IOCON_RSPL, i_edge);
        sdr_set_field((base+0x04), MSDC_IOCON_R_D_SMPL, i_edge);
    
        printk("i_edge=%d \n", i_edge);
    }
    else
    {
        return -1;
    }
    
    return 1;
}

#ifdef ONLINE_TUNING_DVTTEST

static int msdc_debug_proc_read_DVT_show(struct seq_file *m, void *data)
{
    return 0;
}

extern int mt_msdc_online_tuning_test(struct msdc_host *host, u32 rawcmd, u32 rawarg, u8 rw);
static ssize_t msdc_debug_proc_write_DVT(struct file *file, const char __user* buf, size_t count, loff_t *data)
{
    int ret;
    int i_msdc_id = 0;
    
    struct msdc_host *host;
    
    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;
        
    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);
    
    sscanf(cmd_buf, "%d", &i_msdc_id);
    
    if((i_msdc_id < 0) || (i_msdc_id >= HOST_MAX_NUM))
    {
        printk("[****SD_Debug****]msdc id %d out of range [0~%d]\n", i_msdc_id, HOST_MAX_NUM-1);
        return -1;
    }
    
    host = mtk_msdc_host[i_msdc_id];
    
    printk("[****SD_Debug****] Start Online Tuning DVT test \n");
    mt_msdc_online_tuning_test(host, 0, 0, 0);
    printk("[****SD_Debug****] Finish Online Tuning DVT test \n");
    
    return count;
}
#endif  // ONLINE_TUNING_DVTTEST

static int msdc_tune_proc_read_show(struct seq_file *m, void *data)
{
    seq_printf(m, "\n=========================================\n");
    seq_printf(m, "sdio_enable_tune: 0x%.8x\n", sdio_enable_tune);
    seq_printf(m, "sdio_iocon_dspl: 0x%.8x\n", sdio_iocon_dspl);
    seq_printf(m, "sdio_iocon_w_dspl: 0x%.8x\n", sdio_iocon_w_dspl);
    seq_printf(m, "sdio_iocon_rspl: 0x%.8x\n", sdio_iocon_rspl);
    seq_printf(m, "sdio_pad_tune_rrdly: 0x%.8x\n", sdio_pad_tune_rrdly);
    seq_printf(m, "sdio_pad_tune_rdly: 0x%.8x\n", sdio_pad_tune_rdly);
    seq_printf(m, "sdio_pad_tune_wrdly: 0x%.8x\n", sdio_pad_tune_wrdly);
    seq_printf(m, "sdio_dat_rd_dly0_0: 0x%.8x\n", sdio_dat_rd_dly0_0);
    seq_printf(m, "sdio_dat_rd_dly0_1: 0x%.8x\n", sdio_dat_rd_dly0_1);
    seq_printf(m, "sdio_dat_rd_dly0_2: 0x%.8x\n", sdio_dat_rd_dly0_2);
    seq_printf(m, "sdio_dat_rd_dly0_3: 0x%.8x\n", sdio_dat_rd_dly0_3);
    seq_printf(m, "sdio_dat_rd_dly1_0: 0x%.8x\n", sdio_dat_rd_dly1_0);
    seq_printf(m, "sdio_dat_rd_dly1_1: 0x%.8x\n", sdio_dat_rd_dly1_1);
    seq_printf(m, "sdio_dat_rd_dly1_2: 0x%.8x\n", sdio_dat_rd_dly1_2);
    seq_printf(m, "sdio_dat_rd_dly1_3: 0x%.8x\n", sdio_dat_rd_dly1_3);             
    seq_printf(m, "sdio_clk_drv: 0x%.8x\n", sdio_clk_drv);             
    seq_printf(m, "sdio_cmd_drv: 0x%.8x\n", sdio_cmd_drv);                         
    seq_printf(m, "sdio_data_drv: 0x%.8x\n", sdio_data_drv);
    seq_printf(m, "sdio_tune_flag: 0x%.8x\n", sdio_tune_flag);             
    seq_printf(m, "=========================================\n\n");

    return 0;
}

static ssize_t msdc_tune_proc_write(struct file *file, const char __user* buf, size_t count, loff_t *data)
{
    int ret;
    int cmd, p1, p2;
  
    if (count == 0)return -1;
    if(count > 255)count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;
    
    cmd_buf[count] = '\0';
    printk("msdc Write %s\n", cmd_buf);

    if(3  == sscanf(cmd_buf, "%x %x %x", &cmd, &p1, &p2))
    {    
        switch(cmd)
        {
            case 0:
                if(p1 && p2){
                    //sdio_enable_tune = 1;
                    msdc_dump_reg_level=0;
                    ettagent_init();        
                } else {    
                    //sdio_enable_tune = 0;   
                    ettagent_exit();
                    msdc_dump_reg_level=1;
                }   
                break;
            case 1://Cmd and Data latch edge
                sdio_iocon_rspl = p1&0x1;
                sdio_iocon_dspl = p2&0x1;
                break;
            case 2://Cmd Pad/Async
                sdio_pad_tune_rrdly= (p1&0x1F);
                sdio_pad_tune_rdly= (p2&0x1F);
                break;
            case 3:
                sdio_dat_rd_dly0_0= (p1&0x1F);
                sdio_dat_rd_dly0_1= (p2&0x1F);
                break;
            case 4:
                sdio_dat_rd_dly0_2= (p1&0x1F);
                sdio_dat_rd_dly0_3= (p2&0x1F);
                break;
            case 5://Write data edge/delay
                sdio_iocon_w_dspl= p1&0x1;
                sdio_pad_tune_wrdly= (p2&0x1F);
                break;
            case 6:
                sdio_dat_rd_dly1_2= (p1&0x1F);
                sdio_dat_rd_dly1_3= (p2&0x1F);
                break;
            case 7:
                sdio_clk_drv= (p1&0x7);
                break;
            case 8:
                sdio_cmd_drv= (p1&0x7);
                sdio_data_drv= (p2&0x7);
                break;
        }
    }

    return count;
}
static int msdc_proc_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_debug_proc_show, inode->i_private); 
} 

static const struct file_operations msdc_proc_fops = { 
    .open = msdc_proc_open, 
    .write = msdc_debug_proc_write,
    .read = seq_read, 
    .llseek = seq_lseek, 
    .release = single_release, 
};

static int msdc_help_proc_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_help_proc_show, inode->i_private); 
}
static const struct file_operations msdc_help_fops = {
    .open = msdc_help_proc_open,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
};

static int msdc_FT_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_debug_proc_read_FT_show, inode->i_private); 
}

static const struct file_operations msdc_FT_fops = {
    .open = msdc_FT_open,
    .write = msdc_debug_proc_write_FT,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
};

#ifdef ONLINE_TUNING_DVTTEST
static int msdc_DVT_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_debug_proc_read_DVT_show, inode->i_private); 
}

static const struct file_operations msdc_DVT_fops = {
    .open = msdc_DVT_open,
    .write = msdc_debug_proc_write_DVT,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
};
#endif  // ONLINE_TUNING_DVTTEST

static int msdc_tune_proc_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_tune_proc_read_show, inode->i_private); 
}
static const struct file_operations msdc_tune_fops = {
    .open = msdc_tune_proc_open,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
    .write      = msdc_tune_proc_write,
};
static int msdc_tune_flag_proc_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_tune_flag_proc_read_show, inode->i_private); 
}
static const struct file_operations msdc_tune_flag_fops = {
    .open = msdc_tune_flag_proc_open,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
};

#ifdef K2_HQA
u32 sdio_vio18_flag = 0;
u32 sdio_vcore1_flag = 0;
u32 sdio_vcore2_flag = 0;
u32 vio18_reg = 0;
u32 vcore1_reg = 0;
u32 vcore2_reg = 0;
extern void pmic_config_interface(unsigned int, unsigned int, unsigned int, unsigned int);
static ssize_t msdc_voltage_proc_write(struct file *file, const char __user* buf, size_t count, loff_t *data)
{
    int ret;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0)return -1;

    cmd_buf[count] = '\0';
    printk("[****SD_Debug****]msdc Write %s\n", cmd_buf);

    sscanf(cmd_buf, "%d %d %d", &sdio_vio18_flag, &sdio_vcore1_flag, &sdio_vcore2_flag);

    if( sdio_vio18_flag > 1600 && sdio_vio18_flag <2000)
    {
        //0.0125V per step
        //Originally divied by 12.5, to avoid floating-point division, amplify numerator and denominator by 4
        vio18_reg = ((sdio_vio18_flag-1400)<<2)/50;
        pmic_config_interface(0x68c, vio18_reg, 0x7F, 0); //VIO18 1700mv 0x18
        pmic_config_interface(0x68e, vio18_reg, 0x7F, 0);
    }
    #if 0
    //For K2, Vcore1 is VLTE
    if( sdio_vcore1_flag > 900 && sdio_vcore1_flag <1200)
    {
        //0.00625V per step
        //Originally divied by 12.5, to avoid floating-point division, amplify numerator and denominator by 4
        vcore1_reg = ((sdio_vcore1_flag-600)<<2)/25;
        pmic_config_interface(0x63c, vcore1_reg, 0x7F, 0); //VCORE1 1100mv 0x40
        pmic_config_interface(0x63e, vcore1_reg, 0x7F, 0);
    }
    #endif
    //For K2, Vcore2 is VCORE_AO
    if( sdio_vcore2_flag > 900 && sdio_vcore2_flag <1200)
    {
        //0.00625V per step
        //Originally divied by 12.5, to avoid floating-point division, amplify numerator and denominator by 4
        vcore2_reg = ((sdio_vcore2_flag-600)<<2)/25;
        pmic_config_interface(0x662, vcore2_reg, 0x7F, 0); //VCORE2 990mv 0x2e
        pmic_config_interface(0x664, vcore2_reg, 0x7F, 0);
    }

    return count;
}
static int msdc_voltage_flag_proc_read_show(struct seq_file *m, void *data)
{
    seq_printf(m, "vio18: 0x%d 0x%X\n", sdio_vio18_flag, vio18_reg);
    seq_printf(m, "vcore1: 0x%d 0x%X\n", sdio_vcore1_flag,  vcore1_reg);
    seq_printf(m, "vcore2: 0x%d 0x%X\n", sdio_vcore2_flag, vcore2_reg);
    return 0;
}
static int msdc_voltage_flag_proc_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, msdc_voltage_flag_proc_read_show, inode->i_private); 
}
static const struct file_operations msdc_voltage_flag_fops = {
    .open = msdc_voltage_flag_proc_open,
    .read = seq_read,
    .llseek = seq_lseek, 
    .release = single_release, 
    .write = msdc_voltage_proc_write,
};
#endif
int msdc_debug_proc_init(void) 
{       
    struct proc_dir_entry *prEntry;
    struct proc_dir_entry *tune;
    struct proc_dir_entry *tune_flag;
    struct proc_dir_entry *voltage_flag;
    
#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_debug", 0660, NULL, &msdc_proc_fops);
#else
    prEntry = proc_create("msdc_debug", 0660, NULL, &msdc_proc_fops);
#endif
    if(prEntry)
    {
        printk("[%s]: successfully create /proc/msdc_debug\n", __func__);
        proc_set_user(prEntry, 0, 1001);
    }else{
        printk("[%s]: failed to create /proc/msdc_debug\n", __func__);
    }

#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_help", 0660, NULL, &msdc_help_fops);
#else
    prEntry = proc_create("msdc_help", 0440, NULL, &msdc_help_fops);
#endif
    if(prEntry)
    {
        printk("[%s]: successfully create /proc/msdc_help\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_help\n", __func__);
    }

#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_FT", 0660, NULL, &msdc_FT_fops);
#else
    prEntry = proc_create("msdc_FT", 0440, NULL, &msdc_FT_fops);
#endif
    if(prEntry)
    {
        printk("[%s]: successfully create /proc/msdc_FT\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_FT\n", __func__);
    }
    
#ifdef ONLINE_TUNING_DVTTEST
#ifndef USER_BUILD_KERNEL
    prEntry = proc_create("msdc_DVT", 0660, NULL, &msdc_DVT_fops);
#else
    prEntry = proc_create("msdc_DVT", 0440, NULL, &msdc_DVT_fops);
#endif
    if(prEntry)
    {
        printk("[%s]: successfully create /proc/msdc_DVT\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_DVT\n", __func__);
    }
#endif  // ONLINE_TUNING_DVTTEST

    memset(msdc_drv_mode,0,sizeof(msdc_drv_mode));
#ifndef USER_BUILD_KERNEL
    tune = proc_create("msdc_tune", 0660, NULL, &msdc_tune_fops);
#else
    tune = proc_create("msdc_tune", 0460, NULL, &msdc_tune_fops);
#endif
    if(tune)
    {
        proc_set_user(tune, 0, 1001);
        printk("[%s]: successfully create /proc/msdc_tune\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_tune\n", __func__);
    }
#ifndef USER_BUILD_KERNEL
    tune_flag = proc_create("msdc_tune_flag", 0660, NULL, &msdc_tune_flag_fops);
#else
    tune_flag = proc_create("msdc_tune_flag", 0440, NULL, &msdc_tune_flag_fops);
#endif
    if(tune_flag)
    {
        printk("[%s]: successfully create /proc/msdc_tune_flag\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_tune_flag\n", __func__);
    }
#ifdef K2_HQA
#ifndef USER_BUILD_KERNEL
    voltage_flag = proc_create("msdc_voltage_flag", 0660, NULL, &msdc_voltage_flag_fops);
#else
    voltage_flag = proc_create("msdc_voltage_flag", 0460, NULL, &msdc_voltage_flag_fops);
#endif
    if(voltage_flag)
    {
        proc_set_user(voltage_flag, 0, 1001);
        printk("[%s]: successfully create /proc/msdc_voltage_flag\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/msdc_voltage_flag\n", __func__);
    }
#endif
    return 0 ;
}
EXPORT_SYMBOL_GPL(msdc_debug_proc_init);
