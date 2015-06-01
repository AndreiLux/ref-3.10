#ifndef __SYSTRACKER_DRV_H
#define __SYSTRACKER_DRV_H

#include <mach/systracker.h>
struct mt_systracker_driver
{
    struct	platform_driver driver;
    struct	platform_device device;
    void	(*reset_systracker)(void);
    int		(*enable_watch_point)(void);
    int		(*disable_watch_point)(void);
    int		(*set_watch_point_address)(unsigned int wp_phy_address);
    void	(*enable_systracker)(void);
    void	(*disable_systracker)(void);
    int		(*test_systracker)(void);
    int         (*systracker_probe)(struct platform_device *pdev);
    int 	(*systracker_remove)(struct platform_device *pdev);
    int 	(*systracker_suspend)(struct platform_device *pdev, pm_message_t state);
    int 	(*systracker_resume)(struct platform_device *pdev);
};

struct mt_systracker_driver *get_mt_systracker_drv(void);
struct systracker_entry_t
{
    unsigned int dbg_con;
    unsigned int ar_track_l[BUS_DBG_NUM_TRACKER];
    unsigned int ar_track_h[BUS_DBG_NUM_TRACKER];
    unsigned int ar_trans_tid[BUS_DBG_NUM_TRACKER];
    unsigned int aw_track_l[BUS_DBG_NUM_TRACKER];
    unsigned int aw_track_h[BUS_DBG_NUM_TRACKER];
    unsigned int aw_trans_tid[BUS_DBG_NUM_TRACKER];
};
struct systracker_config_t
{
        int state;
        int enable_timeout;
        int enable_slave_err;
        int enable_wp;
        int enable_irq;
        int timeout_ms;
        int wp_phy_address;
};
int tracker_dump(char *buf);
void dump_backtrace_entry_ramconsole_print(unsigned long where, unsigned long from, unsigned long frame);
void dump_regs(const char *fmt, const char v1, const unsigned int reg, const unsigned int reg_val);


//#define SYSTRACKER_TEST_SUIT  //enable for driver poring test suit
//#define TRACKER_DEBUG 0
#endif
