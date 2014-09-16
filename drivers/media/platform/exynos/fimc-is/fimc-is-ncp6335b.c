#include <linux/types.h>
#include <linux/input.h>
#include <linux/errno.h>
#include <linux/err.h>

#include "fimc-is-ncp6335b.h"
static const int vout_table[][2] = {
	{0, NCP6335B_VOUT_1P000}, /* defualt voltage */
	{1, NCP6335B_VOUT_0P875},
	{2, NCP6335B_VOUT_0P900},
	{3, NCP6335B_VOUT_0P925},
	{4, NCP6335B_VOUT_0P950},
	{5, NCP6335B_VOUT_0P975},
	{6, NCP6335B_VOUT_1P000},
};


int ncp6335b_get_vout_val(int sel)
{
	int i, vout = vout_table[0][1];

	if (sel < 0)
		pr_err("%s: error, invalid sel %d\n", __func__, sel);

	for (i = 0; ARRAY_SIZE(vout_table); i++) {
		if (vout_table[i][0] == sel) {
			return vout_table[i][1];
		}
	}

	pr_err("%s: warning, default voltage selected. sel %d\n", __func__, sel);

	return vout;
}

int ncp6335b_set_voltage(struct i2c_client *client, int vout)
{
	int ret = i2c_smbus_write_byte_data(client, 0x14, 0x00);
	if (ret < 0)
		pr_err("[%s::%d] Write Error [%d]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_write_byte_data(client, 0x10, vout); /* 1.05V -> 1V (0xC0) */
	if (ret < 0)
		pr_err("[%s::%d] Write Error [%d]. vout 0x%X\n", __func__, __LINE__, ret, vout);

	ret = i2c_smbus_write_byte_data(client, 0x11, vout); /* 1.05V -> 1V (0xC0) */
	if (ret < 0)
		pr_err("[%s::%d] Write Error [%d]. vout 0x%X\n", __func__, __LINE__, ret, vout);

	/*pr_info("%s: vout 0x%X\n", __func__, vout);*/

	return ret;
}

int ncp6335b_read_voltage(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, 0x3);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B PID[%x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x10);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x10 Read :: %x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x11);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x11 Read :: %x]\n", __func__, __LINE__, ret);

	ret = i2c_smbus_read_byte_data(client, 0x14);
	if (ret < 0)
		pr_err("[%s::%d]Read Error [%d]\n", __func__, __LINE__, ret);
	pr_err("[%s::%d]NCP6335B [0x14 Read :: %x]\n", __func__, __LINE__, ret);

	return ret;
}

