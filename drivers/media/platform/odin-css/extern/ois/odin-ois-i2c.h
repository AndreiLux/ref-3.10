
#ifndef __ODIN_OIS_I2C_H__
#define __ODIN_OIS_I2C_H__

#define MAX_OIS_BIN_FILENAME 64
#define MAX_OIS_BIN_BLOCKS 4
#define MAX_OIS_BIN_FILES 3

#define I2C_SEQ_REG_DATA_MAX      20

struct ois_i2c_bin_addr {
	uint16_t bin_str_addr;
	uint16_t bin_end_addr;
	uint16_t reg_str_addr;
};

struct ois_i2c_bin_entry {
	char  filename[MAX_OIS_BIN_FILENAME];
	uint32_t filesize;
	uint16_t blocks;
	struct ois_i2c_bin_addr addrs[MAX_OIS_BIN_BLOCKS];
};

struct ois_i2c_bin_list {
	uint16_t files;
	struct ois_i2c_bin_entry entries[MAX_OIS_BIN_FILES];
	uint32_t checksum;
};

enum ois_ver_t{
	OIS_VER_RELEASE,
	OIS_VER_CALIBRATION,
	OIS_VER_DEBUG,
	OIS_VER_DEFAULT,
};

enum ois_i2c_reg_addr_type {
	OIS_I2C_BYTE_ADDR = 1,
	OIS_I2C_WORD_ADDR,
};

struct ois_i2c_seq_reg_array {
	uint16_t reg_addr;
	uint8_t reg_data[I2C_SEQ_REG_DATA_MAX];
	uint16_t reg_data_size;
};

struct ois_i2c_seq_reg_setting {
	struct ois_i2c_seq_reg_array *reg_setting;
	uint16_t size;
	enum ois_i2c_reg_addr_type addr_type;
	uint16_t delay;
};

struct odin_ois_info {
	char ois_provider[32];
	int16_t gyro[2];
	int16_t target[2];
	int16_t hall[2];
	uint8_t is_stable;
};

struct odin_ois_fn {
	int (*ois_on) (enum ois_ver_t);
	int (*ois_off) (void);
	int (*ois_mode) (enum ois_mode_t);
	int (*ois_stat) (struct odin_ois_info *);
	int (*ois_move_lens) (int16_t, int16_t);
	int ois_cur_mode;

	struct i2c_client *client;
};

struct odin_ois_ctrl {
	struct odin_ois_info ois_info;
	struct odin_ois_fn *ois_func_tbl;
};

#define OIS_SUCCESS	0
#define OIS_FAIL					CSS_ERROR_OIS_SENSOR_BASE-1
#define OIS_INIT_NOT_SUPPORTED		CSS_ERROR_OIS_SENSOR_BASE-2
#define OIS_INIT_CHECKSUM_ERROR		CSS_ERROR_OIS_SENSOR_BASE-3
#define OIS_INIT_EEPROM_ERROR		CSS_ERROR_OIS_SENSOR_BASE-4
#define OIS_INIT_I2C_ERROR			CSS_ERROR_OIS_SENSOR_BASE-5
#define OIS_INIT_TIMEOUT			CSS_ERROR_OIS_SENSOR_BASE-6
#define OIS_INIT_LOAD_BIN_ERROR		CSS_ERROR_OIS_SENSOR_BASE-7
#define OIS_INIT_NOMEM				CSS_ERROR_OIS_SENSOR_BASE-8
#define OIS_INIT_LOAD_ENTRY_ERROR	CSS_ERROR_OIS_SENSOR_BASE-9

#define OIS_INIT_OLD_MODULE			1
#define OIS_INIT_GYRO_ADJ_FAIL	2
#define OIS_INIT_SRV_GAIN_FAIL	4

#define I2C_NUM_BYTE			(4)//(6)  //QCT maximum size is 6!

////////////////////////////////////////////////////////////////////////

int RamWrite16(struct i2c_client *client, unsigned short RamAddr, unsigned short RamData);
int RamRead16(struct i2c_client *client, unsigned short RamAddr, unsigned short * ReadData);

int RamWrite32(struct i2c_client *client, unsigned short RamAddr, unsigned long RamData);
int RamRead32(struct i2c_client *client, unsigned short RamAddr, unsigned long * ReadData);
int RamWrite8(struct i2c_client *client, unsigned short RamAddr, unsigned char RamData );
int RamRead8(struct i2c_client *client, unsigned short RamAddr, unsigned char *ReadData);

int RegWrite8(struct i2c_client *client, unsigned short RegAddr, unsigned char RegData);
int RegRead8(struct i2c_client *client, unsigned short RegAddr, unsigned char *RegData);

int E2PRegWrite8(struct i2c_client *client, unsigned short RegAddr, unsigned char RegData);
int E2PRegRead8(struct i2c_client *client, unsigned short RegAddr, unsigned char *ReadData);
int E2PRegWrite16(struct i2c_client *client, unsigned short RamAddr, unsigned short RamData);
int E2PRegRead16(struct i2c_client *client, unsigned short RamAddr, unsigned short *ReadData);

void E2pRed(struct i2c_client *client, unsigned short UsAdr, unsigned char UcLen, unsigned char *UcPtr);	// E2P ROM Data Read
void E2pWrt(struct i2c_client *client, unsigned short UsAdr, unsigned char UcLen, unsigned char *UcPtr);	// E2P ROM Data Write

int32_t ois_i2c_load_and_write_bin_list(struct i2c_client *client, struct ois_i2c_bin_list *bin_list);
int32_t ois_i2c_load_and_write_e2prom_data(struct i2c_client *client, uint16_t e2p_str_addr, uint16_t e2p_data_length, uint16_t reg_str_addr);

#endif /* __ODIN_OIS_I2C_H__ */
