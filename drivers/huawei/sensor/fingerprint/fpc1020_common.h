/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_COMMON_H
#define LINUX_SPI_FPC1020_COMMON_H

#define DEBUG
#define CONFIG_INPUT_FPC1020_NAV
#define CONFIG_INPUT_FPC1020_WAKEUP
//#define NO_NAV
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include "fpc1020.h"
#include "fpc1020_regs.h"
#include <linux/wakelock.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#define CS_CONTROL  0
/* -------------------------------------------------------------------- */
/* fpc1020 driver constants						*/
/* -------------------------------------------------------------------- */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define target_little_endian  1
#else
//	#warning BE target not tested!
	#define target_little_endian 0
#endif

#define FPC1020_DEV_NAME                        "fpc1020"
#define FPC1020_INPUT_NAME                      "fingerprint"
#define FPC1020_SPI_CLOCK_SPEED			(8 * 1000000U)

#define FPC1020_PIXEL_ROWS			192U
#define FPC1020_PIXEL_COLUMNS			192U
#define FPC1020_FRAME_SIZE_MAX			(FPC1020_PIXEL_COLUMNS * \
						 FPC1020_PIXEL_ROWS)

#define FPC1020_ADC_GROUP			8
#define	FPC1020_PIXEL_COL_GROUPS 		(FPC1020_PIXEL_COLUMNS / \
											FPC1020_ADC_GROUP)

#define FPC1020_BUFFER_MAX_IMAGES		3
#define FPC1020_IMAGE_BUFFER_SIZE		FPC1020_FRAME_SIZE_MAX * \
										FPC1020_BUFFER_MAX_IMAGES

#define FPC1020_DEFAULT_IRQ_TIMEOUT_MS		(100 * HZ / 1000)

#define FPC1020_STATUS_REG_RESET_VALUE 0x1e

#define FPC1020_STATUS_REG_MODE_MASK ( \
		FPC_1020_STATUS_REG_BIT_MAIN_IDLE_CMD | \
		FPC_1020_STATUS_REG_BIT_SYNC_PWR_IDLE | \
		FPC_1020_STATUS_REG_BIT_PWR_DWN_OSC_HIN)

#define FPC1020_STATUS_REG_IN_DEEP_SLEEP_MODE	0

#define FPC1020_STATUS_REG_IN_SLEEP_MODE	0

#define FPC1020_STATUS_REG_IN_IDLE_MODE ( \
		FPC_1020_STATUS_REG_BIT_MAIN_IDLE_CMD | \
		FPC_1020_STATUS_REG_BIT_SYNC_PWR_IDLE | \
		FPC_1020_STATUS_REG_BIT_PWR_DWN_OSC_HIN)

#define FPC1020_SLEEP_RETRIES			5
#define FPC1020_SLEEP_RETRY_TIME_US		1000

#define FPC1020_POLLING_TIME_MS		10

#define FPC1020_FINGER_DOWN_THRESHOLD		6
#define FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS 	10
#define FPC1020_DEADPIXEL_THRESHOLD  60

#define FPC1020_RESET_RETRIES			2
#define FPC1020_RESET_LOW_US			1000
#define FPC1020_RESET_HIGH1_US			100
#define FPC1020_RESET_HIGH2_US			1250
/* -------------------------------------------------------------------- */
/* fpc1020 data types							*/
/* -------------------------------------------------------------------- */
typedef enum{
	fp_UNINIT = 0,
	fp_LCD_UNBLANK = 1,
	fp_LCD_POWEROFF = 2,
	fp_CAPTURE = 11,
	fp_TOUCH = 12,
	fp_LONGPRESSWAKEUP = 13,
	fp_STATE_UNDEFINE = 255,
}fingerprint_state;

enum{
	FPC_1020_WAKEUP_DEV_ERROR 		= 11,
	FPC_1020_RESET_ENTRY_ERROR,				/*12*/
	FPC_1020_VERIFY_HW_ID_ERROR,		/*13*/
	FPC_1020_CAPTURE_IMAGE_ERROR,	/*14*/
	FPC_1020_IRQ_NOT_PULL_UP_ERROR,	/*15*/
	FPC_1020_WAIT_IRQ_ERROR,			/*16*/
	FPC_1020_CLEAR_IRQ_ERROR,		/*17*/
	FPC_1020_IRQ_NOT_PULL_LOW_ERROR,	/*18*/
	FPC_1020_READ_STATUS_REG_ERROR,	/*19*/
	FPC_1020_TOO_MANY_DEADPIXEL_ERROR,		/*20*/
	FPC_1020_RESET_EXIT_ERROR,		/*21*/
	FPC_1020_WAIT_FOR_FINGER_ERROR,		/*22*/
	FPC_1020_FINGER_DETECT_ERROR,		/*23*/
	FPC_1020_MIN_CHECKER_DIFF_ERROR,		/*24*/
};

typedef enum {
	FPC_1020_STATUS_REG_BIT_IRQ			= 1 << 0,
	FPC_1020_STATUS_REG_BIT_MAIN_IDLE_CMD		= 1 << 1,
	FPC_1020_STATUS_REG_BIT_SYNC_PWR_IDLE		= 1 << 2,
	FPC_1020_STATUS_REG_BIT_PWR_DWN_OSC_HIN		= 1 << 3,
	FPC_1020_STATUS_REG_BIT_FIFO_EMPTY		= 1 << 4,
	FPC_1020_STATUS_REG_BIT_FIFO_FULL		= 1 << 5,
	FPC_1020_STATUS_REG_BIT_MISO_EDGRE_RISE_EN	= 1 << 6
} fpc1020_status_reg_t;

typedef enum {
	FPC1020_CMD_FINGER_PRESENT_QUERY	= 32,
	FPC1020_CMD_WAIT_FOR_FINGER_PRESENT	= 36,
	FPC1020_CMD_ACTIVATE_SLEEP_MODE		= 40,
	FPC1020_CMD_ACTIVATE_DEEP_SLEEP_MODE	= 44,
	FPC1020_CMD_ACTIVATE_IDLE_MODE		= 52,
	FPC1020_CMD_CAPTURE_IMAGE		= 192,
	FPC1020_CMD_READ_IMAGE			= 196,
	FPC1020_CMD_SOFT_RESET			= 248
} fpc1020_cmd_t;

typedef enum {
	FPC_1020_IRQ_REG_BIT_FINGER_DOWN   = 1 << 0,
	FPC_1020_IRQ_REG_BIT_ERROR         = 1 << 2,
	FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA = 1 << 5,
	FPC_1020_IRQ_REG_BIT_COMMAND_DONE  = 1 << 7,
	FPC_1020_IRQ_REG_BITS_REBOOT       = 0xff
} fpc1020_irq_reg_t;

typedef enum {
	FPC1020_CAPTURE_STATE_IDLE = 0,
	FPC1020_CAPTURE_STATE_STARTED,
	FPC1020_CAPTURE_STATE_PENDING,
	FPC1020_CAPTURE_STATE_WRITE_SETTINGS,
	FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN,
	FPC1020_CAPTURE_STATE_ACQUIRE,
	FPC1020_CAPTURE_STATE_FETCH,
	FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP,
	FPC1020_CAPTURE_STATE_COMPLETED,
	FPC1020_CAPTURE_STATE_FAILED,
} fpc1020_capture_state_t;

typedef struct fpc1020_worker_struct {
	struct task_struct *thread;
	struct semaphore sem_idle;
	wait_queue_head_t wq_wait_job;
	int req_mode;
	bool stop_request;
} fpc1020_worker_t;

typedef struct fpc1020_capture_struct {
	fpc1020_capture_mode_t current_mode;
	fpc1020_capture_state_t state;
	u32                    read_offset;
	u32                    available_bytes;
	wait_queue_head_t      wq_data_avail;
	int                    last_error;
	bool read_pending_eof;
} fpc1020_capture_task_t;

#ifdef CONFIG_INPUT_FPC1020_NAV
typedef struct fpc1020_nav_struct {
	bool enabled;
} fpc1020_nav_task_t;
#endif

typedef struct fpc1020_setup {
	u8 adc_gain[FPC1020_BUFFER_MAX_IMAGES];
	u8 adc_shift[FPC1020_BUFFER_MAX_IMAGES];
	u16 pxl_ctrl[FPC1020_BUFFER_MAX_IMAGES];
	u8 capture_settings_mux;
	u8 capture_count;
	fpc1020_capture_mode_t capture_mode;
	u8 capture_row_start;	/* Row 0-191        */
	u8 capture_row_count;	/* Rows <= 192      */
	u8 capture_col_start;	/* ADC group 0-23   */
	u8 capture_col_groups;	/* ADC groups, 1-24 */
} fpc1020_setup_t;


typedef struct fpc1020_diag {
	const char *chip_id;	/* RO */
	u8 selftest;		/* RO */
	u8 spi_register;	/* RW */
	u8 spi_regsize;		/* RO */
	u8 spi_data;		/* RW */
	u8 fingerdetect;	/* RW */
	u8 navigation_enable;
	u8 wakeup_enable;
} fpc1020_diag_t;

typedef struct {
	struct spi_device		*spi;
	struct class			*class;
	struct device			*device;
	struct cdev				cdev;
	dev_t					devno;
	fpc1020_chip_t			chip_type;
	u8						chip_revision;
	u32						cs_gpio;
	u32						reset_gpio;
	u32						irq_gpio;
	u32						moduleID_gpio;
	int						irq;
	wait_queue_head_t		wq_irq_return;
	bool					interrupt_done;
	struct semaphore		mutex;
	u8						*huge_buffer;
	size_t					huge_buffer_size;
	fpc1020_worker_t		worker;
	fpc1020_capture_task_t	capture;
	fpc1020_setup_t			setup;
	fpc1020_diag_t			diag;
	bool					soft_reset_enabled;
	struct regulator		*vcc_spi;
	struct regulator		*vdd_ana;
	struct regulator		*vdd_io;
	bool					power_enabled;
#ifdef CONFIG_INPUT_FPC1020_NAV
	struct input_dev		*input_dev;
	fpc1020_nav_task_t		nav;
#endif
	struct wake_lock		fp_wake_lock;
	atomic_t				state; //wake or sleep
	atomic_t				taskstate; //capture \ nav \ wakeup
#if defined(CONFIG_FB)
	struct					notifier_block fb_notify;
#endif
} fpc1020_data_t;

typedef struct {
	fpc1020_reg_t	reg;
	bool			write;
	u16				reg_size;
	u8				*dataptr;
} fpc1020_reg_access_t;


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
extern int fpc1020_setup_defaults(fpc1020_data_t *fpc1020);

extern int fpc1020_gpio_reset(fpc1020_data_t *fpc1020);

extern int fpc1020_spi_reset(fpc1020_data_t *fpc1020);

extern int fpc1020_reset(fpc1020_data_t *fpc1020);

extern int fpc1020_check_hw_id(fpc1020_data_t *fpc1020);

extern const char *fpc1020_hw_id_text(fpc1020_data_t *fpc1020);

extern int fpc1020_write_sensor_setup(fpc1020_data_t *fpc1020);

extern int fpc1020_write_sensor_checkerboard_setup(fpc1020_data_t *fpc1020);

extern int fpc1020_wait_for_irq(fpc1020_data_t *fpc1020, int timeout);

extern int fpc1020_read_irq(fpc1020_data_t *fpc1020, bool clear_irq);

extern int fpc1020_read_status_reg(fpc1020_data_t *fpc1020);

extern int fpc1020_reg_access(fpc1020_data_t *fpc1020,
								fpc1020_reg_access_t *reg_data);

extern int fpc1020_cmd(fpc1020_data_t *fpc1020, fpc1020_cmd_t cmd,
			u8 wait_irq_mask);

extern int fpc1020_wait_finger_present(fpc1020_data_t *fpc1020);

extern int fpc1020_check_finger_present_raw(fpc1020_data_t *fpc1020);

extern int fpc1020_check_finger_present_sum(fpc1020_data_t *fpc1020);

extern int fpc1020_wake_up(fpc1020_data_t *fpc1020);

extern int fpc1020_sleep(fpc1020_data_t *fpc1020, bool deep_sleep);

extern int fpc1020_fetch_image(fpc1020_data_t *fpc1020,
				u8 *buffer,
				int offset,
				size_t image_size_bytes,
				size_t buff_size);

extern bool fpc1020_check_in_range_u64(u64 val, u64 min, u64 max);
extern size_t fpc1020_calc_image_size(fpc1020_data_t *fpc1020);

#define FPC1020_MK_REG_READ_BYTES(__dst, __reg, __count, __ptr) {	\
	(__dst).reg      = (__reg);					\
	(__dst).reg_size = (__count);					\
	(__dst).write    = false;					\
	(__dst).dataptr  = (__ptr); }

#define FPC1020_MK_REG_READ(__dst, __reg, __ptr) {	\
	(__dst).reg      = (__reg);			\
	(__dst).reg_size = FPC1020_REG_SIZE((__reg));	\
	(__dst).write    = false;			\
	(__dst).dataptr  = (u8 *)(__ptr); }

#define FPC1020_MK_REG_WRITE_BYTES(__dst, __reg, __count, __ptr) {	\
	(__dst).reg      = (__reg);					\
	(__dst).reg_size = (__count);					\
	(__dst).write    = true;					\
	(__dst).dataptr  = (__ptr); }

#define FPC1020_MK_REG_WRITE(__dst, __reg, __ptr) {	\
	(__dst).reg      = (__reg);			\
	(__dst).reg_size = FPC1020_REG_SIZE((__reg));	\
	(__dst).write    = true;			\
	(__dst).dataptr  = (u8 *)(__ptr); }

#define FPC1020_FINGER_DETECT_ZONE_MASK		0x0FFFU

#endif /* LINUX_SPI_FPC1020_COMMON_H */

