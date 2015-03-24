#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include "disp_ovl_engine_api.h"
#include "disp_ovl_engine_core.h"
#include "disp_ovl_engine_dev.h"
#include "disp_ovl_engine_hw.h"


#define DISP_OVL_ENGINE_DEVNAME "mtk_ovl"
/* device and driver */
static dev_t disp_ovl_engine_devno;
static struct cdev *disp_ovl_engine_cdev;
static struct class *disp_ovl_engine_class;


/* Inode to instance id mapping */
#define DISP_OVL_ENGINE_INODE_NUM 16
typedef struct {
	struct file *mFile;
	DISP_OVL_ENGINE_INSTANCE_HANDLE mHandle;
} DISP_OVL_ENGINE_FILE_HANDLE_MAP;

DISP_OVL_ENGINE_FILE_HANDLE_MAP disp_ovl_engine_file_handle_map[DISP_OVL_ENGINE_INODE_NUM];



static int disp_ovl_engine_open(struct inode *inode, struct file *file)
{
	int i;
#ifdef DISP_OVL_ENGINE_REQUEST
	static int first = 1;
#endif

	/* Find empty mapping */
	for (i = 0; i < DISP_OVL_ENGINE_INODE_NUM; i++) {
		if (disp_ovl_engine_file_handle_map[i].mFile == NULL) {
			disp_ovl_engine_file_handle_map[i].mFile = file;
			break;
		}
	}

#ifdef DISP_OVL_ENGINE_REQUEST
	/* For OvlFd */
	if (first) {
		first = 0;

		return 0;
	}
#endif

	if (i < DISP_OVL_ENGINE_INODE_NUM) {
		if (Disp_Ovl_Engine_GetInstance
		    (&(disp_ovl_engine_file_handle_map[i].mHandle), DECOUPLE_MODE) < 0) {
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}


static int disp_ovl_engine_release(struct inode *inode, struct file *file)
{
	int i;

	/* Find match mapping */
	for (i = 0; i < DISP_OVL_ENGINE_INODE_NUM; i++) {
		if (disp_ovl_engine_file_handle_map[i].mFile == file) {
			Disp_Ovl_Engine_ReleaseInstance(disp_ovl_engine_file_handle_map[i].mHandle);
			disp_ovl_engine_file_handle_map[i].mFile = NULL;
			break;
		}
	}

	return 0;
}


static long disp_ovl_engine_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int i;
	DISP_OVL_ENGINE_INSTANCE_HANDLE handle = 1;

	/* Find match mapping */
	for (i = 0; i < DISP_OVL_ENGINE_INODE_NUM; i++) {
		if (disp_ovl_engine_file_handle_map[i].mFile == file) {
			handle = disp_ovl_engine_file_handle_map[i].mHandle;
			break;
		}
	}
	if (is_early_suspended) {
		DISP_OVL_ENGINE_WARN("[disp_ovl_engine_unlocked_ioctl] is_early_suspended,cmd=0x%x\n", cmd);
		return -1;
	}
	if (i >= DISP_OVL_ENGINE_INODE_NUM) {
		DISP_OVL_ENGINE_ERR("[disp_ovl_engine_unlocked_ioctl] invalid file handle 0x%x\n", (unsigned int)file);
	}

	switch (cmd) {
	case DISP_OVL_ENGINE_IOCTL_GET_LAYER_INFO:
		{
			struct fb_overlay_layer laye_info;

			if (copy_from_user
			    (&laye_info, (void __user *)arg, sizeof(struct fb_overlay_layer))) {
				return -EFAULT;
			}

			Disp_Ovl_Engine_Get_layer_info(handle, &laye_info);

			if (copy_to_user
			    ((void __user *)arg, &laye_info, sizeof(struct fb_overlay_layer))) {
				return -EFAULT;
			}
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_SET_LAYER_INFO:
		{
			struct fb_overlay_layer laye_info;

			if (copy_from_user
			    (&laye_info, (void __user *)arg, sizeof(struct fb_overlay_layer))) {
				return -EFAULT;
			}

			Disp_Ovl_Engine_Set_layer_info(handle, &laye_info);
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_SET_OVERLAYED_BUF:
		{
			struct disp_ovl_engine_config_mem_out_struct overlayedBufInfo;

			if (copy_from_user
			    (&overlayedBufInfo, (void __user *)arg,
			     sizeof(struct disp_ovl_engine_config_mem_out_struct))) {
				return -EFAULT;
			}

			return Disp_Ovl_Engine_Set_Overlayed_Buffer(handle, &overlayedBufInfo);
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_TRIGGER_OVERLAY:
		{
			return Disp_Ovl_Engine_Trigger_Overlay(handle);
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_TRIGGER_OVERLAY_FENCE:
		{
			struct disp_ovl_engine_setting setting;
			int ret;

			if (copy_from_user
			    (&setting, (void __user *)arg,
			     sizeof(struct disp_ovl_engine_setting))) {
				return -EFAULT;
			}

			ret = Disp_Ovl_Engine_Trigger_Overlay_Fence(handle, &setting);

			if (copy_to_user
			    ((void __user *)arg, &setting,
			     sizeof(struct disp_ovl_engine_setting))) {
				return -EFAULT;
			}

			return ret;
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_GET_FENCE_FD:
		{
			int ret;
			int fence_fd;

			ret = Disp_Ovl_Engine_Get_Fence_Fd(handle, &fence_fd);

			if (copy_to_user((void __user *)arg, &fence_fd, sizeof(int))) {
				return -EFAULT;
			}

			return ret;
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_WAIT_OVERLAY_COMPLETE:
		{
			int timeout = arg;

			return Disp_Ovl_Engine_Wait_Overlay_Complete(handle, timeout);
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_SECURE_MVA_MAP:
		{
			struct disp_mva_map mva_map_struct;
			if (copy_from_user
			    (&mva_map_struct, (void *)arg, sizeof(struct disp_mva_map))) {
				DISP_OVL_ENGINE_ERR("DISP_SECURE_MVA_MAP, copy_from_user failed\n");
				return -EFAULT;
			}
			DISP_OVL_ENGINE_INFO
			    ("map_mva, module=%d, cache=%d, addr=0x%x, size=0x%x\n",
			     mva_map_struct.module, mva_map_struct.cache_coherent,
			     mva_map_struct.addr, mva_map_struct.size);

			if (mva_map_struct.addr == 0) {
				DISP_OVL_ENGINE_ERR("invalid parameter for DISP_SECURE_MVA_MAP!\n");
			} else {
				disp_ovl_engine_hw_mva_map(&mva_map_struct);
			}
		}
		break;

	case DISP_OVL_ENGINE_IOCTL_SECURE_MVA_UNMAP:
		{
			struct disp_mva_map mva_map_struct;
			if (copy_from_user
			    (&mva_map_struct, (void *)arg, sizeof(struct disp_mva_map))) {
				DISP_OVL_ENGINE_ERR
				    ("DISP_SECURE_MVA_UNMAP, copy_from_user failed\n");
				return -EFAULT;
			}
			DISP_OVL_ENGINE_INFO
			    ("unmap_mva, module=%d, cache=%d, addr=0x%x, size=0x%x\n",
			     mva_map_struct.module, mva_map_struct.cache_coherent,
			     mva_map_struct.addr, mva_map_struct.size);

			if (mva_map_struct.addr == 0) {
				DISP_OVL_ENGINE_ERR
				    ("invalid parameter for DISP_SECURE_MVA_UNMAP!\n");
			} else {
				disp_ovl_engine_hw_mva_unmap(&mva_map_struct);
			}
		}
		break;
#ifdef DISP_OVL_ENGINE_REQUEST
	case DISP_OVL_ENGINE_IOCTL_GET_REQUEST:
		{
			struct disp_ovl_engine_request_struct overlayRequest;

			if (Disp_Ovl_Engine_Get_Request(&overlayRequest)) {
				return -EFAULT;
			}

			if (copy_to_user
			    ((void __user *)arg, &overlayRequest,
			     sizeof(struct disp_ovl_engine_request_struct))) {
				return -EFAULT;
			}
		}
		break;
	case DISP_OVL_ENGINE_IOCTL_ACK_REQUEST:
		{
			struct disp_ovl_engine_request_struct overlayRequest;

			if (copy_from_user
			    (&overlayRequest, (void __user *)arg,
			     sizeof(struct disp_ovl_engine_request_struct))) {
				return -EFAULT;
			}

			return Disp_Ovl_Engine_Ack_Request(&overlayRequest);
		}
		break;
#endif
	}

	return 0;
}



/* Kernel interface */
static struct file_operations disp_ovl_engine_fops = {
	.owner = THIS_MODULE,
	.open = disp_ovl_engine_open,
	.release = disp_ovl_engine_release,
	.unlocked_ioctl = disp_ovl_engine_unlocked_ioctl,
};



static int disp_ovl_engine_probe(struct platform_device *pdev)
{
	int ret;
	struct class_device *class_dev = NULL;

	ret = alloc_chrdev_region(&disp_ovl_engine_devno, 0, 1, DISP_OVL_ENGINE_DEVNAME);

	disp_ovl_engine_cdev = cdev_alloc();
	disp_ovl_engine_cdev->owner = THIS_MODULE;
	disp_ovl_engine_cdev->ops = &disp_ovl_engine_fops;

	ret = cdev_add(disp_ovl_engine_cdev, disp_ovl_engine_devno, 1);

	disp_ovl_engine_class = class_create(THIS_MODULE, DISP_OVL_ENGINE_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(disp_ovl_engine_class, NULL, disp_ovl_engine_devno,
						 NULL, DISP_OVL_ENGINE_DEVNAME);


	return 0;
}


static int disp_ovl_engine_remove(struct platform_device *pdev)
{
	return 0;
}



static struct platform_driver disp_ovl_engine_driver = {
	.probe = disp_ovl_engine_probe,
	.remove = disp_ovl_engine_remove,
	.driver = {
		   .name = DISP_OVL_ENGINE_DEVNAME,
		   },
};


static struct platform_device disp_ovl_engine_device = {
	.name = DISP_OVL_ENGINE_DEVNAME,
	.id = 0,
	.dev = {
		},
	.num_resources = 0,
};


static int __init _disp_ovl_engine_init(void)
{
	disp_ovl_engine_init();
	memset(disp_ovl_engine_file_handle_map, 0,
	       sizeof(DISP_OVL_ENGINE_FILE_HANDLE_MAP) * DISP_OVL_ENGINE_INODE_NUM);

	if (platform_device_register(&disp_ovl_engine_device)) {
		return -ENODEV;
	}

	if (platform_driver_register(&disp_ovl_engine_driver)) {
		platform_device_unregister(&disp_ovl_engine_device);
		return -ENODEV;
	}

	return 0;
}


static void __exit _disp_ovl_engine_exit(void)
{
	cdev_del(disp_ovl_engine_cdev);
	unregister_chrdev_region(disp_ovl_engine_devno, 1);

	platform_driver_unregister(&disp_ovl_engine_driver);
	platform_device_unregister(&disp_ovl_engine_device);

	device_destroy(disp_ovl_engine_class, disp_ovl_engine_devno);
	class_destroy(disp_ovl_engine_class);
}


module_init(_disp_ovl_engine_init);
module_exit(_disp_ovl_engine_exit);
MODULE_DESCRIPTION("Overlay engine");
MODULE_LICENSE("GPL");
