#ifndef __DBG_DUMP_H
#define __DBG_DUMP_H

struct reg_dump_driver_data
{
        u32 mcu_regs;
};

struct mt_lastpc_driver
{
    struct      platform_driver driver;
    struct      platform_device device;
    int         (*lastpc_probe)(struct platform_device *pdev);
    int         (*lastpc_remove)(struct platform_device *pdev);
    int         (*lastpc_suspend)(struct platform_device *pdev, pm_message_t state);
    int         (*lastpc_resume)(struct platform_device *pdev);
    int         (*lastpc_reg_dump)(char *buf);
};
struct mt_lastpc_driver *get_mt_lastpc_drv(void);
extern struct reg_dump_driver_data reg_dump_driver_data;
int mt_reg_dump(char *buf);
//#define LASTPC_TEST_SUIT
#endif
