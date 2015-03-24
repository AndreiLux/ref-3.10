
#ifndef __DEVS_H__
#define __DEVS_H__

#include <mach/board.h>

struct mt_bls_data;


int mt_board_init(void);
int mt_dev_i2c_bus_configure(u16 n_i2c, u16 speed, u16 wrrd[]);
static inline int mt_dev_i2c_set_i2c_bus_speed(u16 n_i2c, u16 speed)
{
	return mt_dev_i2c_bus_configure(n_i2c, speed, 0);
}
void mt_bls_init(struct mt_bls_data *bls_data);
int mt_pwrap_init(void);
extern u32 get_devinfo_with_index(u32 index);
extern u32 g_devinfo_data[];
extern u32 g_devinfo_data_size;
extern void adjust_kernel_cmd_line_setting_for_console(char *, char *);
extern void musbfsh_hcd_release(struct device *dev);
extern unsigned long max_pfn;
extern unsigned int mtk_get_max_DRAM_size(void);

#if defined(CONFIG_MTK_FB)
extern unsigned int DISP_GetVRamSizeBoot(char *cmdline);
#define RESERVED_MEM_SIZE_FOR_FB (DISP_GetVRamSizeBoot((char *)&temp_command_line))
extern void mtkfb_set_lcm_inited(bool isLcmInited);
#else
#define RESERVED_MEM_SIZE_FOR_FB (0x400000)
#endif

#if defined(CONFIG_FIQ_DEBUGGER)
extern void fiq_uart_fixup(int uart_port);
extern struct platform_device mt_fiq_debugger;
#endif

extern void set_usb_vbus_gpio(int gpio);

#endif				/* __DEVS_H__ */
