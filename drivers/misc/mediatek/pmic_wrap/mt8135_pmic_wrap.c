#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched.h>

#include <mach/mt_pmic_wrap.h>
#include <mach/mt_gpio_def.h>
#include "mt8135_pmic_wrap.h"

#define PMIC_WRAP_DEVICE "pmic_wrap"

struct pwrap_debug_data {
	u64 wacs_time;
	int operation;
	int result;
	u32 addr;
	u32 data;
};

#define PWRAP_DEBUG_COUNT 50

struct pmic_wrapper {
	struct platform_device *pdev;
	spinlock_t trace_lock;
	spinlock_t lock;
	bool suspended;
	struct pwrap_debug_data pwrap_debug_data[PWRAP_DEBUG_COUNT];
	u32 pwrap_debug_index;
};

static struct pmic_wrapper mt_wrp;
static struct pmic_wrapper *g_wrp;

#define PWRAP_EVENT_GPIO 47

#define PWRAP_TIMEOUT
#ifdef PWRAP_TIMEOUT
static u64 _pwrap_get_current_time(void)
{
	return sched_clock();
}

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	cur_time = _pwrap_get_current_time(); /* ns */
	if (cur_time < start_time_ns) {
		PWRAPERR("%s: Timer overflow! start%lld cur timer%lld\n",
			__func__, start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 255*1000; /* 255us */
		PWRAPERR("%s: reset timer! start%lld setting%lld\n",
			__func__, start_time_ns, timeout_time_ns);
	}
	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		return true;
	}

	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return time_us*1000;
}

#else
static u64 _pwrap_get_current_time(void)
{
	return 0;
}

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 elapse_time)
{
	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return 0;
}


#endif

static int pwrap_wacs2_locked(bool write, bool dochk, u32 adr, u32 *data);
static void pwrap_trace_wacs2(struct pmic_wrapper *wrp);

static inline int pwrap_read_locked(u32 adr, u32 *rdata)
{
	return pwrap_wacs2_locked(PWRAP_READ, false, adr, rdata);
}

static inline int pwrap_write_locked(u32 adr, u32 wdata)
{
	u32 data = wdata;
	return pwrap_wacs2_locked(PWRAP_WRITE, false, adr, &data);
}

static void pwrap_trace(struct pmic_wrapper *wrp, u64 wacs_time,
	int result, bool write, u32 addr, u32 data)
{
	struct pwrap_debug_data *dbg;
	u32 index;
	unsigned long flags;
	spin_lock_irqsave(&wrp->trace_lock, flags);
	index = wrp->pwrap_debug_index;
	dbg = &wrp->pwrap_debug_data[index];
	dbg->operation = write ? PWRAP_WRITE : PWRAP_READ;
	dbg->wacs_time = wacs_time;
	dbg->result = result;
	dbg->addr = addr;
	dbg->data = data;
	index++;
	if (index == PWRAP_DEBUG_COUNT)
		index = 0;
	wrp->pwrap_debug_index = index;
	spin_unlock_irqrestore(&wrp->trace_lock, flags);
}

static  void pwrap_dump_all_register(struct pmic_wrapper *wrp)
{
	u32 i = 0;
	u32 reg_addr = 0;
	u32 reg_value = 0;
	int gpio_in_state = 0;
	int gpio_out_state = 0;
	int gpio_dir = 0;
	int gpio_mode = 0;
	bool pull_en = false;
	bool pull_up = false;
	bool gpio_eint = 0;

	PWRAPREG("dump pwrap register\n");
	for (i = 0; i < 90; i++) {
		reg_addr = (PMIC_WRAP_BASE + i * 4);
		reg_value = WRAP_RD32(reg_addr);
		PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
	}
	PWRAPREG("dump peri_pwrap register\n");
	for (i = 0; i < 25; i++) {
		reg_addr = (PERI_PWRAP_BRIDGE_BASE + i * 4);
		reg_value = WRAP_RD32(reg_addr);
		PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
	}
	PWRAPREG("dump dewrap register\n");
	for (i = 0; i < 25; i++) {
		reg_addr = (DEW_BASE + i * 2);
		pwrap_read(reg_addr, &reg_value);
		PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
	}

	/* dump PMIC_TOP_CKCON3 status */
	pwrap_read(PMIC_TOP_CKCON3, &reg_value);
	PWRAPREG("PMIC_TOP_CKCON3: 0x%04X\n", reg_value);

	gpio_dir = mt_pin_get_dir(PWRAP_EVENT_GPIO);
	gpio_mode = mt_pin_get_mode(PWRAP_EVENT_GPIO);
	gpio_in_state = mt_pin_get_din(PWRAP_EVENT_GPIO);
	gpio_out_state = mt_pin_get_dout(PWRAP_EVENT_GPIO);
	mt_pin_get_pull(PWRAP_EVENT_GPIO, &pull_en, &pull_up);
	gpio_eint = mt_gpio_to_eint(PWRAP_EVENT_GPIO);
	PWRAPREG("PWRAP_EVENT GPIO dir: %d, mode=%d, in=%d, out=%d, pull_en=%d, pull_up=%d, eint=%d\n",
		gpio_dir, gpio_mode, gpio_in_state, gpio_out_state, pull_en, pull_up, gpio_eint);

}

static  void pwrap_dump_ap_register(void)
{
	u32 i = 0;
	u32 reg_addr = 0;
	u32 reg_value = 0;
	PWRAPREG("dump pwrap register\n");
	for (i = 0; i < 90; i++) {
		reg_addr = (PMIC_WRAP_BASE + i * 4);
		reg_value = WRAP_RD32(reg_addr);
		PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
	}
	PWRAPREG("dump peri_pwrap register\n");
	for (i = 0; i < 25; i++) {
		reg_addr = (PERI_PWRAP_BRIDGE_BASE + i * 4);
		reg_value = WRAP_RD32(reg_addr);
		PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
	}
}

typedef bool (*loop_condition_fp) (u32);	/* define a function pointer */

static inline bool wait_for_fsm_idle(u32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_IDLE;
}

static inline bool wait_for_fsm_vldclr(u32 x)
{
	return GET_WACS0_FSM(x) != WACS_FSM_WFVLDCLR;
}

static inline bool wait_for_sync(u32 x)
{
	return GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE;
}

static inline bool wait_for_idle_and_sync(u32 x)
{
	return  (GET_WACS0_FSM(x) != WACS_FSM_IDLE) ||
			(GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}

static inline bool wait_for_wrap_idle(u32 x)
{
	return  (GET_WRAP_FSM(x) != 0x0) ||
			(GET_WRAP_CH_DLE_RESTCNT(x) != 0x0);
}

static inline bool wait_for_wrap_state_idle(u32 x)
{
	return GET_WRAP_AG_DLE_RESTCNT(x) != 0;
}

static inline bool wait_for_man_idle_and_noreq(u32 x)
{
	return  (GET_MAN_REQ(x) != MAN_FSM_NO_REQ) ||
			(GET_MAN_FSM(x) != MAN_FSM_IDLE);
}

static inline bool wait_for_man_vldclr(u32 x)
{
	return GET_MAN_FSM(x) != MAN_FSM_WFVLDCLR;
}

static inline bool wait_for_cipher_ready(u32 x)
{
	return x != 1;
}

static inline bool wait_for_stdupd_idle(u32 x)
{
	return GET_STAUPD_FSM(x) != 0x0;
}

static inline int wait_for_state_ready_init(loop_condition_fp fp,
		u32 timeout_us, u32 wacs_register, u32 *read_reg)
{
	u32 reg_rdata = 0x0;
	u64 start_time_ns = 0, timeout_ns = 0;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	do {
		reg_rdata = WRAP_RD32(wacs_register);

		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout wait_for_state_ready_init\n");
			return -ETIMEDOUT;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

static inline int wait_for_state_idle(loop_condition_fp fp, u32 timeout_us, u32 wacs_register,
				      u32 wacs_vldclr_register, u32 *read_reg)
{
	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	reg_rdata = WRAP_RD32(wacs_register);
	if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
		PWRAPERR("initialization isn't finished\n");
		return -ENODEV;
	}
	do {
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}

		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			pwrap_trace_wacs2(g_wrp);
			return -ETIMEDOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));	/* IDLE State */

	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

static inline int wait_for_state_ready(loop_condition_fp fp,
	u32 timeout_us, u32 wacs_register, u32 *read_reg)
{
	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	reg_rdata = WRAP_RD32(wacs_register);

	if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
		PWRAPERR("initialization isn't finished\n");
		return -ENODEV;
	}
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			return -ETIMEDOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));	/* IDLE State */

	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

static int pwrap_wacs2_locked(bool write, bool dochk, u32 adr, u32 *data)
{
	u32 reg_rdata = 0x0;
	u32 wacs_write = 0x0;
	u32 wacs_adr = 0x0;
	u32 wacs_cmd = 0x0;
	int ret;
	u32 wdata;
	/* PWRAPFUC(); */
	/* Check argument validation */

	if ((adr & ~(0xfffe)) != 0 || !data)
		return -EINVAL;

	/* Check IDLE */
	if (dochk)
		ret = wait_for_state_idle(wait_for_fsm_idle,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
			PMIC_WRAP_WACS2_VLDCLR, 0);
	else
		ret = wait_for_state_ready_init(wait_for_fsm_idle,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (ret < 0) {
		PWRAPERR("%s write command fail, ret=%d\n", __func__, ret);
		return ret;
	}

	if (write) {
		wdata = *data & 0xFFFF;
		wacs_write = 1 << 31;
	} else
		wdata = 0;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);

	if (!write) {
		u32 rval;
		/* wait for read data ready */
		ret = wait_for_state_ready_init(wait_for_fsm_vldclr,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, &reg_rdata);
		if (ret < 0) {
			PWRAPERR("%s read fail, ret=%d\n", __func__, ret);
			return ret;
		}
		rval = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
		*data = rval;
	}
	return 0;
}

struct pwrap_rwspin_record {
	u32 addr;
	bool write;
	bool spin_lock;
};

static struct pwrap_rwspin_record rwspin_record;

void pwrap_dump_rwspin_user(void)
{
	pr_err("[Power/PWRAP]"
			"[pwrap_dump_rwspin_user]R/W(%d)[%d] 0x%x\n",
			rwspin_record.write, rwspin_record.spin_lock, rwspin_record.addr);
}

static int pwrap_wacs2_internal(struct pmic_wrapper *wrp,
	bool write, u32 adr, u32 *data)
{
	int ret = 0;
	u64 wrap_access_time = 0;
	unsigned long flags = 0;

	wrap_access_time = _pwrap_get_current_time();
	spin_lock_irqsave(&wrp->lock, flags);
	rwspin_record.addr = adr;
	rwspin_record.write = write;
	rwspin_record.spin_lock = true;
	if (wrp->suspended) {
		PWRAPERR("PMIC WRAPPER device in in suspend. Try again later\n");
		ret = -EAGAIN;
	}
	if (!ret)
		ret = pwrap_wacs2_locked(write, true, adr, data);
	rwspin_record.spin_lock = false;
	spin_unlock_irqrestore(&wrp->lock, flags);

	pwrap_trace(wrp, wrap_access_time, ret, write, adr, *data);

	/* EINVAL is indication of invalid arguments,
	 * not real transaction failure. Do not  */
	WARN_ON(ret < 0 && ret != -EINVAL);

	return ret;
}

static int pwrap_init_dio(struct pmic_wrapper *wrp, u32 dio_en)
{
	u32 arb_en_backup = 0x0;
	u32 rdata = 0x0;
	int ret;

	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x8);	/* only WACS2 */
	pwrap_write_locked(DEW_DIO_EN, dio_en);

	/* Check IDLE & INIT_DONE in advance */
	ret =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (ret < 0) {
		PWRAPERR("pwrap_init_dio fail,ret=%d\n", ret);
		return ret;
	}
	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);
	/* Read Test */
	pwrap_read_locked(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0x5aa5\n",
			 dio_en, rdata);
		return -EFAULT;
	}
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

static int pwrap_init_cipher(struct pmic_wrapper *wrp)
{
	int ret;
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	u32 start_time_ns = 0, timeout_ns = 0;

	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x8);	/* only WACS0 */

	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL, 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_LOAD, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_START, 1);

	/* Config CIPHER @ PMIC */
	pwrap_write_locked(DEW_CIPHER_SWRST, 0x1);
	pwrap_write_locked(DEW_CIPHER_SWRST, 0x0);
	pwrap_write_locked(DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_locked(DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write_locked(DEW_CIPHER_LOAD, 0x1);
	pwrap_write_locked(DEW_CIPHER_START, 0x1);

	/* wait for cipher data ready@AP */
	ret =
	    wait_for_state_ready_init(wait_for_cipher_ready, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_CIPHER_RDY, 0);
	if (ret < 0) {
		PWRAPERR("wait for cipher data ready@AP fail,ret=%d\n", ret);
		return ret;
	}
	/* wait for cipher data ready@PMIC */
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do {
		pwrap_read_locked(DEW_CIPHER_RDY, &rdata);
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait for cipher data ready@PMIC\n");
			return -ETIMEDOUT;
		}
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write_locked(DEW_CIPHER_MODE, 0x1);
	/* wait for cipher mode idle */
	ret =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (ret < 0) {
		PWRAPERR("wait for cipher mode idle fail,ret=%d\n", ret);
		return ret;
	}
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 1);

	/* Read Test */
	pwrap_read_locked(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return -EFAULT;
	}

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

static int pwrap_init_sidly(struct pmic_wrapper *wrp)
{
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	u32 ind = 0;
	u32 result = 0;
	bool failed = false;
	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x8);	/* only WACS2 */

	/* Scan all SIDLY by Read Test */
	result = 0;
	for (ind = 0; ind < 4; ind++) {
		WRAP_WR32(PMIC_WRAP_SIDLY, ind);
		pwrap_read_locked(DEW_READ_TEST, &rdata);
		if (rdata == DEFAULT_VALUE_READ_TEST) {
			PWRAPLOG("pwrap_init_sidly [Read Test] pass,SIDLY=%x rdata=%x\n", ind,
				 rdata);
			result |= (0x1 << ind);
		} else
			PWRAPERR("pwrap_init_sidly [Read Test] fail,SIDLY=%x,rdata=%x\n", ind,
				 rdata);
	}

	/* Config SIDLY according to result */
	switch (result) {
		/* Only 1 pass, choose it */
	case 0x1:
		WRAP_WR32(PMIC_WRAP_SIDLY, 0);
		break;
	case 0x2:
		WRAP_WR32(PMIC_WRAP_SIDLY, 1);
		break;
	case 0x4:
		WRAP_WR32(PMIC_WRAP_SIDLY, 2);
		break;
	case 0x8:
		WRAP_WR32(PMIC_WRAP_SIDLY, 3);
		break;

		/* two pass, choose the one on SIDLY boundary */
	case 0x3:
		WRAP_WR32(PMIC_WRAP_SIDLY, 0);
		break;
	case 0x6:
		WRAP_WR32(PMIC_WRAP_SIDLY, 1);	/* no boundary, choose smaller one */
		break;
	case 0xc:
		WRAP_WR32(PMIC_WRAP_SIDLY, 3);
		break;

		/* three pass, choose the middle one */
	case 0x7:
		WRAP_WR32(PMIC_WRAP_SIDLY, 1);
		break;
	case 0xe:
		WRAP_WR32(PMIC_WRAP_SIDLY, 2);
		break;
		/* four pass, choose the smaller middle one */
	case 0xf:
		WRAP_WR32(PMIC_WRAP_SIDLY, 1);
		break;

		/* pass range not continuous, should not happen */
	default:
		WRAP_WR32(PMIC_WRAP_SIDLY, 0);
		failed = true;
		break;
	}

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	if (!failed)
		return 0;
	else {
		PWRAPERR("error,pwrap_init_sidly fail,result=%x\n", result);
		return -EIO;
	}
}

int pwrap_reset_spislv(struct pmic_wrapper *wrp)
{
	int ret;
	/* PWRAPFUC(); */
	/* This driver does not using _pwrap_switch_mux */
	/* because the remaining requests are expected to fail anyway */

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, 1);
	WRAP_WR32(PMIC_WRAP_MAN_EN, 1);
	WRAP_WR32(PMIC_WRAP_DIO_EN, 0);

	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSL << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));	/* to reset counter */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSH << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));

	ret = wait_for_state_ready_init(wait_for_sync,
		TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (ret)
		PWRAPERR("pwrap_reset_spislv fail; ret=%d\n", ret);

	WRAP_WR32(PMIC_WRAP_MAN_EN, 0);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, 0);

	return ret;
}

static int pwrap_init_reg_clock(struct pmic_wrapper *wrp, u32 regck_sel)
{
	u32 wdata = 0;
	u32 rdata = 0;

	/* Set reg clk freq */
	pwrap_read_locked(PMIC_TOP_CKCON2, &rdata);

	if (regck_sel == 1)
		wdata = (rdata & (~(0x3 << 10))) | (0x1 << 10);
	else
		wdata = rdata & (~(0x3 << 10));

	pwrap_write_locked(PMIC_TOP_CKCON2, wdata);
	pwrap_read_locked(PMIC_TOP_CKCON2, &rdata);
	if (rdata != wdata) {
		PWRAPERR("pwrap_init_reg_clock,PMIC_TOP_CKCON2 Write [15]=1 Fail, rdata=%x\n",
			 rdata);
		return -EFAULT;
	}
	/* Config SPI Waveform according to reg clk */
	if (regck_sel == 1) {	/* 18MHz */
		WRAP_WR32(PMIC_WRAP_CSHEXT, 0xc);
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x4);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0xc);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0x0);
	} else if (regck_sel == 2) {	/* 36MHz */
		WRAP_WR32(PMIC_WRAP_CSHEXT, 0x4);
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x0);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0x4);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0x0);
	} else {		/* Safe mode */
		WRAP_WR32(PMIC_WRAP_CSHEXT, 0xf);
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0xf);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START, 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END, 0xf);
	}

	return 0;
}

/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_interrupt(int irqno, void *dev_id)
{
	unsigned long flags;
	struct pmic_wrapper *wrp = dev_id;
	u32 reg_int_flg = 0;

	PWRAPERR("PMIC WRAP device failed; crush dump is below\n");

	pwrap_dump_all_register(wrp);
	pwrap_trace_wacs2(wrp);

	spin_lock_irqsave(&wrp->lock, flags);
	reg_int_flg = WRAP_RD32(PMIC_WRAP_INT_FLG);

	/* raise the priority of WACS2 for AP */
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, 1 << 3);
	/* clear interrupt flag */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);

	/* disable interrupt: empty event */
	if((reg_int_flg & PWRAP_INT_EVENT_EMPTY) != 0) {
		WRAP_WR32(PMIC_WRAP_INT_EN, ~(PWRAP_INT_DEBUG | PWRAP_INT_SIG_ERR | PWRAP_INT_EVENT_EMPTY));
		PWRAPERR("SET PMIC_WRAP_INT_EN: 0x%x\n", WRAP_RD32(PMIC_WRAP_INT_EN));
	}
	spin_unlock_irqrestore(&wrp->lock, flags);

	WARN_ON(1);

	return IRQ_HANDLED;
}

int pwrap_init(struct pmic_wrapper *wrp)
{
	int ret = -EFAULT;
	u32 rdata = 0x0;
	unsigned long flags;
	/* u32 timeout=0; */
	PWRAPFUC();

	/* Reset related modules */
	/* PMIC_WRAP, PERI_PWRAP_BRIDGE, PWRAP_SPICTL */
	/* Subject to project */
	spin_lock_irqsave(&wrp->lock, flags);
	WRAP_SET_BIT(0x80, INFRA_GLOBALCON_RST0);
	WRAP_SET_BIT(0x04, PERI_GLOBALCON_RST1);
	rdata = WRAP_RD32(WDT_SWSYSRST);
	WRAP_WR32(WDT_SWSYSRST, (0x88000000 | ((rdata | (0x1 << 11)) & 0x00ffffff)));

	WRAP_CLR_BIT(0x80, INFRA_GLOBALCON_RST0);
	WRAP_CLR_BIT(0x04, PERI_GLOBALCON_RST1);
	rdata = WRAP_RD32(WDT_SWSYSRST);
	WRAP_WR32(WDT_SWSYSRST, (0x88000000 | ((rdata & (~(0x1 << 11))) & 0x00ffffff)));

	/* TBD: Set SPI_CK freq = 26MHz */
	rdata = WRAP_RD32(CLK_CFG_8);
	WRAP_WR32(CLK_CFG_8, (rdata & ~0x7) | 0x0);

	/* Enable DCM */
	WRAP_WR32(PMIC_WRAP_DCM_EN, 1);
	WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, 0);

	/* Reset SPISLV */
	if (pwrap_reset_spislv(wrp) < 0)
		goto done_unlock;

	/* Enable WACS2 */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 1);	/* enable wrap */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 8);	/* Only WACS2 */
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 1);

	/* TBD: Set SPI_CK freq = 66MHz */
	/* WRAP_WR32(CLK_CFG_8, 5); */
	rdata = WRAP_RD32(CLK_CFG_8);
	/* WRAP_WR32(CLK_CFG_8, (rdata & ~0x7) | 0x5); */
	WRAP_WR32(CLK_CFG_8, (rdata & ~0x7) | 0x0);	/* use 26MHz by default */

	/* SIDLY setting */
	if (pwrap_init_sidly(wrp))
		goto done_unlock;
	/* SPI Waveform Configuration */
	/* 0:safe mode, 1:18MHz, 2:36MHz //2 */
	if (pwrap_init_reg_clock(wrp, 2))
		goto done_unlock;

	/* Enable PMIC */
	/* (May not be necessary, depending on S/W partition) */
	if (pwrap_write_locked(PMIC_WRP_CKPDN, 0) || /* set dewrap clock bit */
		pwrap_write_locked(PMIC_WRP_RST_CON, 0))	{/* clear dewrap reset bit */
		PWRAPERR("Enable PMIC fail\n");
		goto done_unlock;
	}

	/* Enable DIO mode */
	if (pwrap_init_dio(wrp, 1))
		goto done_unlock;

	/* Enable Encryption */
	if (pwrap_init_cipher(wrp))
		goto done_unlock;

	/* Write test using WACS2 */
	if (pwrap_write_locked(DEW_WRITE_TEST, WRITE_TEST_VALUE) ||
		pwrap_read_locked(DEW_WRITE_TEST, &rdata) ||
		(rdata != WRITE_TEST_VALUE)) {
		PWRAPERR("write test error,rdata=0x%04X,exp=0x%04X\n",
			rdata, WRITE_TEST_VALUE);
		goto done_unlock;
	}
	/* Signature Checking - Using CRC */
	/* should be the last to modify WRITE_TEST */
	if (pwrap_write_locked(DEW_CRC_EN, 0x1))
		goto done_unlock;

	WRAP_WR32(PMIC_WRAP_CRC_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_CRC_VAL);

	/* PMIC_WRAP enables */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x1ff);
	WRAP_WR32(PMIC_WRAP_RRARB_EN, 0x7);
	WRAP_WR32(PMIC_WRAP_WACS0_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_WACS1_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x1);	/* already enabled */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x5);	/* 0x1:20us,for concurrence test,MP:0x5;  //100us */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xff);
	WRAP_WR32(PMIC_WRAP_WDT_UNIT, 0xf);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_TIMER_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_INT_EN, ~(PWRAP_INT_DEBUG | PWRAP_INT_SIG_ERR));

	/* switch event pin from usbdl mode to normal mode for pmic interrupt, NEW@MT6397 */
	if (pwrap_read_locked(PMIC_TOP_CKCON3, &rdata) ||
		pwrap_write_locked(PMIC_TOP_CKCON3, (rdata & 0x0007))) {
		PWRAPERR("%s: switch event pin fail\n", __func__);
		goto done_unlock;
	}

	/* PMIC_WRAP enables for event  */
	WRAP_WR32(PMIC_WRAP_EVENT_IN_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_EVENT_DST_EN, 0xffff);

	/* PERI_PWRAP_BRIDGE enables */
	WRAP_WR32(PERI_PWRAP_BRIDGE_IORD_ARB_EN, 0x7f);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WACS3_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WACS4_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_UNIT, 0xf);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_SRC_EN, 0xffff);
	WRAP_WR32(PERI_PWRAP_BRIDGE_TIMER_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_INT_EN, 0x7ff);

	/* PMIC_DEWRAP enables */
	if (pwrap_write_locked(DEW_EVENT_OUT_EN, 0x1) ||
		pwrap_write_locked(DEW_EVENT_SRC_EN, 0xffff)) {
		PWRAPERR("enable dewrap fail\n");
		goto done_unlock;
	}

	/* Initialization Done */
	WRAP_WR32(PMIC_WRAP_INIT_DONE2, 0x1);

	/* TBD: Should be configured by MD MCU */
#if 1 /* def CONFIG_MTK_LDVT_PMIC_WRAP */
	WRAP_WR32(PMIC_WRAP_INIT_DONE0, 1);
	WRAP_WR32(PMIC_WRAP_INIT_DONE1, 1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_INIT_DONE3, 1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_INIT_DONE4, 1);
#endif
	ret = 0;
done_unlock:
	spin_unlock_irqrestore(&wrp->lock, flags);
	return ret;
}

static void pwrap_print_trace_data(struct pmic_wrapper *wrp, int idx)
{
	struct pwrap_debug_data data;
	unsigned long flags;
	if (idx < 0 || idx >= PWRAP_DEBUG_COUNT)
		return;
	spin_lock_irqsave(&wrp->trace_lock, flags);
	data = wrp->pwrap_debug_data[idx];
	spin_unlock_irqrestore(&wrp->trace_lock, flags);
	PWRAPREG("idx=%d,time=(%lld),op=%d,result=%d,addr=%x,data=%x\n",
		idx, data.wacs_time,
		data.operation, data.result, data.addr, data.data);
}

static void pwrap_trace_wacs2(struct pmic_wrapper *wrp)
{
	unsigned long flags;
	int idx, i;
	/* print the latest access of pmic */
	PWRAPREG("the latest %d access of pmic is following.\n", PWRAP_DEBUG_COUNT);
	spin_lock_irqsave(&wrp->trace_lock, flags);
	idx = wrp->pwrap_debug_index;
	spin_unlock_irqrestore(&wrp->trace_lock, flags);
	for (i = 0; i < PWRAP_DEBUG_COUNT; ++i) {
		pwrap_print_trace_data(wrp, idx);
		idx++;
		if (idx == PWRAP_DEBUG_COUNT)
			idx = 0;
	}
}

static void pwrap_read_reg_on_ap(u32 reg_addr)
{
	u32 reg_value = 0;
	reg_value = WRAP_RD32(reg_addr);
	PWRAPREG("0x%04X: 0x%04X\n", reg_addr, reg_value);
}

static void pwrap_write_reg_on_ap(u32 reg_addr, u32 reg_value)
{
	PWRAPREG("write 0x%x to register 0x%x\n", reg_value, reg_addr);
	WRAP_WR32(reg_addr, reg_value);
	reg_value = WRAP_RD32(reg_addr);
	PWRAPREG("result: 0x%04X: 0x%04X\n", reg_addr, reg_value);
}

static void pwrap_read_reg_on_pmic(u32 reg_addr)
{
	u32 reg_value = 0;
	u32 ret = 0;
	/* PWRAPFUC(); */
	ret = pwrap_read(reg_addr, &reg_value);
	PWRAPREG("0x%04X: 0x%04X,ret=%d\n", reg_addr, reg_value, ret);
}

static void pwrap_write_reg_on_pmic(u32 reg_addr, u32 reg_value)
{
	u32 ret = 0;
	PWRAPREG("write 0x%x to register 0x%x\n", reg_value, reg_addr);
	ret = pwrap_write(reg_addr, reg_value);
	ret = pwrap_read(reg_addr, &reg_value);
	/* PWRAPFUC(); */
	PWRAPREG("the result:0x%04X: 0x%04X,ret=%d\n", reg_addr, reg_value, ret);
}


static int pwrap_read_test(void)
{
	u32 rdata = 0;
	/* Read Test */
	pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPREG("Read Test fail,rdata=0x%04X, exp=0x%04X\n",
			rdata, DEFAULT_VALUE_READ_TEST);
		return -EFAULT;
	}
	PWRAPREG("Read Test pass\n");
	return 0;
}

static int pwrap_write_test(void)
{
	u32 rdata;
	/* Write test using WACS2 */
	if (pwrap_write(DEW_WRITE_TEST, WRITE_TEST_VALUE) ||
		pwrap_read(DEW_WRITE_TEST, &rdata) ||
		rdata != WRITE_TEST_VALUE) {
		PWRAPREG
		    ("write test error,rdata=0x%04X, exp=0x%04X\n",
		     rdata, WRITE_TEST_VALUE);
		return -EFAULT;
	}
	PWRAPREG("write Test pass\n");
	return 0;
}

#define WRAP_ACCESS_TEST_REG DEW_CIPHER_IV1
static void pwrap_wacs2_para_test(void)
{
	int ret;
	u32 result = 0;
	u32 rdata;
	/* test 2nd parameter-------------------------------------------- */
	ret = pwrap_wacs2(0, 0xffff + 0x10, 0x1234, &rdata);
	if (ret < 0) {
		PWRAPREG("pwrap_wacs2_para_test 2nd para,ret=%d\n", ret);
		result += 1;
	}
	if (result == 1)
		PWRAPREG("pwrap_wacs2_para_test pass\n");
	else
		PWRAPREG("pwrap_wacs2_para_test fail\n");
}

static void pwrap_ut(u32 ut_test)
{
	switch (ut_test) {
	case 1:
		pwrap_wacs2_para_test();
		break;
	case 2:
		/* pwrap_wacs2_para_test(); */
		break;

	default:
		PWRAPREG("default test.\n");
		break;
	}
	return;
}

static int pwrap_suspend_internal(struct pmic_wrapper *wrp)
{
	unsigned long flags;
	spin_lock_irqsave(&wrp->lock, flags);
	wrp->suspended = true;
	spin_unlock_irqrestore(&wrp->lock, flags);
	return 0;
}

static void pwrap_resume_internal(struct pmic_wrapper *wrp)
{
	unsigned long flags;
	/* PWRAPLOG("pwrap_resume\n"); */
	/* Reset related modules */
	/* PERI_PWRAP_BRIDGE */
	/* Subject to project */
	spin_lock_irqsave(&wrp->lock, flags);
	WRAP_SET_BIT(0x04, PERI_GLOBALCON_RST1);
	WRAP_CLR_BIT(0x04, PERI_GLOBALCON_RST1);

	/* PERI_PWRAP_BRIDGE enables */
	WRAP_WR32(PERI_PWRAP_BRIDGE_IORD_ARB_EN, 0x7f);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WACS3_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WACS4_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_UNIT, 0xf);
	WRAP_WR32(PERI_PWRAP_BRIDGE_WDT_SRC_EN, 0xffff);
	WRAP_WR32(PERI_PWRAP_BRIDGE_TIMER_EN, 0x1);
	WRAP_WR32(PERI_PWRAP_BRIDGE_INT_EN, 0x7ff);
	wrp->suspended = false;
	spin_unlock_irqrestore(&wrp->lock, flags);
}

static int mt_pwrap_check_and_init(struct pmic_wrapper *wrp)
{
	int ret;
	u32 rdata;
	unsigned long flags;

	/* TBD: Set SPI_CK freq = 26MHz */
	spin_lock_irqsave(&wrp->lock, flags);
	rdata = WRAP_RD32(CLK_CFG_8);
	WRAP_WR32(CLK_CFG_8, (rdata & ~0x7) | 0x0);

	rdata = WRAP_RD32(PMIC_WRAP_INIT_DONE2);
	spin_unlock_irqrestore(&wrp->lock, flags);

	if (rdata)
		return 0;

	pr_notice("PMIC wrapper HW init start\n");
	ret = pwrap_init(wrp);

	if (ret < 0) {
		pr_err("PMIC wrapper init error (%d)\n", ret);
		pwrap_dump_all_register(wrp);
		return ret;
	}
	pr_notice("PMIC wrapper HW init done\n");
	return 0;
}

int pwrap_wacs2(bool write, u32 adr, u32 wdata, u32 *rdata)
{
	int ret = 0;
	struct pmic_wrapper *wrp = g_wrp;
	u32 data = wdata;

	if (!wrp) {
		PWRAPERR("PMIC WRAPPER device does not exist\n");
		ret = -EIO;
	} else
		ret = pwrap_wacs2_internal(wrp, write, adr, &data);

	if (!ret && !write && rdata)
		*rdata = data;

	return ret;
}
EXPORT_SYMBOL(pwrap_wacs2);

int pwrap_read(u32 adr, u32 *rdata)
{
	return pwrap_wacs2(PWRAP_READ, adr, 0, rdata);
}
EXPORT_SYMBOL(pwrap_read);

int pwrap_write(u32 adr, u32 wdata)
{
	return pwrap_wacs2(PWRAP_WRITE, adr, wdata, 0);
}
EXPORT_SYMBOL(pwrap_write);

static ssize_t mt_pwrap_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct pmic_wrapper *wrp = dev_get_drvdata(dev);
	u32 reg_value = 0;
	u32 reg_addr = 0;
	u32 ret = 0;
	u32 ut_test = 0;
	if (!strncmp(buf, "-h", 2)) {
		PWRAPREG("PWRAP debug:\n"
			"\t[-dump_reg][-trace_wacs2][-init][-rdap]\n"
			"\t[-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	} else if (!strncmp(buf, "-dump_reg", 9))
		pwrap_dump_all_register(wrp);
	else if (!strncmp(buf, "-trace_wacs2", 12))
		pwrap_trace_wacs2(wrp);
	else if (!strncmp(buf, "-init", 5)) {
		ret = pwrap_init(wrp);
		if (ret == 0)
			PWRAPREG("pwrap_init pass,ret=%d\n", ret);
		else
			PWRAPREG("pwrap_init fail,ret=%d\n", ret);
	} else if (!strncmp(buf, "-rdap", 5) &&
		(1 == sscanf(buf + 5, "%x", &reg_addr)))
		pwrap_read_reg_on_ap(reg_addr);
	else if (!strncmp(buf, "-wrap", 5)
		   && (2 == sscanf(buf + 5, "%x %x", &reg_addr, &reg_value)))
		pwrap_write_reg_on_ap(reg_addr, reg_value);
	else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf + 7, "%x", &reg_addr)))
		pwrap_read_reg_on_pmic(reg_addr);
	else if (!strncmp(buf, "-wrpmic", 7)
		   && (2 == sscanf(buf + 7, "%x %x", &reg_addr, &reg_value)))
		pwrap_write_reg_on_pmic(reg_addr, reg_value);
	else if (!strncmp(buf, "-readtest", 9))
		pwrap_read_test();
	else if (!strncmp(buf, "-writetest", 10))
		pwrap_write_test();
	else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf + 3, "%d", &ut_test))) {
		pwrap_ut(ut_test);
	} else
		PWRAPREG("wrong parameter\n");
	return count;
}

DEVICE_ATTR(pwrap, S_IWUSR | S_IWGRP, NULL, mt_pwrap_store);

static int pwrap_suspend(void)
{
	return g_wrp ? pwrap_suspend_internal(g_wrp) : 0;
}

static void pwrap_resume(void)
{
	if (g_wrp)
		pwrap_resume_internal(g_wrp);
}

static struct syscore_ops pwrap_syscore_ops = {
	.resume = pwrap_resume,
	.suspend = pwrap_suspend,
};

static int mt_pmic_wrap_probe(struct platform_device *pdev)
{
	int ret;
	u32 rdata;
	unsigned long flags;
	struct pmic_wrapper *wrp;

	if (g_wrp) {
		pr_err("%s: PMIC wrapper already exists; ignored\n", __func__);
		return -EEXIST;
	}

	wrp = &mt_wrp;

	spin_lock_init(&wrp->lock);
	spin_lock_init(&wrp->trace_lock);
	wrp->pdev = pdev;
	dev_set_drvdata(&pdev->dev, wrp);

	pr_notice("MT%04X device PMIC wrapper init\n",
		(u32)pdev->id_entry->driver_data);

	if (mt_pwrap_check_and_init(wrp)) {
		pr_err("PMIC wrapper HW init failed\n");
		ret = -EIO;
		/* gating eint/i2c/pwm/kpd clock@PMIC */
		/* disable clock,except kpd(bit[4] kpd and bit[6] 32k); */

		spin_lock_irqsave(&wrp->lock, flags);
		pwrap_read_locked(PMIC_WRP_CKPDN, &rdata);
		pwrap_write_locked(PMIC_WRP_CKPDN,  (rdata | 0x2F)); /* set dewrap clock bit */
		spin_unlock_irqrestore(&wrp->lock, flags);
		goto err_out;
	}

	ret = request_threaded_irq(MT_PMIC_WRAP_IRQ_ID, mt_pmic_wrap_interrupt,
		NULL, IRQF_TRIGGER_HIGH, PMIC_WRAP_DEVICE, wrp);
	if (ret) {
		PWRAPERR("register IRQ failed (%d)\n", ret);
		goto err_out;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_pwrap);
	if (ret)
		pr_err("Fail to create PMIC wrapper sysfs files; ret=%d\n", ret);

	/* PWRAPLOG("pwrap_init_ops\n"); */
	register_syscore_ops(&pwrap_syscore_ops);

	if (!ret)
		g_wrp = wrp;

err_out:
	return ret;
}

struct platform_device_id pmic_device_id[] = {
	{ .name = "mt8135-pwrap", .driver_data = 0x8135 },
	{}
};

static struct platform_driver mt_pmic_wrap_drv = {
	.driver = {
		.name = PMIC_WRAP_DEVICE,
		.owner = THIS_MODULE,
	},
	.probe = mt_pmic_wrap_probe,
	.id_table = pmic_device_id,
};

static int __init mt_pwrap_drv_init(void)
{
	return platform_driver_register(&mt_pmic_wrap_drv);
}
postcore_initcall(mt_pwrap_drv_init);

MODULE_AUTHOR("Ranran Lu <ranran.lu@mediatek.com>");
MODULE_DESCRIPTION("pmic_wrapper Driver");
MODULE_LICENSE("GPL");
