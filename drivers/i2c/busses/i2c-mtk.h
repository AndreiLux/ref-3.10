
#ifndef __MT_I2C_H__
#define __MT_I2C_H__
#include <mach/mt_typedefs.h>
#include <mach/i2c.h>

enum {
	MT_I2C_HS = (MT_DRIVER_FIRST << 0),
};

enum I2C_REGS_OFFSET {
	OFFSET_DATA_PORT = 0x0,	/* 0x0 */
	OFFSET_SLAVE_ADDR = 0x04,	/* 0x04 */
	OFFSET_INTR_MASK = 0x08,	/* 0x08 */
	OFFSET_INTR_STAT = 0x0C,	/* 0x0C */
	OFFSET_CONTROL = 0x10,	/* 0X10 */
	OFFSET_TRANSFER_LEN = 0x14,	/* 0X14 */
	OFFSET_TRANSAC_LEN = 0x18,	/* 0X18 */
	OFFSET_DELAY_LEN = 0x1C,	/* 0X1C */
	OFFSET_TIMING = 0x20,	/* 0X20 */
	OFFSET_START = 0x24,	/* 0X24 */
	OFFSET_EXT_CONF = 0x28,
	OFFSET_FIFO_STAT = 0x30,	/* 0X30 */
	OFFSET_FIFO_THRESH = 0x34,	/* 0X34 */
	OFFSET_FIFO_ADDR_CLR = 0x38,	/* 0X38 */
	OFFSET_IO_CONFIG = 0x40,	/* 0X40 */
	OFFSET_RSV_DEBUG = 0x44,	/* 0X44 */
	OFFSET_HS = 0x48,	/* 0X48 */
	OFFSET_SOFTRESET = 0x50,	/* 0X50 */
	OFFSET_PATH_DIR = 0x60,
	OFFSET_DEBUGSTAT = 0x64,	/* 0X64 */
	OFFSET_DEBUGCTRL = 0x68,	/* 0x68 */
};

struct mt_i2c_msg {
	u16 addr;		/* slave address                        */
	u16 flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	u16 len;		/* msg length                           */
	u8 *buf;		/* pointer to msg data                  */
	u32 ext_flag;
};

#define I2C_A_FILTER_MSG	0x8000	/* filer out error messages     */
#define I2C_A_CHANGE_TIMING	0x4000	/* change timing paramters      */
#define I2C_MASK_FLAG	(0x00ff)
#define I2C_WR_FLAG		(0x1000)
#define I2C_DMA_FLAG	(0x2000)
#define I2C_RS_FLAG		(0x0800)
#define I2C_HS_FLAG   (0x0400)
#define I2C_ENEXT_FLAG (0x0200)
#define I2C_DISEXT_FLAG (0x0000)
#define I2C_POLL_FLAG (0x4000)
#define I2C_CH2_FLAG	(0x8000)

#define I2C_PUSHPULL_FLAG (0x00000002)
#define I2C_3DCAMERA_FLAG (0x00000004)
#define I2C_DIRECTION_FLAG (0x00000008)

int mt_i2c_master_send(const struct i2c_client *client,
	const char *buf, int count, u32 ext_flag);
int mt_i2c_master_recv(
	const struct i2c_client *client, char *buf, int count, u32 ext_flag);
int mt_i2c_transfer(
	struct i2c_adapter *adap, struct mt_i2c_msg *msgs, int num);

#define I2C_HS_NACKERR				(1 << 2)
#define I2C_ACKERR					(1 << 1)
#define I2C_TRANSAC_COMP			(1 << 0)

#define I2C_FIFO_SIZE               8

#define MAX_ST_MODE_SPEED           100	/* khz */
#define MAX_FS_MODE_SPEED           400	/* khz */
#define MAX_HS_MODE_SPEED           3400	/* khz */

#define MAX_DMA_TRANS_SIZE          252	/* Max(255) aligned to 4 bytes = 252 */
#define MAX_DMA_TRANS_NUM           256

#define MAX_SAMPLE_CNT_DIV          8
#define MAX_STEP_CNT_DIV            64
#define MAX_HS_STEP_CNT_DIV         8

#define DMA_ADDRESS_HIGH				(0xC0000000)

/* #define I2C_DEBUG */

#ifdef I2C_DEBUG
#define I2C_BUG_ON(a) BUG_ON(a)
#else
#define I2C_BUG_ON(a)
#endif

enum DMA_REGS_OFFSET {
	OFFSET_INT_FLAG = 0x0,
	OFFSET_INT_EN = 0x04,
	OFFSET_EN = 0x08,
	OFFSET_CON = 0x18,
	OFFSET_TX_MEM_ADDR = 0x1c,
	OFFSET_RX_MEM_ADDR = 0x20,
	OFFSET_TX_LEN = 0x24,
	OFFSET_RX_LEN = 0x28,
};

enum i2c_trans_st_rs {
	I2C_TRANS_STOP = 0,
	I2C_TRANS_REPEATED_START,
};

enum {
	ST_MODE,
	FS_MODE,
	HS_MODE,
};


enum mt_trans_op {
	I2C_MASTER_NONE = 0,
	I2C_MASTER_WR = 1,
	I2C_MASTER_RD,
	I2C_MASTER_WRRD,
};

/* CONTROL */
#define I2C_CONTROL_RS          (0x1 << 1)
#define I2C_CONTROL_DMA_EN      (0x1 << 2)
#define I2C_CONTROL_CLK_EXT_EN      (0x1 << 3)
#define I2C_CONTROL_DIR_CHANGE      (0x1 << 4)
#define I2C_CONTROL_ACKERR_DET_EN   (0x1 << 5)
#define I2C_CONTROL_TRANSFER_LEN_CHANGE (0x1 << 6)
#define I2C_CONTROL_WRAPPER          (0x1 << 0)

#define I2C_DRV_NAME				"mt-i2c"
#define I2C_DMA_BUF_SIZE 1024

struct i2c_dma_buf {
	u8 *vaddr;
	dma_addr_t paddr;
};

struct mt_trans_data {
	u16 trans_num;
	u16 data_size;
	u16 trans_len;
	u16 trans_auxlen;
};

struct mt_i2c {
	struct i2c_adapter adap;	/* i2c host adapter */
	struct device *dev;	/* the device object of i2c host adapter */
	u32 base;		/* i2c base addr */
	u16 id;
	u16 speed;
	u16 irqnr;		/* i2c interrupt number */
	u16 irq_stat;		/* i2c interrupt status */
	spinlock_t lock;	/* for mt_i2c struct protection */
	wait_queue_head_t wait;	/* i2c transfer wait queue */

	atomic_t trans_err;	/* i2c transfer error */
	atomic_t trans_comp;	/* i2c transfer completion */
	atomic_t trans_stop;	/* i2c transfer stop */

	unsigned long clk;	/* host clock speed in khz */
	unsigned long sclk;	/* khz */
	int pdn;		/*clock number */

	unsigned char master_code;	/* master code in HS mode */
	unsigned char mode;	/* ST/FS/HS mode */

	enum i2c_trans_st_rs st_rs;
	bool dma_en;
	bool suspended;
	u32 pdmabase;
	u16 delay_len;
	enum mt_trans_op op;
	struct mt_trans_data trans_data;
	struct i2c_dma_buf dma_buf;
	struct mt_i2c_data *pdata;
};

/*this field is only for 3d camera*/
static struct mt_i2c_msg g_msg[2];
static struct mt_i2c *g_i2c[2];

/* I2C GPIO debug */
struct mt_i2c_gpio_t {
	u16 scl;
	u16 sda;
};
#endif				/* __MT_I2C_H__ */
