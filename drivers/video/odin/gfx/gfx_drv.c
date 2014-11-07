/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2013 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See th
 * GNU General Public License for more details.
 */

/*       
  
                                              
                                                                         
  
                                         
                
                    
                                 
  
                          
     
 */

/*-------------------------------------------------------------------
	Control Constants
--------------------------------------------------------------------*/
/* #undef SUPPORT_GFX_DEVICE_READ_WRITE_FOPS */
/* #define SUPPORT_GFX_UNLOCKED_IOCTL */

/*-------------------------------------------------------------------
	File Inclusions
--------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#ifdef CONFIG_PM	/*  added by SC Jung for quick booting */
#include <linux/platform_device.h>
#include <linux/delay.h>
#endif
#include "os_util.h"
/* #include "base_device.h"  */
#include "gfx_impl.h"
#include "gfx_drv.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/odin_mailbox.h>
#include <linux/odin_pd.h>
#include <linux/odin_iommu.h>
#include <linux/pm_qos.h>

#include <video/odindss.h>
#include "../dss/dss.h"

#include <linux/pm_runtime.h>
#include <linux/odin_pm_domain.h>

#include <linux/miscdevice.h>

/*-------------------------------------------------------------------
	Constant Definitions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	Macro Definitions
--------------------------------------------------------------------*/
#define DRV_NAME 	"gfx0"

/*-------------------------------------------------------------------
	Type Definitions
--------------------------------------------------------------------*/
/**
 *	main control block for gfx device.
 *	each minor device has unique control block
 *
 */
typedef struct
{
/*  BEGIN of common device */
	int		dev_open_count;		/*  check if device is opened or not */
	dev_t	devno;				/*  device number */
	struct cdev	cdev;			/*  char device structure */
/*  END of command device */

/*  BEGIN of device specific data */

/*  END of device specific data */
}
GFX_DEVICE_T;


#ifdef CONFIG_PM
typedef struct
{
	bool	is_suspended;		/*  add here extra parameter */
}GFX_DRVDATA_T;
#endif

static struct device *dev;

/*-------------------------------------------------------------------
	External Function Prototype Declarations
--------------------------------------------------------------------*/
extern void gfx_odin_inithal ( GFX_HAL_T*  hal );
extern int odin_crg_gfx_swreset (bool enable);
extern void gfx_restart_function(void);
extern void gfx_stop_function(void);

/* implemented at core/gfx_core_drv.c */

/*-------------------------------------------------------------------
	External Variables
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	global Functions
--------------------------------------------------------------------*/

/*-------------------------------------------------------------------
	global Variables
--------------------------------------------------------------------*/
static int	g_gfx_major = GFX_MAJOR;
static int	g_gfx_minor = GFX_MINOR;
/* int	g_gfx_debug_fd;	*/
int	g_gfx_trace_depth;

LX_GFX_CFG_T	g_gfx_cfg;
GFX_HAL_T		g_gfx_hal;
int	g_fhd_flag = 0;
static int	g_gfx_oper_status = GFX_NONE_FLAG;

static int	g_gfx_working = 0;

/*-------------------------------------------------------------------
	Static Function Prototypes Declarations
--------------------------------------------------------------------*/
static int gfx_open(struct inode *inode, struct file *filp);
static int gfx_close(struct inode *inode, struct file *filp);

static long gfx_ioctl(struct file *filp, u32 cmd, ULONG arg );
/* #endif */
static int gfx_mmap(struct file *filp, struct vm_area_struct *vma);

/*-------------------------------------------------------------------
	Static Variables
--------------------------------------------------------------------*/
static OS_SEM_T gfx_dev_mtx;

/**
 * main control block for gfx device
*/
static GFX_DEVICE_T *g_gfx_device;

static struct pm_qos_request mem_qos;

/**
 * file I/O description for gfx device
 *
*/
static struct file_operations g_gfx_fops =
{
	.open 	= gfx_open,
	.release= gfx_close,
	.unlocked_ioctl = gfx_ioctl,
	.mmap	= gfx_mmap,
};

static inline char* gfx_ioc2str ( u32 ioc_cmd )
{
    switch (ioc_cmd)
    {
	case GFX_IO_RESET:
		return "GFX_IO_RESET";
	case GFX_IOR_CHIP_REV_INFO:
		return "GFX_IOR_CHIP_REV_INFO";
	case GFX_IOR_GET_CFG:
		return "GFX_IOR_GET_CFG";
	case GFX_IORW_ALLOC_SURFACE:
		return "GFX_IORW_ALLOC_SURFACE";
	case GFX_IOW_FREE_SURFACE:
		return "GFX_IOW_FREE_SURFACE";
	case GFX_IOW_SET_SURFACE_PALETTE:
		return "GFX_IOW_SET_SURFACE_PALETTE";
	case GFX_IOR_GET_SURFACE_PALETTE:
		return "GFX_IOR_GET_SURFACE_PALETTE";
	case GFX_IORW_GET_SURFACE_MEM_INFO:
		return "GFX_IORW_GET_SURFACE_MEM_INFO";
	case GFX_IOR_GET_POWERSTATUS:
		return "GFX_IOR_GET_POWERSTATUS";
	case GFX_IOW_SET_POWERSTATUS:
		return "GFX_IOW_SET_POWERSTATUS";
	case GFX_IOW_SET_DUAL_GFX:
		return "GFX_IOW_SET_DUAL_GFX";
	case GFX_IOR_GET_OPER_MODE:
		return "GFX_IOR_GET_OPER_MODE";
	case GFX_IOW_SET_OPER_MODE:
		return "GFX_IOW_SET_OPER_MODE";
	case GFX_IOW_BLEND:
		return "GFX_IOW_BLEND";
	case GFX_IOW_MANUAL_BLEND:
		return "GFX_IOW_MANUAL_BLEND";
	case GFX_IOW_SET_CSC_CONV_TBL:
		return "GFX_IOW_SET_CSC_CONV_TBL";
	case GFX_IOR_GET_CMD_DELAY:
		return "GFX_IOR_GET_CMD_DELAY";
	case GFX_IOW_SET_CMD_DELAY:
		return "GFX_IOW_SET_CMD_DELAY";
	case GFX_IOW_SET_BATCH_RUN_MODE:
		return "GFX_IOW_SET_BATCH_RUN_MODE";
	case GFX_IOR_GET_BATCH_RUN_MODE:
		return "GFX_IOR_GET_BATCH_RUN_MODE";
	case GFX_IOW_SET_GRAPHIC_SYNC_MODE:
		return "GFX_IOW_SET_GRAPHIC_SYNC_MODE";
	case GFX_IOW_WAIT_FOR_SYNC:
		return "GFX_IOW_WAIT_FOR_SYNC";
	case GFX_IOR_GET_MEM_STAT:
		return "GFX_IOR_GET_MEM_STAT";
	default:
		return "Unknown IOCTL";
    }
}

/** lock GFX device */
void gfx_lockdevice(void)
{
	OS_LockMutex( &gfx_dev_mtx );
}

/** unlock GFX device */
void gfx_unlockdevice  (void)
{
	OS_UnlockMutex( &gfx_dev_mtx );
}


/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
/** initialize HAL function list
 *
 */
static void gfx_inithal ( void )
{
    memset( &g_gfx_hal, 0x0, sizeof(GFX_HAL_T));

    /* initialize HAL (Hardware Abstraction Layer) */
	gfx_odin_inithal( &g_gfx_hal );

}

/** initialize GFX environment before the real initialization
 *
 *	@note gfx_inithal should be called before calling any other functions.
 *	@note gfx_initcfg should be called before calling any other functions.
 */
void gfx_preinit(void)
{
	gfx_inithal( );
	gfx_initcfg( );
}

/*-------------------------------------------------------------------
	Implementation Group
--------------------------------------------------------------------*/
#ifdef CONFIG_PM
/**
 *
 * suspending module.
 *
 * @param	struct platform_device *pdev pm_message_t state
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int gfx_suspend(struct platform_device *pdev, pm_message_t state)
 {

	GFX_DRVDATA_T *drv_data;
/* 	int count = 0; */

	drv_data = platform_get_drvdata(pdev);

	/*  add here the suspend code */
/*
	while(1)
	{
		if(gfx_isgfxidle() == TRUE)
			break;
		if(count++ > 10)
			return -1;
		else
		{
			mdelay(10);
		}

	}

*/
	gfx_runsuspend();
	drv_data->is_suspended = 1;
/*	GFXDBG("[%s] done suspend\n", GFX0_MODULE); */

	return 0;
 }

/**
 *
 * resuming module.
 *
 * @param	struct platform_device *
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int gfx_resume(struct platform_device *pdev)
 {
	GFX_DRVDATA_T *drv_data;

	drv_data = platform_get_drvdata(pdev);

	if (drv_data->is_suspended == 0) return -1;

	/*  add here the resume code */
	gfx_runresume();

	drv_data->is_suspended = 0;
/*	GFXDBG("[%s] done resume\n", GFX0_MODULE); */
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id odin_gfx_match[] = {
	{
		.compatible	= "odin-gfx0",
	},
};
#endif
static void gfx_release(struct device *dev);

const struct file_operations gfx_fops = {
	.owner 			= THIS_MODULE,
	.open 			= gfx_open,
	.release 			= gfx_close,
	.unlocked_ioctl 	= gfx_ioctl,
	.mmap 			= gfx_mmap,
};

struct miscdevice gfx_misc_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= DRV_NAME,
	.mode 	= 0660,
	.fops  	= &gfx_fops,
};

/**
 *
 * probing module.
 *
 * @param	struct platform_device *pdev
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
 int /*__init*/ gfx_probe(struct platform_device *pdev)
{

	int r;
	GFX_DRVDATA_T *drv_data;

	drv_data = (GFX_DRVDATA_T *)kmalloc(sizeof(GFX_DRVDATA_T) , GFP_KERNEL);

	/*  add here driver registering code & allocating resource code */
#ifdef CONFIG_OF
	pdev->dev.release = gfx_release;
#endif

	dev = &pdev->dev;

	printk("%s[%d]", __F__, __L__);

	r  = misc_register(&gfx_misc_device);
	if (r ) {
		printk("gfx0 : failed to register misc device.\n");
		return r;
	}

	r = odin_pd_register_dev(&pdev->dev, &odin_pd_dss4);
	if (r < 0)
	{
		printk("failed to register power domain\n");
	}

	/* FIXME: comment */
/*	pm_runtime_get_noresume(&pdev->dev);	*/
/*	pm_runtime_set_active(&pdev->dev);	*/
	pm_runtime_enable(&pdev->dev);

	GFXDBG("[%s] done probe\n", GFX0_MODULE);
	drv_data->is_suspended = 0;
	platform_set_drvdata(pdev, drv_data);

	return 0;
}


/**
 *
 * module remove function. this function will be called in rmmod fbdev module.
 *
 * @param	struct platform_device
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static int  gfx_remove(struct platform_device *pdev)
{
	GFX_DRVDATA_T *drv_data;

	/*  add here driver unregistering code & deallocating resource code */

	drv_data = platform_get_drvdata(pdev);
	misc_deregister(&gfx_misc_device);
	kfree(drv_data);

	GFXDBG("released\n");

	return 0;
}


/**
 *
 * module release function. this function will be called in rmmod module.
 *
 * @param	struct device *dev
 * @return	int 0 : OK , -1 : NOT OK
 *
 */
static void gfx_release(struct device *dev)
{
	GFXDBG("device released\n");
}


static int gfx_runtime_suspend(struct device *dev)
{
	odin_crg_dss_clk_ena(CRG_CORE_CLK, GFX0_CLK | GFX1_CLK, false);
	printk("[GFX] %s\n", __F__);
	return 0;
}

static int gfx_runtime_resume(struct device *dev)
{
	odin_crg_dss_clk_ena(CRG_CORE_CLK, GFX0_CLK | GFX1_CLK, true);
	printk("[GFX] %s\n", __F__);
	return 0;
}


/*
 *	module platform driver structure
 */
static const struct dev_pm_ops gfx_pm_ops = {
	.runtime_suspend = gfx_runtime_suspend,
	.runtime_resume = gfx_runtime_resume,
};

static struct platform_driver odin_gfx_driver =
{
	.probe          = gfx_probe,
	.suspend        = gfx_suspend,
	.remove         = gfx_remove,
	.resume         = gfx_resume,
	.driver         =
	{
		.name   = GFX0_MODULE,
		.pm	= &gfx_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(odin_gfx_match),
#endif
	},
};

#ifndef CONFIG_OF
static struct platform_device gfx_device = {
	.name = GFX0_MODULE,
	.id = 0,
	.id = -1,
	.dev = {
		.release = gfx_release,
	},
};
#endif
#endif

/** initialize GFX deivce. this function is called from base_dev_cfg.c
 *	for the large single kernel driver
 */
int gfx_init(void)
{
	int			i;
	int			err;
	dev_t		dev;

	/* Get the handle of debug output for gfx device.
	 *
	 * Most module should open debug handle before the
	 * real initialization of module.
	 * As you know, debug_util offers 4 independent debug outputs
	 * for your device driver.
	 * So if you want to use all the debug outputs, you should initialize
	 * each debug output
	 * using OS_DEBUG_EnableModuleByIndex() function.
	 */
	g_gfx_trace_depth	= 0;
#if 0
	g_gfx_debug_fd		= DBG_OPEN( GFX0_MODULE );

	if ( g_gfx_debug_fd < 0 )
	{
		printk("[GFX] can't get debug handle\n");
	}
	else
	{
	OS_DEBUG_EnableModule ( g_gfx_debug_fd );
/* 	OS_DEBUG_EnableModuleByIndex ( g_gfx_debug_fd, 0, DBG_COLOR_NONE ); */
/* 	OS_DEBUG_EnableModuleByIndex ( g_gfx_debug_fd, 1, DBG_COLOR_GREEN ); */
/* 	OS_DEBUG_EnableModuleByIndex ( g_gfx_debug_fd, 2, DBG_COLOR_YELLOW );*/
/*	OS_DEBUG_EnableModuleByIndex ( g_gfx_debug_fd, 3, DBG_COLOR_RED ); */
	}
#endif

	/* allocate main device handler, register current device.
	 *
	 * If device major is predefined then register device using that number.
	 * otherwise, major number of device is automatically assigned by
	 * Linux kernel.
	 */

#ifdef CONFIG_PM
	/* added by SC Jung for quick booting  */
	if (platform_driver_register(&odin_gfx_driver) < 0)
	{
		GFXDBG("[%s] platform driver register failed\n",GFX0_MODULE);
	}
	else
	{
#ifndef CONFIG_OF
		if (platform_device_register(&gfx_device))
		{
			platform_driver_unregister(&odin_gfx_driver);
			GFXDBG("[%s] platform device register failed\n",GFX0_MODULE);
		}
		else
		{
			GFXDBG("[%s] platform register done\n", GFX0_MODULE);
		}
#endif
	}
#endif
	g_gfx_device =
		(GFX_DEVICE_T*)OS_Malloc( sizeof(GFX_DEVICE_T)*GFX_MAX_DEVICE);

	if ( NULL == g_gfx_device )
	{
		GFXERR("out of memory. can't allocate %d bytes\n",
			sizeof(GFX_DEVICE_T)* GFX_MAX_DEVICE );
		return -ENOMEM;
	}

	memset( g_gfx_device, 0x0, sizeof(GFX_DEVICE_T)*GFX_MAX_DEVICE );

	if (g_gfx_major)
	{
		dev = MKDEV( g_gfx_major, g_gfx_minor );
		err = register_chrdev_region(dev, GFX_MAX_DEVICE, GFX0_MODULE );
	}
	else
	{
		err = alloc_chrdev_region(&dev, g_gfx_minor, GFX_MAX_DEVICE, GFX0_MODULE );
		g_gfx_major = MAJOR(dev);
	}

	if ( err < 0 )
	{
		GFXERR("can't register gfx device\n" );
		return -EIO;
	}

	/* TODO : initialize your module not specific minor device */
	OS_InitMutex ( &gfx_dev_mtx, OS_SEM_ATTR_DEFAULT );

	/*  To do : Check whether this function is proper.. cherith */
	gfx_preinit();

	gfx_inithw();
		/* initialize H/W for GFX to be used from kdriver initialization */

   	gfx_initsurfacememory();
   		/* initialize dynamic surface memory */
	gfx_setcommanddelay(0x0);
    	/* set default cmd delay. the value came from verification test */

	/* END */

#ifndef CONFIG_ODIN_DUAL_GFX
	/* power gating for gfx */
	odin_pd_off(PD_DSS, 4);
	g_gfx_cfg.power_status = 0;
	GFXINFO("gfx0, gfx1 power is disabled\n");
#endif

	for ( i=0; i<GFX_MAX_DEVICE; i++ )
	{
		/* initialize cdev structure with predefined variable */
		dev = MKDEV( g_gfx_major, g_gfx_minor+i );
		cdev_init( &(g_gfx_device[i].cdev), &g_gfx_fops );
		g_gfx_device[i].devno		= dev;
		g_gfx_device[i].cdev.owner	= THIS_MODULE;
		g_gfx_device[i].cdev.ops	= &g_gfx_fops;

		/* TODO: initialize minor device */

		/* END */
		err = cdev_add (&(g_gfx_device[i].cdev), dev, 1 );

		if (err)
		{
			GFXERR("error (%d) while adding gfx device (%d.%d)\n",
				err, MAJOR(dev), MINOR(dev) );
			return -EIO;
		}

		os_createdeviceclass ( g_gfx_device[i].devno, "%s%d", GFX0_MODULE, i );
	}

	os_proc_init();

	/* initialize proc system */
	gfx_proc_init ( );

	pm_qos_add_request(&mem_qos, PM_QOS_ODIN_MEM_MIN_FREQ, 0);

	GFXINFO("gfx0 device is initialized\n");

	return 0;
}

void gfx_cleanup(void)
{
	int i;
	dev_t dev = MKDEV( g_gfx_major, g_gfx_minor );

#ifdef CONFIG_PM
	/* added by SC Jung for quick booting	*/
	platform_driver_unregister(&odin_gfx_driver);
#ifndef CONFIG_OF
	platform_device_unregister(&gfx_device);
#endif
#endif

	/* cleanup proc system */
	gfx_proc_cleanup( );

	/* remove all minor devicies and unregister current device */
	for ( i=0; i<GFX_MAX_DEVICE;i++)
	{
		/* TODO: cleanup each minor device */


		/* END */
		cdev_del( &(g_gfx_device[i].cdev) );
	}

	/* TODO : cleanup your module not specific minor device */

	unregister_chrdev_region(dev, GFX_MAX_DEVICE );

	OS_Free( g_gfx_device );
}


/**
 * open handler for gfx device
 *
 */
static int
gfx_open(struct inode *inode, struct file *filp)
{
	GFX_DEV_CTX_T *dev_ctx;
    int	major,minor;
    struct cdev *cdev;
    GFX_DEVICE_T *my_dev;

    major	= imajor(inode);
    minor	= iminor(inode);
    cdev	= inode->i_cdev;
    my_dev	= container_of ( cdev, GFX_DEVICE_T, cdev);

    /* TODO : add your device specific code */
	if ( my_dev->dev_open_count == 0 )
	{
		/* do nothing */
	}

    my_dev->dev_open_count++;

	GFXINFO("gfx_open dev_open_count %d\n", my_dev->dev_open_count);

	/* allocate device context */
	dev_ctx = kcalloc(1, sizeof(GFX_DEV_CTX_T), GFP_KERNEL);
	GFX_CHECK_ERROR( NULL==dev_ctx, /* nop */, "out of memory\n");
	dev_ctx->cached_mmap 	= FALSE;
    filp->private_data 		= dev_ctx;

    GFXINFO("device opened (%d:%d)\n", major, minor );

    return RET_OK;
}

/**
 * release handler for gfx device
 *
 */
static int gfx_close(struct inode *inode, struct file *file)
{
    int major,minor;
	GFX_DEV_CTX_T *dev_ctx;
    GFX_DEVICE_T *my_dev;
    struct cdev *cdev;

    major	= imajor(inode);
    minor	= iminor(inode);
    cdev	= inode->i_cdev;
    my_dev	= container_of ( cdev, GFX_DEVICE_T, cdev);
	dev_ctx = (GFX_DEV_CTX_T*)file->private_data;

    if ( my_dev->dev_open_count > 0 )
    {
        --my_dev->dev_open_count;
    }

    /* TODO : add your device specific code */
	if ( dev_ctx )
	{
		kfree(dev_ctx);
	}
	/* END */

	/* some debug */
/*    GFXDBG("device closed (%d:%d)\n", major, minor ); */
    return RET_OK;
}

/**
 * memory mapping to virtual region
 *
 */
static int gfx_mmap(struct file *file, struct vm_area_struct *vma)
{
	int	ret = 0;
	GFXDBG("%s : BEGIN\n", __F__ );
/* 	ret = gfx_surfacemmap( file, vma); */
	GFXDBG("%s : END\n", __F__ );

	return ret;
}

/**
 * ioctl handler for gfx device.
 *
 *
 * note: if you have some critial data, you should protect them
 * using semaphore or spin lock.
 */
/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
	|| !defined(SUPPORT_GFX_UNLOCKED_IOCTL) */
/* static int gfx_ioctl ( struct inode* inode, struct file *filp,
 * u32 cmd, ULONG arg ) */
/* #else */
static long gfx_ioctl ( struct file* filp, u32 cmd, ULONG arg )
/* #endif */
{
	int ret = RET_ERROR;
    int err = 0;

    GFX_DEV_CTX_T *dev_ctx;

	/*
	 * get current gfx device object
	 */
/* #if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
 * || !defined(SUPPORT_GFX_UNLOCKED_IOCTL) */
/*     struct cdev* cdev; */
/* 	cdev	= inode->i_cdev; */
/*     dev_ctx	= container_of ( cdev, GFX_DEV_CTX_T, cdev); */
/* #else */
	dev_ctx	= (GFX_DEV_CTX_T*)filp->private_data;
/* #endif */

	GFX_TRACE_BEGIN();

    /*
     * check if IOCTL command is valid or not.
     * - if magic value doesn't match, return error (-ENOTTY)
     * - if command is out of range, return error (-ENOTTY)
     *
     * note) -ENOTTY means "Inappropriate ioctl for device.
     */
    if (_IOC_TYPE(cmd) != GFX_IOC_MAGIC)
    {
    	GFXWARN("invalid magic. magic=0x%02X\n", _IOC_TYPE(cmd) );
    	GFX_TRACE_END();
    	return -ENOTTY;
    }
    if (_IOC_NR(cmd) > GFX_IOC_MAXNR)
    {
    	GFXWARN("out of ioctl command. cmd_idx=%d\n", _IOC_NR(cmd) );
    	GFX_TRACE_END();
    	return -ENOTTY;
    }

	/* TODO : add some check routine for your device */

    /*
     * check if user memory is valid or not.
     * if memory can't be accessed from kernel, return error (-EFAULT)
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (err)
    {
    	GFXWARN("memory access error. cmd_idx=%d, rw=%c%c, memptr=%p\n",
				_IOC_NR(cmd),(_IOC_DIR(cmd) & _IOC_READ)? 'r':'-',
				(_IOC_DIR(cmd) & _IOC_WRITE)? 'w':'-', (void*)arg );
    	GFX_TRACE_END();
        return -EFAULT;
	}

/*	GFXDBG("%s : IOC 0x%08x (%s:%d)\n", __F__, cmd, gfx_ioc2str(cmd),
			_IOC_NR(cmd)); */

	switch (cmd)
	{
		case GFX_IO_RESET:
		{
			GFXINFO("reset ok\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_CHIP_REV_INFO:
		{
			LX_CHIP_REV_INFO_T	rev_info;

			rev_info.version = 0x100;
			rev_info.date[0] = 12;	/* 2012/12/25 */
			rev_info.date[1] = 12;
			rev_info.date[2] = 25;

			err = copy_to_user((void __user *)arg, &rev_info, sizeof(CHIP_REV_INFO_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_CFG:
		{
			err = copy_to_user((void __user *)arg, (void *)&g_gfx_cfg ,
				sizeof(LX_GFX_CFG_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IORW_ALLOC_SURFACE :
		{
			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			LX_GFX_SURFACE_SETTING_PARAM_T param;

			err = copy_from_user((void *)&param, (void __user *)arg,
				sizeof(LX_GFX_SURFACE_SETTING_PARAM_T));

			gfx_lockdevice();
			err = gfx_allocsurface(&param);
			gfx_unlockdevice();

			GFX_CHECK_ERROR( RET_OK != err, goto func_exit,
				"gfx_allocsurface failed\n" );

			err = copy_to_user((void __user *)arg, (void *)&param,
				sizeof(LX_GFX_SURFACE_SETTING_PARAM_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_FREE_SURFACE :
		{
			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			u32	surface_fd;

			err = copy_from_user((void *)&surface_fd, (void __user *)arg, sizeof(u32));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			gfx_lockdevice();
			err = gfx_freesurface(surface_fd);
			gfx_unlockdevice();
			GFX_CHECK_ERROR( RET_OK != err, goto func_exit,
				"[GFX] gfx_freesurface failed\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_SURFACE_PALETTE :
		{
			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			LX_GFX_SURFACE_PALETTE_PARAM_T param;
			LX_GFX_SURFACE_PALETTE_PARAM_T *input_param =
				(LX_GFX_SURFACE_PALETTE_PARAM_T*)arg;

			/*	raxis.lim (2010/06/05)
			 *	--	palette data is passed as pointer, so we should
			 *	copy it from user space
		 	 */
			param.palette.palette_data 	= (u32*)OS_Malloc(sizeof(u32)*256);

			if ( __get_user( param.surface_fd, &(input_param->surface_fd) ) )
			{
				ret = RET_ERROR; goto func_exit;
			}

			if ( __get_user( param.palette.palette_num,
				&(input_param->palette.palette_num) ) )
			{
				ret = RET_ERROR; goto func_exit;
			}

			err = copy_from_user((void*)param.palette.palette_data,
				(void __user*)input_param->palette.palette_data,
				sizeof(u32)*input_param->palette.palette_num );

			GFX_CHECK_ERROR( err > 0, OS_Free(param.palette.palette_data);
				goto func_exit, "copy error\n");

			gfx_lockdevice();
			gfx_setsurfacepalette( param.surface_fd, param.palette.palette_num,
			param.palette.palette_data );
			gfx_unlockdevice();

			OS_Free(param.palette.palette_data);
			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_SURFACE_PALETTE :
		{
			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			LX_GFX_SURFACE_PALETTE_PARAM_T param;
			LX_GFX_SURFACE_PALETTE_PARAM_T *input_param =
				(LX_GFX_SURFACE_PALETTE_PARAM_T*)arg;

			param.palette.palette_data 	= (u32*)OS_Malloc(sizeof(u32)*256);
			param.palette.palette_num 	= 256;

			if ( __get_user( param.surface_fd, &(input_param->surface_fd) ) )
			{
				ret = RET_ERROR; goto func_exit;
			}

			if ( __get_user( param.palette.palette_num,
				&(input_param->palette.palette_num) ) )
			{
				ret = RET_ERROR; goto func_exit;
			}

			gfx_lockdevice();
			gfx_getsurfacepalette( param.surface_fd, param.palette.palette_num,
				param.palette.palette_data );
			gfx_unlockdevice();

			err = copy_to_user( (void __user*)input_param->palette.palette_data,
				(void*)param.palette.palette_data,
				sizeof(u32)*input_param->palette.palette_num );

			GFX_CHECK_ERROR( err > 0, OS_Free(param.palette.palette_data);
				goto func_exit, "copy error\n");

			OS_Free(param.palette.palette_data);
			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_POWERSTATUS :
		{
			err = copy_to_user((void __user *)arg, (void *)&g_gfx_cfg.power_status,
				sizeof(u32));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_POWERSTATUS :
		{
			u32 power_state =0;
			int i = 0;

			err = copy_from_user((void *)&power_state, (void __user *)arg,
				sizeof(u32));
           		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			g_gfx_cfg.power_status = power_state;

			if (power_state == 0x1)
			{
				pm_qos_update_request(&mem_qos, 800000);
/*
				odin_crg_gfx_swreset(1);
				mdelay(1);
				odin_pd_on(PD_DSS, 4);
				mdelay(3);
				odin_crg_gfx_swreset(0);
*/
				pm_runtime_get_sync(dev);
				udelay(1);
				gfx_restart_function();

				GFXINFO("gfx domain clock/power is enabled\n");
			}
			else
			{
				gfx_stop_function();
				udelay(1);

				for ( i=10; i>0; i-- )
				{
					if (g_gfx_working == 0) break;
					msleep(10);
				}

				g_gfx_working = 0;

				pm_qos_update_request(&mem_qos, 200000);
				pm_runtime_put_sync(dev);

/*				odin_pd_off(PD_DSS, 4);	*/

				GFXINFO("gfx domain clock/power is disabled..\n");
			}

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_DUAL_GFX :
		{
			u32 dual_gfx =0;

			err = copy_from_user((void *)&dual_gfx, (void __user *)arg,
				sizeof(u32));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			g_fhd_flag = dual_gfx;
			/* printk("DualGFX enable = %d\n", g_fhd_flag ); */
		}
		break;

		case GFX_IOR_GET_OPER_MODE :
		{
			err = copy_to_user((void __user *)arg, (void *)&g_gfx_oper_status,
				sizeof(u32));
            		GFX_CHECK_CODE( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_OPER_MODE :
		{
			LX_GFX_OPER_MODE_T oper_mode;

			err = copy_from_user((void *)&oper_mode, (void __user *)arg,
				sizeof(LX_GFX_OPER_MODE_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			if (oper_mode.enable)
				g_gfx_oper_status |= oper_mode.operation_flag;
			else
				g_gfx_oper_status &= ~(oper_mode.operation_flag);

			printk("GFX current operation status = %d\n", g_gfx_oper_status);

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_BLEND :
		case GFX_IOW_MANUAL_BLEND :
		{
			/*GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");*/
			if(g_gfx_cfg.power_status == 0){
				g_gfx_working = 0;
				GFXINFO("ioctl mamual blend GFX Power Off..\n" );
				return RET_ERROR;
			}

			g_gfx_working = 1;

			int	retry;
			LX_GFX_MANUAL_BLEND_CTRL_PARAM_T manual_blend;

			memset( &manual_blend, 0x0,
				sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T));
			err = copy_from_user((void *)&manual_blend, (void __user *)arg,
				sizeof(LX_GFX_MANUAL_BLEND_CTRL_PARAM_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			gfx_lockdevice();

			for ( retry=g_gfx_cfg.sync_fail_retry_count ; retry>0 ; retry-- )
			{
				if(g_gfx_cfg.power_status == 0){
					err = RET_ERROR;
					g_gfx_working = 0;
					break;
				}
				err = gfx_runblendop(&manual_blend);

			    /* when batch mode is auto, driver automatically
			    	flush queue (start batch run) */
			    if (gfx_getruncommand() == LX_GFX_BATCH_RUN_MODE_AUTO)
				{
					gfx_runflushcommand();
				}

				/* g_fhd_flag = 0x0 in case of FHD, g_fhd_flag = 0x1 except FHD */
				if ( g_fhd_flag == 0x0)
				{
					/* raxis.lim (2011/05/21)
					 * since there is some bugs in L9A0 scaler read/write timing
					 *	problem, I should add s/w workaround
					 * to do "soft reset" for GFX block every scaler request.
					 * [NOTE] check if L9B0 fixes this bug
				 	 *
				 	 * raxis.lim (2012/12/27)
				 	 * GFX soft reset is moved into gfx_waitsynccommand.
					 * GFX soft reset will be done after every GFX operation.
					 */
					if ( gfx_getgraphicsyncmode() == LX_GFX_GRAPHIC_SYNC_MODE_AUTO )
					{
						u32	timeout = gfx_calcsynctimeout( &manual_blend,
							g_gfx_cfg.sync_wait_timeout );
						err = gfx_waitsynccommand( timeout );
#if 0
					if ( GFX_WADesc(scaler_read_buf_stuck)
						&& manual_blend.scaler.mode )
					{
						gfx_swresetcommand();
					}
#endif
					}
				}
				else
				{
					/* printk("GFX0 is skipped\n" ); */
				}
				/* if GFX operation failed, retry it */
				if ( err == RET_OK ) break;
#ifdef GFX_USE_NOISY_ERR_DEBUG
				/* if ERROR, report the requet info */
				gfx_dumpblitparam ( &manual_blend );
#endif
			}
			gfx_unlockdevice();

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_CSC_CONV_TBL:
		{
			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			LX_GFX_CSC_TBL_T csc_conv;

			err = copy_from_user((void *)&csc_conv, (void __user *)arg,
				sizeof(LX_GFX_CSC_TBL_T));
           		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			err = gfx_setcolorspace(csc_conv.coef);
           		 GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_setcolorspace error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_CMD_DELAY :
		{
			u32	cmd_delay;

			gfx_getcommanddelay(&cmd_delay);

			err = copy_to_user((void __user *)arg, (void *)&cmd_delay,
				sizeof(u32));
            		GFX_CHECK_CODE( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_CMD_DELAY :
		{
			u32	cmd_delay;

			err = copy_from_user((void *)&cmd_delay, (void __user *)arg,
				sizeof(u32));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			gfx_setcommanddelay(cmd_delay);

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IORW_GET_SURFACE_MEM_INFO:
		{
			LX_GFX_SURFACE_MEM_INFO_PARAM_T mem_info;

			err = copy_from_user((void *)&mem_info, (void __user *)arg,
				sizeof(LX_GFX_SURFACE_MEM_INFO_PARAM_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			gfx_lockdevice();
			err = gfx_getsurfacememory(&mem_info);
			gfx_unlockdevice();

			GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_getsurfacememory error\n");

			err = copy_to_user((void __user *)arg, (void *)&mem_info,
				sizeof(LX_GFX_SURFACE_MEM_INFO_PARAM_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_BATCH_RUN_MODE:
		{
			LX_GFX_BATCH_RUN_MODE_T	run_mode;

			err = copy_from_user((void *)&run_mode, (void __user *)arg,
				sizeof(LX_GFX_BATCH_RUN_MODE_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			err = gfx_setruncommand(&run_mode);
            		GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_setruncommand error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_BATCH_RUN_MODE:
		{
			LX_GFX_BATCH_RUN_MODE_T	run_mode;

			run_mode = gfx_getruncommand();

			err = copy_to_user((void __user *)arg, (void *)&run_mode,
				sizeof(LX_GFX_BATCH_RUN_MODE_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IO_START_BATCH_RUN:
		{
			err = gfx_runflushcommand();
            		GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_runflushcommand error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_SET_GRAPHIC_SYNC_MODE:
		{
			LX_GFX_GRAPHIC_SYNC_MODE_T	mode;

			err = copy_from_user((void *)&mode, (void __user *)arg,
				sizeof(LX_GFX_GRAPHIC_SYNC_MODE_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			err = gfx_setgraphicsyncmode(mode);
           		GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_setgraphicsyncmode error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOW_WAIT_FOR_SYNC:
		{
			u32	msec_tm;

			GFX_CHECK_ERROR( g_gfx_cfg.power_status == 0, goto func_exit,
				"power error\n");

			err = copy_from_user((void *)&msec_tm, (void __user *)arg,
				sizeof(u32));
            		GFX_CHECK_CODE( err > 0, goto func_exit, "copy error\n");

			err = gfx_waitsynccommand(msec_tm);
            		GFX_CHECK_CODE( err != RET_OK, goto func_exit,
            			"gfx_waitsynccommand error\n");

			ret = RET_OK; /* all work done */
		}
		break;

		case GFX_IOR_GET_MEM_STAT:
		{
			LX_GFX_MEM_STAT_T mem_stat;
			(void)gfx_getsurfacememorystat( &mem_stat );

			err = copy_to_user((void __user *)arg, (void *)&mem_stat,
				sizeof(LX_GFX_MEM_STAT_T));
            		GFX_CHECK_ERROR( err > 0, goto func_exit, "copy error\n");

			ret = RET_OK; /* all work done */
		}
		break;

	    	default:
	    	{
			/* redundant check but it seems more readable */
    	    		ret = -ENOTTY;
		}
    }

func_exit:
/*	GFXINFO("%s : IOC 0x%08x (%s:%d) -- ret %d\n", __F__, cmd,
		gfx_ioc2str(cmd), _IOC_NR(cmd), ret ); */
/* 	Cherith,, need for dubugging */
	GFX_TRACE_END();

    return ret;
}


module_init(gfx_init);
module_exit(gfx_cleanup);

MODULE_AUTHOR("");
MODULE_DESCRIPTION("GFX0 base driver");
MODULE_LICENSE("GPL v2");
