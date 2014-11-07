#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>

#include "odin_proxy.h"
#include "odin_proxy_i2c.h"
#include "../../odin-camera.h"

int32_t ProxyWrite16bit(uint32_t RamAddr, uint16_t RamData )
{
	int32_t ret = 0;
	//ret = proxy_i2c_write(RamAddr,RamData, 2);
	return ret;
}

int32_t ProxyRead16bit(uint32_t RamAddr, uint16_t *ReadData )
{
	int32_t ret = 0;
	//ret = proxy_i2c_read(RamAddr, ReadData, 2);
	unsigned char regs[4] = {0, };

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;

	ret = vl6180_i2c_read(regs, 2);

	*ReadData = regs[0] << 8 | regs[1];
	return ret;
}

int32_t CameraRead16bit(uint32_t RamAddr, uint16_t *ReadData )
{
	int32_t ret = 0;
	//ret = proxy_i2c_read(RamAddr, ReadData, 2);
	unsigned char regs[4] = {0, };

	regs[0] = RamAddr >> 8;
	regs[1] = RamAddr & 0xff;
#if 1
	ret = vl6180_camera_i2c_read(regs, 2);
	//ret = camera_i2c_read(regs, 2);
//	ret = camera_i2c_read(proxy_client, regs);

	*ReadData = regs[0] << 8 | regs[1];
#else
	ret = vl6180_camera_i2c_read(regs, 2);
	*ReadData = regs[0];
#endif
	return ret;
}


int32_t ProxyWrite32bit(uint32_t RamAddr, uint32_t RamData )
{
	int32_t ret = 0;
	uint8_t data[4];

	data[0] = (RamData >> 24) & 0xFF;
	data[1] = (RamData >> 16) & 0xFF;
	data[2] = (RamData >> 8)  & 0xFF;
	data[3] = (RamData) & 0xFF;

	ret = proxy_i2c_write_seq(RamAddr, &data[0], 4);
	return ret;
}

int32_t ProxyRead32bit(uint32_t RamAddr, uint32_t *ReadData )
{
	unsigned char buf[4] = {0, };
	unsigned char local_data[4] = {0, };

	buf[0] = RamAddr >> 8;
	buf[1] = RamAddr & 0xff;

	vl6180_i2c_read(buf, 1);
	local_data[0] = buf[0];

	RamAddr++;
	buf[0] = RamAddr >> 8;
	buf[1] = RamAddr & 0xff;
	vl6180_i2c_read(buf, 1);
	local_data[1] = buf[0];

	RamAddr++;
	buf[0] = RamAddr >> 8;
	buf[1] = RamAddr & 0xff;
	vl6180_i2c_read(buf, 1);
	local_data[2] = buf[0];

	RamAddr++;
	buf[0] = RamAddr >> 8;
	buf[1] = RamAddr & 0xff;
	vl6180_i2c_read(buf, 1);
	local_data[3] = buf[0];

	*ReadData = ((unsigned long)local_data[0] << 24 | (unsigned long)local_data[1] << 16
										| (unsigned long)local_data[2] << 8 | local_data[3]);
	return 1;
}

int32_t ProxyWrite8bit(uint32_t RegAddr, uint8_t RegData)
{
	int32_t ret = 0;

	unsigned char reg_data[3] = {0, };
	reg_data[0] = RegAddr >> 8;
	reg_data[1] = RegAddr & 0xff;
	reg_data[2] = RegData & 0xff;;

	vl6180_i2c_write(reg_data, 3);
	return ret;

}

int32_t ProxyRead8bit(uint32_t RegAddr, uint8_t *RegData)
{
	unsigned char regs[4] = {0, };
	int ret = 1;

	regs[0] = RegAddr >> 8;
	regs[1] = RegAddr & 0xff;

	ret = vl6180_i2c_read(regs, 1);

	*RegData = regs[0];

	return 1;
}
