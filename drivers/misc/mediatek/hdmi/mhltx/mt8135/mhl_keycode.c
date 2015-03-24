#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>
#include <mach/mt_pmic_wrap.h>

#include "mhl_keycode.h"
#include "mhl_dbg.h"
/*************************************RCP function report added by garyyuan*********************************/
static struct input_dev *kpd_input_dev;

void mhl_init_rmt_input_dev(void)
{
	int ret;
	MHL_CBUS_LOG("%s:%d\n", __func__, __LINE__);
	kpd_input_dev = input_allocate_device();
	if (!kpd_input_dev) {
		pr_info("Error!!!failed to allocate input device, no memory!!!%s:%d:.....\n",
			__func__, __LINE__);
		return /*-ENOMEM*/;
	}

	kpd_input_dev->name = "mhl-keyboard";
	set_bit(EV_KEY, kpd_input_dev->evbit);
	set_bit(KEY_SELECT, kpd_input_dev->keybit);
	set_bit(KEY_UP, kpd_input_dev->keybit);
	set_bit(KEY_DOWN, kpd_input_dev->keybit);
	set_bit(KEY_LEFT, kpd_input_dev->keybit);
	set_bit(KEY_RIGHT, kpd_input_dev->keybit);
	/* set_bit(KEY_RIGHT_UP, kpd_input_dev->keybit); */
	/* set_bit(KEY_RIGHT_DOWN , kpd_input_dev->keybit); */
	/* set_bit(KEY_LEFT_UP,kpd_input_dev->keybit); */
	/* set_bit(KEY_LEFT_DOWN, kpd_input_dev->keybit); */

	set_bit(KEY_MENU, kpd_input_dev->keybit);
	/* set_bit(KEY_SETUP, kpd_input_dev->keybit); */

	set_bit(KEY_EXIT, kpd_input_dev->keybit);

	/* set_bit(KEY_CONTEXT_MENU ,kpd_input_dev->keybit); */
	/* set_bit(KEY_FAVORITES, kpd_input_dev->keybit); */
	/* set_bit(KEY_EXIT, kpd_input_dev->keybit); */


	set_bit(KEY_0, kpd_input_dev->keybit);
	set_bit(KEY_1, kpd_input_dev->keybit);
	set_bit(KEY_2, kpd_input_dev->keybit);
	set_bit(KEY_3, kpd_input_dev->keybit);
	set_bit(KEY_4, kpd_input_dev->keybit);
	set_bit(KEY_5, kpd_input_dev->keybit);
	set_bit(KEY_6, kpd_input_dev->keybit);
	set_bit(KEY_7, kpd_input_dev->keybit);
	set_bit(KEY_8, kpd_input_dev->keybit);
	set_bit(KEY_9, kpd_input_dev->keybit);

	set_bit(KEY_DOT, kpd_input_dev->keybit);
	set_bit(KEY_ENTER, kpd_input_dev->keybit);
	set_bit(KEY_CLEAR, kpd_input_dev->keybit);
	/* set_bit(KEY_CHANNELUP, kpd_input_dev->keybit); */
	/* set_bit(KEY_CHANNELDOWN, kpd_input_dev->keybit); */
	/* set_bit(KEY_CHANNEL_PREV, kpd_input_dev->keybit); */

	set_bit(KEY_SOUND, kpd_input_dev->keybit);
	/* set_bit(KEY_INFO, kpd_input_dev->keybit); */
	/* set_bit(KEY_HELP, kpd_input_dev->keybit); */
	/* set_bit(KEY_PAGEUP,kpd_input_dev->keybit); */
	/* set_bit(KEY_PAGEDOWN, kpd_input_dev->keybit); */
	set_bit(KEY_VOLUMEUP, kpd_input_dev->keybit);
	set_bit(KEY_VOLUMEDOWN, kpd_input_dev->keybit);
	set_bit(KEY_MUTE, kpd_input_dev->keybit);

	set_bit(KEY_PLAY, kpd_input_dev->keybit);
	set_bit(KEY_STOP, kpd_input_dev->keybit);
	/* set_bit(KEY_PAUSE, kpd_input_dev->keybit); */
	set_bit(KEY_PAUSECD, kpd_input_dev->keybit);

	/* set_bit(KEY_RECORD,kpd_input_dev->keybit); */

	set_bit(KEY_REWIND, kpd_input_dev->keybit);
	set_bit(KEY_FASTFORWARD, kpd_input_dev->keybit);
	set_bit(KEY_EJECTCD, kpd_input_dev->keybit);
	/* set_bit(KEY_FORWARD, kpd_input_dev->keybit); */
	set_bit(KEY_NEXTSONG, kpd_input_dev->keybit);
	set_bit(KEY_BACK, kpd_input_dev->keybit);
	set_bit(KEY_PREVIOUSSONG, kpd_input_dev->keybit);

	/* set_bit(KEY_ANGLE, kpd_input_dev->keybit); */
	/* set_bit(KEY_RESTART, kpd_input_dev->keybit); */
	set_bit(KEY_PLAYPAUSE, kpd_input_dev->keybit);

	ret = input_register_device(kpd_input_dev);
	if (ret) {
		MHL_CBUS_LOG("Error!!!failed to register input device %s:%d\n", __func__,
			     __LINE__);
		input_free_device(kpd_input_dev);
	}
}

/* input key conversion */
void input_report_rcp_key(unsigned char rcp_keycode, int up_down)
{
	rcp_keycode &= 0x7F;

	MHL_CBUS_LOG("updown = %d\n", up_down);
	switch (rcp_keycode) {
	case MHL_RCP_CMD_SELECT:	/* error */
		input_report_key(kpd_input_dev, KEY_SELECT, up_down);
		MHL_CBUS_LOG("Select received\n");
		break;
	case MHL_RCP_CMD_UP:
		input_report_key(kpd_input_dev, KEY_UP, up_down);
		MHL_CBUS_LOG("Up received\n");
		break;
	case MHL_RCP_CMD_DOWN:
		input_report_key(kpd_input_dev, KEY_DOWN, up_down);
		MHL_CBUS_LOG("Down received\n");
		break;
	case MHL_RCP_CMD_LEFT:
		input_report_key(kpd_input_dev, KEY_LEFT, up_down);
		MHL_CBUS_LOG("Left received\n");
		break;
	case MHL_RCP_CMD_RIGHT:
		input_report_key(kpd_input_dev, KEY_RIGHT, up_down);
		MHL_CBUS_LOG("Right received\n");
		break;
	case MHL_RCP_CMD_ROOT_MENU:
		input_report_key(kpd_input_dev, KEY_MENU, up_down);
		MHL_CBUS_LOG("Root Menu received\n");
		break;
	case MHL_RCP_CMD_EXIT:
		input_report_key(kpd_input_dev, KEY_BACK, up_down);
		MHL_CBUS_LOG("Exit received\n");
		break;
	case MHL_RCP_CMD_NUM_0:
		input_report_key(kpd_input_dev, KEY_0, up_down);
		MHL_CBUS_LOG("Number 0 received\n");
		break;
	case MHL_RCP_CMD_NUM_1:
		input_report_key(kpd_input_dev, KEY_1, up_down);
		MHL_CBUS_LOG("Number 1 received\n");
		break;
	case MHL_RCP_CMD_NUM_2:
		input_report_key(kpd_input_dev, KEY_2, up_down);
		MHL_CBUS_LOG("Number 2 received\n");
		break;
	case MHL_RCP_CMD_NUM_3:
		input_report_key(kpd_input_dev, KEY_3, up_down);
		MHL_CBUS_LOG("Number 3 received\n");
		break;
	case MHL_RCP_CMD_NUM_4:
		input_report_key(kpd_input_dev, KEY_4, up_down);
		MHL_CBUS_LOG("Number 4 received\n");
		break;
	case MHL_RCP_CMD_NUM_5:
		input_report_key(kpd_input_dev, KEY_5, up_down);
		MHL_CBUS_LOG("Number 5 received\n");
		break;
	case MHL_RCP_CMD_NUM_6:
		input_report_key(kpd_input_dev, KEY_6, up_down);
		MHL_CBUS_LOG("Number 6 received\n");
		break;
	case MHL_RCP_CMD_NUM_7:
		input_report_key(kpd_input_dev, KEY_7, up_down);
		MHL_CBUS_LOG("Number 7 received\n");
		break;
	case MHL_RCP_CMD_NUM_8:
		input_report_key(kpd_input_dev, KEY_8, up_down);
		MHL_CBUS_LOG("Number 8 received\n");
		break;
	case MHL_RCP_CMD_NUM_9:
		input_report_key(kpd_input_dev, KEY_9, up_down);
		MHL_CBUS_LOG("Number 9 received\n");
		break;
	case MHL_RCP_CMD_DOT:
		input_report_key(kpd_input_dev, KEY_DOT, up_down);
		MHL_CBUS_LOG("Dot received\n");
		break;
	case MHL_RCP_CMD_ENTER:
		input_report_key(kpd_input_dev, KEY_ENTER, up_down);
		MHL_CBUS_LOG("Enter received\n");
		break;
	case MHL_RCP_CMD_CLEAR:
		input_report_key(kpd_input_dev, KEY_CLEAR, up_down);
		MHL_CBUS_LOG("Clear received\n");
		break;
	case MHL_RCP_CMD_SOUND_SELECT:
		input_report_key(kpd_input_dev, KEY_SOUND, up_down);
		MHL_CBUS_LOG("Sound Select received\n");
		break;
	case MHL_RCP_CMD_PLAY:
		input_report_key(kpd_input_dev, KEY_PLAY, up_down);
		MHL_CBUS_LOG("Play received\n");
		break;
	case MHL_RCP_CMD_PAUSE:
		/* input_report_key(kpd_input_dev, KEY_PAUSE, up_down); */
		input_report_key(kpd_input_dev, KEY_PAUSECD, up_down);

		MHL_CBUS_LOG("Pause received\n");
		break;
	case MHL_RCP_CMD_STOP:
		input_report_key(kpd_input_dev, KEY_STOP, up_down);
		MHL_CBUS_LOG("Stop received\n");
		break;
	case MHL_RCP_CMD_FAST_FWD:
		input_report_key(kpd_input_dev, KEY_FASTFORWARD, up_down);
		input_report_key(kpd_input_dev, KEY_FASTFORWARD, up_down);
		MHL_CBUS_LOG("Fastfwd received\n");
		break;
	case MHL_RCP_CMD_REWIND:
		input_report_key(kpd_input_dev, KEY_REWIND, up_down);
		MHL_CBUS_LOG("Rewind received\n");
		break;
	case MHL_RCP_CMD_EJECT:
		input_report_key(kpd_input_dev, KEY_EJECTCD, up_down);
		MHL_CBUS_LOG("Eject received\n");
		break;
	case MHL_RCP_CMD_FWD:
		/* input_report_key(kpd_input_dev, KEY_FORWARD, up_down);//next song */
		input_report_key(kpd_input_dev, KEY_NEXTSONG, up_down);
		MHL_CBUS_LOG("Next song received\n");
		break;
	case MHL_RCP_CMD_BKWD:
		/* input_report_key(kpd_input_dev, KEY_BACK, up_down);//previous song */
		input_report_key(kpd_input_dev, KEY_PREVIOUSSONG, up_down);
		MHL_CBUS_LOG("Previous song received\n");
		break;
	case MHL_RCP_CMD_PLAY_FUNC:
		/* input_report_key(kpd_input_dev, KEY_PL, up_down); */
		input_report_key(kpd_input_dev, KEY_PLAY, up_down);
		MHL_CBUS_LOG("Play Function received\n");
		break;
	case MHL_RCP_CMD_PAUSE_PLAY_FUNC:
		input_report_key(kpd_input_dev, KEY_PLAYPAUSE, up_down);
		MHL_CBUS_LOG("Pause_Play Function received\n");
		break;
	case MHL_RCP_CMD_STOP_FUNC:
		input_report_key(kpd_input_dev, KEY_STOP, up_down);
		MHL_CBUS_LOG("Stop Function received\n");
		break;
	case MHL_RCP_CMD_F1:
		input_report_key(kpd_input_dev, KEY_F1, up_down);
		MHL_CBUS_LOG("F1 received\n");
		break;
	case MHL_RCP_CMD_F2:
		input_report_key(kpd_input_dev, KEY_F2, up_down);
		MHL_CBUS_LOG("F2 received\n");
		break;
	case MHL_RCP_CMD_F3:
		input_report_key(kpd_input_dev, KEY_F3, up_down);
		MHL_CBUS_LOG("F3 received\n");
		break;
	case MHL_RCP_CMD_F4:
		input_report_key(kpd_input_dev, KEY_F4, up_down);
		MHL_CBUS_LOG("F4 received\n");
		break;
	case MHL_RCP_CMD_F5:
		input_report_key(kpd_input_dev, KEY_F5, up_down);
		MHL_CBUS_LOG("F5 received\n");
		break;
	default:
		break;
	}

	/* added  for improving mhl RCP start */
	input_sync(kpd_input_dev);
	/* added  for improving mhl RCP end */
}

void input_report_mhl_rcp_key(unsigned char rcp_keycode)
{
	/* added  for improving mhl RCP start */
	input_report_rcp_key(rcp_keycode & 0x7F, 1);

	if ((MHL_RCP_CMD_FAST_FWD == rcp_keycode) || (MHL_RCP_CMD_REWIND == rcp_keycode)) {
		msleep(10);
	}
	input_report_rcp_key(rcp_keycode & 0x7F, 0);
	/* added  for improving mhl RCP end */
}

#endif
