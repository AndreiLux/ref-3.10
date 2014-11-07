/*============================================================================*/
/* FujiFlim OIS firmware													  */
/*============================================================================*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/poll.h>
#include "odin-ois-i2c.h"
#include "../../odin-css.h"

#define LAST_UPDATE "13-11-28, 28B"

#define GET_OLD_MODULE_ID
#define E2P_FIRST_ADDR 			(0x0710)
#define E2P_DATA_BYTE			(28)
#define CTL_END_ADDR_FOR_E2P_DL	(0x13A8)

#define OIS_START_DL_ADDR		(0xF010)
#define OIS_COMPLETE_DL_ADDR	(0xF006)
#define OIS_READ_STATUS_ADDR	(0x6024)
#define OIS_CHECK_SUM_ADDR		(0xF008)

#define LIMIT_STATUS_POLLING	(10)
#define LIMIT_OIS_ON_RETRY		(5)
#define LIMIT_GYRO_CALIBRATION  (5)

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 187

static struct ois_i2c_bin_list FF_VERX_REL_BIN_DATA =
{
	.files = 3,
	.entries =
	{
		{
			.filename = "DLdata_rev28B_data1.ecl",
			.filesize = 0x0E48,
			.blocks = 1,
			.addrs = {
				{0x0000,0x0E47,0x0000},
				}
		},
		{
			.filename = "DLdata_rev28B_data2.ecl",
			.filesize = 0x00D4,
			.blocks = 1,
			.addrs = {
				{0x0000,0x00D3,0x5400},
				}
		},
		{
			.filename = "DLdata_rev28B_data3.ecl",
			.filesize = 0x0720,
			.blocks = 1,
			.addrs = {
				{0x0000,0x071F,0x1188},
				}
		}
	},
	.checksum = 0x0005488e
};

int8_t from_signed_byte(uint8_t byte)
{
    byte &= 255;
    if (byte > 127) return byte - 256;
    else
		return byte;
}

int16_t from_signed_word(uint16_t word)
{
    word &= 65535;
    if (word > 32767) return word - 65536;
    else
		return word;
}

static struct odin_ois_fn fuji_ois_func_tbl;

static int fuji_ois_poll_ready(int limit)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	unsigned char ois_status = 0;
	int read_byte = 0;

	/* polling status ready */
	RegRead8(client,OIS_READ_STATUS_ADDR, &ois_status);
	read_byte++;

	while ((ois_status != 0x01) && (read_byte < limit)) {
		udelay(1000); /* wait 1ms */
		/* polling status ready */
		RegRead8(client,OIS_READ_STATUS_ADDR, &ois_status);
		read_byte++;
	}
	return ois_status;
}

int fuji_bin_download(struct ois_i2c_bin_list *bin_list)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	int rc = 0;
	int cnt = 0;
	int32_t read_value_32t;
	int i;
	unsigned char read_value[4] = {0, };
	unsigned short addr;

	css_info("fuji_bin_download : %s\n",bin_list->entries[0].filename);

	/* check OIS ic is alive */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) {
		css_err("fuji_bin_download: no reply 1\n");
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	/* Send command ois start dl */
	rc = RegWrite8(client,OIS_START_DL_ADDR, 0x00);

	while (rc < 0 && cnt < LIMIT_STATUS_POLLING) {
		udelay(2000);
		rc = RegWrite8(client,OIS_START_DL_ADDR, 0x00);
		cnt++;
	}

	if (rc < 0) {
		css_err("fuji_bin_download: no reply 2\n");
		rc = OIS_INIT_I2C_ERROR;
		goto END;
	}

	if (!bin_list) {
		css_err("bin_list is null\n");
		rc = OIS_FAIL;
		goto END;
	}

	/* OIS program downloading */
	rc = ois_i2c_load_and_write_bin_list(client,bin_list);
	if (rc < 0)
		goto END;

	/* Check sum value!*/
#if 0
	RamRead32(client, OIS_CHECK_SUM_ADDR , &read_value_32t );
#else
	addr = OIS_CHECK_SUM_ADDR;

	for (i = 0; i< 4 ; i++)
	{
		RamRead8(client, addr++ , &read_value[i]);
	}
	read_value_32t = ((unsigned long)read_value[0] << 24 |
		(unsigned long)read_value[1] << 16 |
		(unsigned long)read_value[2] << 8 |
		read_value[3] ) ;
#endif

	if (read_value_32t != bin_list->checksum) {
		css_err("error [0xF008]checksum = 0x%x, bin_checksum 0x%x\n",
			read_value_32t, bin_list->checksum);
		rc = OIS_INIT_CHECKSUM_ERROR;
		goto END;
	}

	rc = ois_i2c_load_and_write_e2prom_data(client, E2P_FIRST_ADDR,
		E2P_DATA_BYTE, CTL_END_ADDR_FOR_E2P_DL);
	if (rc < 0)
		goto END;

	/* Send command ois complete dl */
	RegWrite8(client,OIS_COMPLETE_DL_ADDR, 0x00) ;

	/* Read ois status */
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING)) {
		css_err("%s: no reply 3\n", __func__);
		rc = OIS_INIT_TIMEOUT;
		goto END;
	}

	css_info("%s, complete dl FINISHED! \n",__func__);

END:
	return rc;
}

int fuji_ois_write_8bytes(uint16_t addr, uint32_t data_u, uint32_t data_d)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	RegWrite8(client,addr++, 0xFF & (data_u >> 24));
	RegWrite8(client,addr++, 0xFF & (data_u >> 16));
	RegWrite8(client,addr++, 0xFF & (data_u >> 8));
	RegWrite8(client,addr++, 0xFF & (data_u));
	RegWrite8(client,addr++, 0xFF & (data_d >> 24));
	RegWrite8(client,addr++, 0xFF & (data_d >> 16));
	RegWrite8(client,addr++, 0xFF & (data_d >> 8));
	RegWrite8(client,addr  , 0xFF & (data_d));
	return OIS_SUCCESS;
}

int fuji_ois_init_cmd(int limit, int ver)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	uint16_t gyro_intercept_x,gyro_intercept_y = 0;
	int16_t gyro_signed_intercept_x,gyro_signed_intercept_y = 0;
	uint16_t gyro_slope_x,gyro_slope_y = 0;
	int16_t gyro_signed_slope_x,gyro_signed_slope_y = 0;

	int trial = 0;
	uint16_t gyro_temp = 0;
	int16_t gyro_signed_temp = 0;
	int16_t gyro_signed_offset_x = 0, gyro_signed_offset_y = 0;

	do {
		RegWrite8(client, 0x6020, 0x01);
		trial++;
	} while (trial < limit && !fuji_ois_poll_ready(LIMIT_STATUS_POLLING));

	if (trial == limit)
		return OIS_INIT_TIMEOUT; /* initialize fail */

	fuji_ois_write_8bytes(0x6080, 0x504084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x504088C3, 0xFD040000);
	fuji_ois_write_8bytes(0x6080, 0x505084C3, 0x02FC0000);
	fuji_ois_write_8bytes(0x6080, 0x505088C3, 0xFD040000);

	RegWrite8(client, 0x602C, 0x1B);
	RegWrite8(client, 0x602D, 0x00);

	/* common, cal 8, init 8 */
	switch (ver) {
	case OIS_VER_CALIBRATION:
	case OIS_VER_DEBUG:
		RegWrite8(client, 0x6023, 0x00);
		break;
	case OIS_VER_RELEASE:
	default:
		RegWrite8(client, 0x602c, 0x41);
		RamRead16(client, 0x602D, &gyro_temp);

		E2PRegRead16(client, 0x070A, &gyro_intercept_x);
		E2PRegRead16(client, 0x070C, &gyro_intercept_y);
		E2PRegRead16(client, 0x072C, &gyro_slope_x);
		E2PRegRead16(client, 0x072E, &gyro_slope_y);

		gyro_signed_slope_x = from_signed_word(gyro_slope_x);
		gyro_signed_slope_y = from_signed_word(gyro_slope_y);
		gyro_signed_temp = from_signed_word(gyro_temp);
		gyro_signed_intercept_x = from_signed_word(gyro_intercept_x);
		gyro_signed_intercept_y = from_signed_word(gyro_intercept_y);

		css_sensor("%s [0x070A]gyro_intercept_x 0x%x \n", __func__, gyro_intercept_x);
		css_sensor("%s [0x070C]gryo_intercept_y 0x%x \n", __func__, gyro_intercept_y);
		css_sensor("%s [0x072C]gyro_slope_x 0x%x \n", __func__, gyro_slope_x);
		css_sensor("%s [0x072E]gyro_slope_y 0x%x \n", __func__, gyro_slope_y);

		gyro_signed_offset_x = ((gyro_signed_slope_x * gyro_signed_temp) >> 15)
			+ gyro_signed_intercept_x;

		css_sensor("%s gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);

		RegWrite8(client, 0x609C, 0x00);
		RegWrite8/*RamWrite16*/(client, 0x609D, 0xFFFF & (uint16_t)gyro_signed_offset_x);
		RegWrite8(client, 0x609C, 0x01);

		gyro_signed_offset_y= ((gyro_signed_slope_y * gyro_signed_temp) >> 15)
			+ gyro_signed_offset_y/*gyro_signed_intercept_y*/;

		css_sensor("%s gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);

		RegWrite8/*RamWrite16*/(client, 0x609D, 0xFFFF & (uint16_t)gyro_signed_offset_y);

		RegWrite8(client, 0x6023, 0x04);
		//usleep(200000); /* wait 200ms */
		msleep(200);

		break;
	}
	return OIS_SUCCESS;
}

int fuji_ois_gyro_calibration(int ver)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	unsigned char read_value[2] = {0, };

	uint16_t gyro_offset_x = 0, gyro_offset_y = 0;
	uint16_t gyro_temp = 0;
	int16_t gyro_intercept_x,gyro_intercept_y = 0;
	uint16_t gyro_diff_x = 0, gyro_diff_y = 0;
	int16_t gyro_signed_diff_x = 0, gyro_signed_diff_y = 0;
	uint16_t gyro_slope_x = 0,gyro_slope_y = 0;
	uint16_t gyro_raw_x = 0,gyro_raw_y = 0;
	int16_t gyro_signed_differences_x = 0, gyro_signed_differences_y = 0;
	int16_t gyro_signed_slope_x = 0, gyro_signed_slope_y = 0, gyro_signed_temp = 0;
	int16_t gyro_signed_offset_x = 0, gyro_signed_offset_y = 0;
	int16_t gyro_signed_raw_x = 0, gyro_signed_raw_y = 0;

	css_info("%s start \n",__func__);

	RegWrite8(client, 0x6088, 0x00);
//	usleep(10000);
	msleep(10);
	if (!fuji_ois_poll_ready(100)) {
		css_err("%s fuji_ois_poll_ready error \n", __func__);
		return OIS_INIT_TIMEOUT;
	}

	RamRead16(client, 0x608A, &gyro_offset_x);
    css_info("%s read [0x608A]gyro_offset_x 0x%4x \n", __func__, gyro_offset_x);

	RegWrite8(client, 0x6023, 0x02);
	RegWrite8(client, 0x602c, 0x41);
	RamRead16(client, 0x602d,&gyro_temp);

    css_info("%s read [0x602d]gtemp 0x%4x \n", __func__, gyro_temp);

	RegWrite8(client, 0x6023, 0x00);
	RegWrite8(client, 0x6088, 0x01);
	/* usleep(10000);*/
	msleep(10);

	if (!fuji_ois_poll_ready(100)) {
		css_err("%s fuji_ois_poll_ready error \n", __func__);
		return OIS_INIT_TIMEOUT;
	}
	RamRead16(client, 0x608A, &gyro_offset_y); /* 21 */
	css_info("%s read [0x608A]gyroy_offset_y 0x%4x \n", __func__, gyro_offset_y);

	RegWrite8(client, 0x609C, 0x00); /* 22 */

	RamWrite16(client, 0x609D, (uint16_t)(0xFFFF & (gyro_offset_x))); /* 23 */

	RegWrite8(client, 0x609C, 0x01); /* 24 */
	RamWrite16(client, 0x609D, (uint16_t)(0xFFFF & (gyro_offset_y))); /* 25 */
	/* usleep(10000);*/
	msleep(10);

	css_info("%s write [0x609D]gyro_offset_x 0x%4x [0x609D]gyro_offset_y 0x%4x \n",
		__func__, gyro_offset_x, gyro_offset_y);

	RegWrite8(client, 0x609C, 0x02); /* 26 */
	RamRead16(client, 0x609D, &gyro_diff_x); /* 27 */

	RegWrite8(client, 0x609C, 0x03);  /* 28 */
	RamRead16(client, 0x609D, &gyro_diff_y);  /* 29 */

	gyro_signed_diff_x = from_signed_word(gyro_diff_x);
	gyro_signed_diff_y = from_signed_word(gyro_diff_y);

	/* 1dps */
	if ((abs(gyro_signed_diff_x) > 262) || (abs(gyro_signed_diff_y) > 262)) {
		css_err("Gyro Offset Diff X is FAIL!!!  %d %x\n",gyro_signed_diff_x,
			gyro_signed_diff_x);
		css_err("Gyro Offset Diff Y is FAIL!!!  %d %x\n",gyro_signed_diff_y,
			gyro_signed_diff_y);
		return OIS_FAIL;
	}

	css_info("%s read [0x609D]gyro_signed_diff_x 0x%x [0x609D]gyro_signed_diff_y 0x%x \n",
		__func__, gyro_signed_diff_x, gyro_signed_diff_y);

	E2PRegRead16(client, 0x072C, &gyro_slope_x);
	E2PRegRead16(client, 0x072E, &gyro_slope_y);

	gyro_signed_slope_x = from_signed_word(gyro_slope_x);
	gyro_signed_slope_y = from_signed_word(gyro_slope_y);
	gyro_signed_temp = from_signed_word(gyro_temp);
	gyro_signed_offset_x = from_signed_word(gyro_offset_x);
	gyro_signed_offset_y = from_signed_word(gyro_offset_y);

	css_info("%s gyro_signed_slope_x 0x%x \n", __func__, gyro_signed_slope_x);
	css_info("%s gyro_signed_slope_y 0x%x \n", __func__, gyro_signed_slope_y);
	css_info("%s gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);
	css_info("%s gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);
	css_info("%s gyro_signed_temp 0x%x \n", __func__, gyro_signed_temp);

	gyro_intercept_x =
		gyro_signed_offset_x - ((gyro_signed_slope_x * gyro_signed_temp) >> 15);
	gyro_intercept_y =
		gyro_signed_offset_y - ((gyro_signed_slope_y * gyro_signed_temp) >> 15);

	css_info("%s calc gyro_intercept_x 0x%x \n", __func__, gyro_intercept_x);
	css_info("%s calc gyro_intercept_y 0x%x \n", __func__, gyro_intercept_y);

	E2PRegWrite16(client, 0x070A, (uint16_t)(0xFFFF & gyro_intercept_x));
	/* usleep(10000);*/
	msleep(10);
	E2PRegWrite16(client, 0x070C, (uint16_t)(0xFFFF & gyro_intercept_y));
	/* usleep(10000);*/
	msleep(10);

	/*32 */
	RegWrite8(client, 0x6023, 0x02);
	RegWrite8(client, 0x602c, 0x41);
	RamRead16(client, 0x602D, &gyro_temp);
	css_info("%s read [0x602D]gyro_temp 0x%x \n", __func__, gyro_temp);

	RegWrite8(client, 0x6023, 0x00);
	gyro_signed_temp = from_signed_word(gyro_temp);
	/* 36 */
	gyro_signed_offset_x = ((gyro_signed_slope_x * gyro_signed_temp) >> 15)
		+ gyro_intercept_x;
	gyro_signed_offset_y = ((gyro_signed_slope_y * gyro_signed_temp) >> 15)
		+ gyro_intercept_y;

	css_info("%s calc gyro_signed_offset_x 0x%x \n", __func__, gyro_signed_offset_x);
	css_info("%s calc gyro_signed_offset_y 0x%x \n", __func__, gyro_signed_offset_y);

	/* 37 ,38 */
	RamRead16(client, 0x6042, &gyro_raw_x);
	RamRead16(client, 0x6044, &gyro_raw_y);
	/* 40 */

	gyro_signed_raw_x = from_signed_word(gyro_raw_x);
	gyro_signed_raw_y = from_signed_word(gyro_raw_y);

	gyro_signed_differences_x = gyro_signed_offset_x + gyro_signed_raw_x;
	gyro_signed_differences_y = gyro_signed_offset_y - gyro_signed_raw_y;

	css_info("%s calc gyro_differences_x 0x%x \n", __func__,
		gyro_signed_differences_x);
	css_info("%s calc gyro_differences_y 0x%x \n", __func__,
		gyro_signed_differences_y);

	if ((abs(gyro_signed_differences_x) > 262) ||
		(abs(gyro_signed_differences_y) > 262)) {
		css_err("Differences X is FAIL!!! %d %x\n",gyro_signed_differences_x,
			gyro_signed_differences_x);
		css_err("Differences Y is FAIL!!! %d %x\n",gyro_signed_differences_y,
			gyro_signed_differences_y);
		return OIS_FAIL;
	}

	/* 41a */
	RegWrite8(client, 0x6023, 0x02);
	/* 41 */
	RegWrite8(client, 0x609C, 0x00);
	/* 42 */
	RegWrite8/*RamWrite16*/(client, 0x609D, (uint16_t)(0xFFFF & (gyro_signed_offset_x)));
	/* 43 */
	RegWrite8(client, 0x609C, 0x01);
	/* 44 */
	RegWrite8/*RamWrite16*/(client, 0x609D, (uint16_t)(0xFFFF & (gyro_signed_offset_y)));
	/* 44a */
	RegWrite8(client, 0x6023, 0x04);

	css_info("%s end \n",__func__);
	return OIS_SUCCESS;
}

int32_t fuji_ois_mode(enum ois_mode_t data)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	int cur_mode = fuji_ois_func_tbl.ois_cur_mode;
	css_info("%s:%d\n", __func__,data);

	if (cur_mode == data)
		return 0;

	if (cur_mode != OIS_MODE_CENTERING_ONLY) {
		/* go to lens centering mode */
		RegWrite8(client, 0x6020, 0x01);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
	}

	switch(data) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
		css_info("%s:%d, %d preview capture \n", __func__,data, cur_mode);
		RegWrite8(client, 0x6021, 0x10);
		RegWrite8(client, 0x6020, 0x02);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	case OIS_MODE_VIDEO:
		css_info("%s:%d, %d capture \n", __func__,data, cur_mode);
		RegWrite8(client, 0x6021, 0x11);
		RegWrite8(client, 0x6020, 0x02);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	case OIS_MODE_CENTERING_ONLY:
		css_info("%s:%d, %d centering_only \n", __func__,data, cur_mode);
		break;
	case OIS_MODE_CENTERING_OFF:
		css_info("%s:%d, %d centering_off \n", __func__,data, cur_mode);
		RegWrite8(client, 0x6020, 0x00);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		break;
	}

	fuji_ois_func_tbl.ois_cur_mode = data;
	return 0;
}

int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y);

int32_t	fuji_ois_on (enum ois_ver_t ver)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	int32_t rc = OIS_SUCCESS;
	int retry = 0;
#ifdef GET_OLD_MODULE_ID
	uint16_t ver_module = 0;
#endif
	//printk("%s, %s\n", __func__,LAST_UPDATE);

	rc = fuji_bin_download(&FF_VERX_REL_BIN_DATA);
	if (rc < 0)	{
		css_err("%s: fuji_bin_download fail : %d \n", __func__, rc);
		return rc;
	}

	rc = fuji_ois_init_cmd(LIMIT_OIS_ON_RETRY,ver);
	if (rc < 0)	{
		css_err("%s: fuji_ois_init_cmd fail :%d \n", __func__,rc);
		return rc;
	}

	switch (ver) {
	case OIS_VER_RELEASE:
		break;
	case OIS_VER_CALIBRATION:
	case OIS_VER_DEBUG:

		/* RegWrite8(client, 0x6020, 0x01); */
		RegWrite8(client, 0x6021, 0x10);
		/* wait 50ms */
		/* usleep(50000); */
		msleep(50);
		do {
			rc = fuji_ois_gyro_calibration(ver);
		} while (rc < 0 && retry++ < LIMIT_GYRO_CALIBRATION);

		if (rc < 0) {
			css_err("%s: gyro cal fail \n", __func__);
			rc = OIS_INIT_GYRO_ADJ_FAIL; /* gyro failed. */
		}

		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
		fuji_ois_move_lens(0x00, 0x00); /* force move to center */
		break;
	}

	fuji_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

#ifdef GET_OLD_MODULE_ID
	E2PRegRead8(client, 0x0730, &ver_module);

	if (ver_module != 0x01)
	{
		css_info("%s, Old module %x, ver = %d\n", __func__, ver_module, ver);
		rc |= OIS_INIT_OLD_MODULE;
	}
	else
		css_info("New OIS module, ver = %d\n", ver);
#endif
	return rc;
}

int32_t	fuji_ois_off(void)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	int16_t i;
	css_info("%s enter\n", __func__);

	/* go to lens centering mode */
	RegWrite8(client, 0x6020, 0x01);
	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
		return OIS_INIT_TIMEOUT;

	for (i = 0x01C9; i > 0; i-= 46) {
		fuji_ois_write_8bytes(0x6080, 0x504084C3, i << 16);		 /*high limit xch */
		fuji_ois_write_8bytes(0x6080, 0x504088C3, (-i) << 16);   /* low limit xch */
		fuji_ois_write_8bytes(0x6080, 0x505084C3, i << 16);		 /* high limit ych */
		fuji_ois_write_8bytes(0x6080, 0x505088C3, (-i) << 16);	 /* low limit ych */
		/* wait 5ms */
		/* usleep(5000); */
		msleep(5);
	}

	return 0;
}

int32_t fuji_ois_stat(struct odin_ois_info *ois_stat)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	uint8_t	hall_x = 0, hall_y = 0;
	int16_t	hall_signed_x = 0, hall_signed_y = 0;
	uint16_t gyro_diff_x = 0, gyro_diff_y = 0;
	int16_t gyro_signed_diff_x = 0, gyro_signed_diff_y = 0;

	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider), "FF_ROHM");

	/* OIS ON */
	RegWrite8(client, 0x6020, 0x02);
	if (!fuji_ois_poll_ready(5))
		return OIS_INIT_TIMEOUT;

	RegWrite8(client, 0x609C, 0x02); /* 26 */
	RamRead16(client, 0x609D, &gyro_diff_x); /* 27 */

	RegWrite8(client, 0x609C, 0x03); /* 28 */
	RamRead16(client, 0x609D, &gyro_diff_y); /* 29 */

	gyro_signed_diff_x = from_signed_word(gyro_diff_x);
	gyro_signed_diff_y = from_signed_word(gyro_diff_y);

	/*                                                             */
	ois_stat->gyro[0] = (int16_t)gyro_signed_diff_x * 3;
	ois_stat->gyro[1] = (int16_t)gyro_signed_diff_y * 3;
	css_info("%s gyro_signed_diff_x(0x%x) -> 0x%x \n", __func__,
		gyro_signed_diff_x, ois_stat->gyro[0]);
	css_info("%s gyro_signed_diff_y(0x%x) -> 0x%x \n", __func__,
		gyro_signed_diff_y, ois_stat->gyro[1]);

	RegRead8(client,0x6058, &hall_x);
	RegRead8(client,0x6059, &hall_y);

	/* change to unsigned factor */
	hall_signed_x = from_signed_byte(hall_x);

	/* change to unsigned factor */
	hall_signed_y = from_signed_byte(hall_y);

	/* 10 LSB, max = 10 * 262(up scale)  */
	ois_stat->hall[0] = (-1) * hall_signed_x * 262;
	ois_stat->hall[1] = (-1) * hall_signed_y * 262;

	css_info("%s [0x6058]hall_x 0x%x->0x%x\n", __func__, hall_x, ois_stat->hall[0]);
	css_info("%s [0x6059]hall_y 0x%x->0x%x\n", __func__, hall_y, ois_stat->hall[1]);

	ois_stat->is_stable = 1;

	/* 3 dsp */
	if ((abs(gyro_signed_diff_x) > (262 * 3)) ||
		(abs(gyro_signed_diff_y) > (262 * 3))) {
		css_err("Gyro Offset Diff X is FAIL!!! %d 0x%x\n", gyro_signed_diff_x,
			gyro_signed_diff_x);
		css_err("Gyro Offset Diff Y is FAIL!!! %d 0x%x\n", gyro_signed_diff_y,
			gyro_signed_diff_y);
		ois_stat->is_stable = 0;
	}

	/* hall_x, hall_y check -10 ~ 10, 10LSB */
	/* will be fuji adjust hall_x, hall_y */
	if ((abs(hall_signed_x) > 10) || (abs(hall_signed_y) > 10)) {
		css_err("hall_signed_x FAIL!!! %d 0x%x\n", hall_signed_x, hall_signed_x);
		css_err("hall_signed_y FAIL!!! %d 0x%x\n", hall_signed_y, hall_signed_y);
		ois_stat->is_stable = 0;
	}

	ois_stat->target[0] = 0; /* not supported */
	ois_stat->target[1] = 0; /* not supported */

	return 0;
}

int32_t fuji_ois_move_lens(int16_t target_x, int16_t target_y)
{
	struct i2c_client *client = fuji_ois_func_tbl.client;
	int8_t hallx =  target_x / HALL_SCALE_FACTOR;
	int8_t hally =  target_y / HALL_SCALE_FACTOR;
	uint8_t result = 0;

	/* check ois mode & change to suitable mode */
	RegRead8(client, 0x6020, &result);
	if (result != 0x01) {
		RegWrite8(client, 0x6020, 0x01);
		if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING))
			return OIS_INIT_TIMEOUT;
	}

	css_info("%s target : %d(0x%x), %d(0x%x)\n", __func__,
		hallx, hallx, hally, hally);

	/* hallx range -> D2 to 2E (-46, 46) */
	RegWrite8(client, 0x6099, 0xFF & hallx); /* target x position input */
	RegWrite8(client, 0x609A, 0xFF & hally); /* target y position input */
	/* wait 100ms */
//	usleep(100000);
	msleep(100);
	RegWrite8(client, 0x6098, 0x01); /* order to move. */

	if (!fuji_ois_poll_ready(LIMIT_STATUS_POLLING * 2))
		return OIS_INIT_TIMEOUT;

	RegRead8(client, 0x609B, &result);

	if (result == 0x03)
		return OIS_SUCCESS;

	css_info("%s move fail : 0x%x \n", __func__, result);
	return OIS_FAIL;
}

void fuji_ois_init(struct i2c_client *client, struct odin_ois_ctrl *ois_ctrl)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	fuji_ois_func_tbl.ois_on = fuji_ois_on;
    fuji_ois_func_tbl.ois_off = fuji_ois_off;
    fuji_ois_func_tbl.ois_mode = fuji_ois_mode;
    fuji_ois_func_tbl.ois_stat = fuji_ois_stat;
	fuji_ois_func_tbl.ois_move_lens = fuji_ois_move_lens;
    fuji_ois_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;
	fuji_ois_func_tbl.client = client;

	pdata->sensor_sid = client->addr;
	pdata->ois_sid = 0x7C;
	pdata->eeprom_sid = 0xA0;

	//ois_ctrl->sid_ois = 0x7C >> 1;
	ois_ctrl->ois_func_tbl = &fuji_ois_func_tbl;
}

