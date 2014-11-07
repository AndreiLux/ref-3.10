#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "../../odin-css.h"

#include "odin-ois-i2c.h"

unsigned char I2cSlvAddrWr = 0x7C;
unsigned char EEPROMSlaveAddr = 0xA0;
unsigned char I2cAddrLen = 2;

int ois_i2c_write(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (char *)buf;

	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}

int ois_i2c_read(const struct i2c_client *client, const char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msgs[] = {
		{
			.addr  = client->addr,
			.flags = 0,
			.len   = 2,
			.buf   = (char *)buf,
		}, {
			.addr  = client->addr,
		//	.flags = I2C_M_RD,
			.flags = (I2C_M_RD | I2C_M_STOP),
			.len   = count,
			.buf   = (char *)buf,
		},
	};

	if (i2c_transfer(adap, msgs, ARRAY_SIZE(msgs)) != count) {
		css_err("ois_i2c_read error!!\n");
		return -EREMOTEIO;
	}

	return OIS_SUCCESS;
}

/***************************/
/* 16bit - 16bit I2C Write */
/***************************/
int RamWrite16(struct i2c_client *client, unsigned short RamAddr,
		unsigned short RamData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[4] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	regs[2] = RamData >> 8;
	regs[3] = RamData & 0xff;
	reg_table = regs;
	ret = ois_i2c_write(client, reg_table, 4);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);
	if (ret != 4) {
		ret = ois_i2c_write(client, reg_table, 4);
		css_err("RamWrite16 retry\n");
	}

	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 		16bit - 16bit I2C Read 		*/
/************************************/
int RamRead16(struct i2c_client *client, unsigned short RamAddr,
		unsigned short *ReadData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char regs[2] = {0, };
	unsigned char data[2] = {0, };
/*                                                                                            */
	unsigned char *reg_table;
	unsigned short *read_data = (unsigned short *)ReadData;
/*                                                                                            */
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
#if 1 /*                                                                                          */
	reg_table = regs;
	ret = ois_i2c_read(client, reg_table, 2);
	*read_data = (*reg_table << 8) | *(reg_table+1) ;
#else
	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	css_sensor("addr = 0x%x, data[0] = 0x%4x\n", RamAddr, regs[0]);
	data[0] = regs[0];

	RamAddr++;
	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	css_sensor("addr = 0x%x, data[1] = 0x%4x\n", RamAddr, regs[0]);
	data[1] = regs[0];

//	*ReadData = ((unsigned short)data[0] << 8) | (data[1] & 0xff);
	*ReadData = ((unsigned short)data[1] << 8) | (data[0] & 0xff);

	css_sensor("ReadData = 0x%4x\n", *ReadData);
#endif

	client->addr = pdata->sensor_sid;

	return ret;
}


/************************************/
/* 		16bit - 32bit I2C Write 	*/
/************************************/
int RamWrite32(struct i2c_client *client, unsigned short RamAddr,
			unsigned long RamData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[6] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	regs[2] = (RamData >> 24)& 0xff;
	regs[3] = (RamData >> 16) & 0xff;
	regs[4] = (RamData >> 8) & 0xff;
	regs[5] = RamData & 0xff;
	reg_table = regs;

	ret = ois_i2c_write(client, reg_table, 6);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);

	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 		16bit - 32bit I2C Read 		*/
/************************************/
int RamRead32(struct i2c_client *client, unsigned short RamAddr,
		unsigned long *ReadData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char regs[4] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;

	ret = ois_i2c_read(client, (unsigned char *)regs, 4);
	*ReadData = ((unsigned long)regs[0] << 24 | (unsigned long)regs[1] << 16
						| (unsigned long)regs[2] << 8 | regs[3]);
	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 		16bit - 8bit I2C Write		*/
/************************************/
int RamWrite8(struct i2c_client *client, unsigned short RamAddr,
		unsigned char RamData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[3] = {0, };
	int ret = OIS_SUCCESS;
	unsigned short addr;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	regs[2] = RamData & 0xff;
	reg_table = regs;

	ret = ois_i2c_write(client, reg_table, 3);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);
	if(ret != 3){
		ret = ois_i2c_write(client, reg_table, 3);
		css_err("RamWrite8 retry\n");
	}

	client->addr = pdata->sensor_sid;

	return ret;
}


/************************************/
/* 	16bit - 8bit I2C Read  - IMSI	*/
/************************************/
int RamRead8(struct i2c_client *client, unsigned short RamAddr, unsigned char *ReadData )
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char regs[2] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;

	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	*ReadData = regs[0];
	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 		16bit - 8bit I2C Write		*/
/************************************/
int RegWrite8(struct i2c_client *client, unsigned short RegAddr, unsigned char RegData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[3] = {0, };
	int ret = OIS_SUCCESS;
	unsigned short addr;

	client->addr = pdata->ois_sid;

	regs[0] = RegAddr >> 8;
	regs[1] = RegAddr & 0xff;
	regs[2] = RegData & 0xff;

	reg_table = regs;

	ret = ois_i2c_write(client, reg_table, 3);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);
	if (ret != 3) {
		ret = ois_i2c_write(client, reg_table, 3);
		css_err("RegWrite8 retry\n");
	}
	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 		16bit - 8bit I2C Read		*/
/************************************/
int RegRead8(struct i2c_client *client, unsigned short RegAddr, unsigned char *RegData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char regs[2] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->ois_sid;

	regs[0] = RegAddr >> 8;
	regs[1] = RegAddr & 0xff;

	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	*RegData = regs[0];
	client->addr = pdata->sensor_sid;

	return ret;
}

/************************************/
/* 	EEPROM I2C 16bit-8bit Write		*/
/************************************/
int E2PRegWrite8(struct i2c_client *client, unsigned short RegAddr,
		unsigned char RegData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[3] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->eeprom_sid;

	regs[0] = RegAddr >> 8;
	regs[1] = RegAddr & 0xff;
	regs[2] = RegData & 0xff;
	reg_table = regs;

	ret = ois_i2c_write(client, reg_table, 3);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);
	if (ret != 3) {
		ret = ois_i2c_write(client, reg_table, 3);
		css_err("E2PRegWrite8 retry\n");
	}

	client->addr = pdata->sensor_sid;

	return ret;
}


/************************************/
/* 	EEPROM I2C 16bit-8bit Read		*/
/************************************/
int E2PRegRead8(struct i2c_client *client, unsigned short RegAddr,
				unsigned char * ReadData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char regs[2] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->eeprom_sid;

	regs[0] = RegAddr >> 8;
	regs[1] = RegAddr & 0xff;

	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	*ReadData = regs[0];

	client->addr = pdata->sensor_sid;

	return ret;
}


/************************************/
/* 	EEPROM I2C 16bit-16bit Write	*/
/************************************/
int E2PRegWrite16(struct i2c_client *client, unsigned short RamAddr,
			unsigned short RamData)
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	unsigned char *reg_table;
	unsigned char regs[4] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->eeprom_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	regs[2] = RamData >> 8;
	regs[3] = RamData & 0xff;
	reg_table = regs;

	ret = ois_i2c_write(client, reg_table, 4);
	css_sensor("Dev ID = %02x,addr1 = %02x,addr2 = %02x,data = %02x,ret = %d\n",
				client->addr, *reg_table , *(reg_table+1), *(reg_table+2), ret);
	if (ret != 4) {
		ret = ois_i2c_write(client, reg_table, 4);
		css_err("E2PRegWrite16 retry\n");
	}

	client->addr = pdata->sensor_sid;

	return ret;
}


int E2PRegRead16(struct i2c_client *client, unsigned short RamAddr,
		unsigned short *ReadData )
{
	struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
//	unsigned char *reg_table;
	unsigned char regs[2] = {0, };
	unsigned char data[2] = {0, };
	int ret = OIS_SUCCESS;

	client->addr = pdata->eeprom_sid;

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
#if 0
	reg_table = regs;

	ret = ois_i2c_read(client, reg_table, 2);
	*ReadData = (*reg_table << 8) | *(reg_table+1) ;
#else
	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	data[0] = regs[0];

	RamAddr++;
	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
	ret = ois_i2c_read(client, (unsigned char *)regs, 1);
	data[1] = regs[0];

	*ReadData = ((unsigned short)data[0] << 8) | data[1] ;
#endif
	client->addr = pdata->sensor_sid;

	return ret;
}

void E2pRed(struct i2c_client *client, unsigned short UsAdr,
		unsigned char UcLen, unsigned char *UcPtr)
{
	unsigned char UcCnt;

	for (UcCnt = 0; UcCnt < UcLen;  UcCnt++) {
		E2PRegRead8(client, UsAdr + UcCnt ,
			(unsigned char *)&UcPtr[ abs((UcLen - 1) - UcCnt) ]);
	}
}

void E2pWrt(struct i2c_client *client, unsigned short UsAdr,
		unsigned char UcLen, unsigned char *UcPtr)
{
	unsigned short UcCnt;

	for (UcCnt = 0; UcCnt < UcLen;  UcCnt++) {
		E2PRegWrite8(client, UsAdr + UcCnt ,
			(unsigned char) UcPtr[ abs((UcLen - 1) - UcCnt) ]);
	}
}

int32_t load_bin(uint8_t * ois_bin, uint32_t filesize, char * filename)
{
	int fd;
	uint8_t fs_name_buf[256];
	uint32_t cur_fd_pos;
	int32_t rc = OIS_SUCCESS;

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	sprintf(fs_name_buf,"/system/media/%s",filename);

	fd = sys_open(fs_name_buf, O_RDONLY, 0);
	if (fd<0) {
		css_err("%s: File not exist \n",__func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
		goto END;
	}

#ifndef NO_SUMCHECK
	if ((unsigned)sys_lseek(fd, (off_t)0, 2) != filesize) {
		css_err("%s: File size error \n",__func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
		goto END;
	}
	memset(ois_bin, 0x00, filesize);
#else
	memset(ois_bin, 0x00, filesize);
	filesize = (unsigned)sys_lseek(fd, (off_t)0, 2);
#endif
	cur_fd_pos = (unsigned)sys_lseek(fd, (off_t)0, 0);

	if ((unsigned)sys_read(fd, (char __user *)ois_bin, filesize) != filesize) {
		css_err("%s: File read error \n",__func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
	}

END:
	sys_close(fd);
	set_fs(old_fs);

	return rc;
}

int32_t ois_i2c_write_seq(struct i2c_client *client, uint16_t addr,
			uint8_t *data, uint16_t num_byte)
{
#if 1
	 struct css_i2cboard_platform_data *pdata = client->dev.platform_data;
	 int ret = 0;
	 int addr_type = OIS_I2C_WORD_ADDR;
	 unsigned char buf[addr_type+num_byte];
	 uint8_t len = 0, i = 0;

	 if (addr_type == OIS_I2C_BYTE_ADDR) {
		 buf[0] = addr;
		 len = 1;
	 } else if (addr_type == OIS_I2C_WORD_ADDR) {
		 buf[0] = addr >> BITS_PER_BYTE;
		 buf[1] = addr;
		 len = 2;
	 }

	 for (i = 0; i < num_byte; i++) {
		 buf[i+len] = data[i];
	 }

	 client->addr = pdata->ois_sid;
	 ret = ois_i2c_write(client, buf, len+num_byte);

	 if (ret != len+num_byte) {
		 ret = ois_i2c_write(client, buf, len+num_byte);
		 css_err("ois_i2c_write_seq retry\n");
	 }

	client->addr = pdata->sensor_sid;

	if (ret == len+num_byte)
		return 0;
	else
		return -EFAULT;

#else
	int i = 0;

	for (i = 0; i < num_byte; i++) {
		RegWrite8(client, addr++, 0xFF & *(data + i));
	}
	return 0;
#endif
}

int32_t ois_i2c_write_seq_table(struct i2c_client *client,
				struct ois_i2c_seq_reg_setting *write_setting)
{
#if 0
	int32_t rc = 0;
	int i = 0;
	uint16_t addr = write_setting->reg_setting->reg_addr;
	uint8_t *data;// = (uint8_t *)write_setting->reg_setting->reg_data;
	int j = 0;

	for (i = 0; i < write_setting->size; i++) {
		data = write_setting->reg_setting[i].reg_data;
		RegWrite8(client, addr++, 0xFF & (*data));
		RegWrite8(client, addr++, 0xFF & (*(data+1)));
		RegWrite8(client, addr++, 0xFF & (*(data+2)));
		RegWrite8(client, addr++, 0xFF & (*(data+3)));
		RegWrite8(client, addr++, 0xFF & (*(data+4)));
		RegWrite8(client, addr++, 0xFF & (*(data+5)));
	}
#else
	int i;
	int32_t rc = -EFAULT;
	struct ois_i2c_seq_reg_array *reg_setting;
	uint16_t client_addr_type;
	int addr_type = OIS_I2C_WORD_ADDR;

	if (!client || !write_setting)
			  return rc;

	if ((write_setting->addr_type != OIS_I2C_BYTE_ADDR &&
		write_setting->addr_type != OIS_I2C_WORD_ADDR)) {
			css_err("%s Invalide addr type %d\n", __func__,
				write_setting->addr_type);
		return rc;
	}

	client_addr_type = addr_type;
	addr_type = write_setting->addr_type;
	reg_setting = write_setting->reg_setting;

	for (i = 0; i < write_setting->size; i++) {
		rc = ois_i2c_write_seq(client, reg_setting->reg_addr,
						 reg_setting->reg_data, reg_setting->reg_data_size);
		if (rc < 0)
			break;

		reg_setting++;
	}

	if (write_setting->delay > 20)
		msleep(write_setting->delay);
	else if (write_setting->delay)
		usleep_range(write_setting->delay * 1000, (write_setting->delay * 1000)
													+ 1000);

	addr_type = client_addr_type;
#endif

	return rc;
}

int32_t ois_i2c_bin_seq_write(struct i2c_client *client, int src_saddr,
				int src_eaddr, int dst_addr, uint8_t * ois_bin)
{
	int writen_byte, cnt, cnt2;
	int total_byte, remaining_byte;
	struct ois_i2c_seq_reg_setting conf_array;
	struct ois_i2c_seq_reg_array *reg_setting = NULL;
	int32_t rc = 0;

	writen_byte = 0;

	total_byte = src_eaddr-src_saddr+1;

	conf_array.size = total_byte/I2C_NUM_BYTE;
	remaining_byte = total_byte - conf_array.size*I2C_NUM_BYTE;

	css_sensor("%s, write seq array size = %d, remaining_byte = %d\n",
		__func__, conf_array.size, remaining_byte);

	if (remaining_byte<0) {
		css_sensor("%s, remaining_byte = %d\n",__func__,remaining_byte);
		conf_array.size = conf_array.size-1;
		remaining_byte = total_byte - conf_array.size*I2C_NUM_BYTE;
	}

	reg_setting = kzalloc(conf_array.size *
		(sizeof(struct ois_i2c_seq_reg_array)), GFP_KERNEL);

	if (!reg_setting) {
		css_err("%s:%d failed\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}

	for (cnt = 0; cnt < conf_array.size; cnt++, dst_addr += I2C_NUM_BYTE) {
		reg_setting[cnt].reg_addr = dst_addr;
		reg_setting[cnt].reg_data_size = I2C_NUM_BYTE;
		for (cnt2 = 0; cnt2 < I2C_NUM_BYTE; cnt2++) {
			reg_setting[cnt].reg_data[cnt2] = ois_bin[src_saddr++];
		}
	}

	conf_array.addr_type = OIS_I2C_WORD_ADDR;
	conf_array.delay = 0;
	conf_array.reg_setting = reg_setting;

	rc = ois_i2c_write_seq_table(client,&conf_array);
	if (rc < 0)
	{
		css_err("%s:%d failed\n", __func__, __LINE__);
		goto END;
	}
	css_sensor("%s, write array seq finished!, src_saddr %d, dst_addr %d \n",
		__func__,src_saddr,  dst_addr);

	if(remaining_byte>0)
	{
		rc = ois_i2c_write_seq(client, dst_addr, &ois_bin[src_saddr],
				remaining_byte);
		css_sensor("%s, remaining bytes write!, dst_addr %d, src_saddr %d, \
			remaining_byte %d \n", __func__, dst_addr, src_saddr, remaining_byte);
	}

END :
	kfree(reg_setting);
	return rc;
}

int32_t ois_i2c_load_and_write_bin(struct i2c_client *client,
				struct ois_i2c_bin_entry *bin_entry)
{
	int32_t rc = OIS_SUCCESS;
	uint8_t *ois_bin_data = NULL;
	int i;
	struct ois_i2c_bin_addr addr;

	css_sensor("%s\n", __func__);

	if (!bin_entry) {
		css_err("Can not load bin entry data\n");
		rc = OIS_INIT_LOAD_ENTRY_ERROR;
		goto END2;
	}
	ois_bin_data = kmalloc(bin_entry->filesize, GFP_KERNEL);

	if (!ois_bin_data) {
		css_err("%s: Can not alloc bin data\n");
		rc = OIS_INIT_NOMEM;
		goto END2;
	}

	rc = load_bin(ois_bin_data, bin_entry->filesize, bin_entry->filename);
	if (rc <0) {
		css_err("%s: load bin error, %d\n", __func__, rc);
		goto END1;
	}

	for (i = 0; i < bin_entry->blocks; i++) {
		addr = bin_entry->addrs[i];
		rc = ois_i2c_bin_seq_write(client,addr.bin_str_addr, addr.bin_end_addr,
					addr.reg_str_addr, ois_bin_data);
		if (rc < 0) {
			css_err("%s: program %d down error\n",__func__, i);
			rc = OIS_INIT_I2C_ERROR;
			goto END1;
		}
	}

END1:
	kfree(ois_bin_data);
END2:
	return rc;
}

int32_t ois_i2c_load_and_write_bin_list(struct i2c_client *client,
				struct ois_i2c_bin_list *bin_list)
{
	int i;
	int length = bin_list->files;
	int32_t rc = OIS_SUCCESS;
	for (i = 0; i < length; i++) {
		rc = ois_i2c_load_and_write_bin(client,&bin_list->entries[i]);
		if (rc <0) {
			goto END;
		}
	}

END:
	return rc;
}

int32_t ois_i2c_load_and_write_e2prom_data(struct i2c_client *client,
				uint16_t e2p_str_addr, uint16_t e2p_data_length,
				uint16_t reg_str_addr)
{
	int32_t rc = OIS_SUCCESS;
	uint8_t *e2p_cal_data = NULL;
	int cnt;
	uint8_t data;

	/* Read E2P data!*/
	e2p_cal_data = kmalloc(e2p_data_length, GFP_KERNEL);

	if (!e2p_cal_data) {
		css_err("%s: Can not alloc e2p data\n", __func__);
		rc = OIS_INIT_NOMEM;
		goto END2;
	}

	memset(e2p_cal_data, 0x00, e2p_data_length);

	/* start reading from e2p rom*/
	for (cnt = 0; cnt < e2p_data_length; cnt++) {
		E2PRegRead8(client, e2p_str_addr+cnt, &data);//ois_i2c_e2p_read(e2p_str_addr+cnt, &data, 1);
		e2p_cal_data[cnt] = data & 0xFF;
		/*printk("%s: eeprom 0x%x :0x%x, %x\n",__func__,t_addr,e2p_str_addr+cnt,e2p_cal_data[cnt]);*/
	}

	rc = ois_i2c_bin_seq_write(client, 0, e2p_data_length-1, reg_str_addr, e2p_cal_data);
	if (rc < 0) {
		css_err("%s: e2p down error\n",__func__);
		goto END1;
	}

END1:
	kfree(e2p_cal_data);
END2:
	return rc;
}

