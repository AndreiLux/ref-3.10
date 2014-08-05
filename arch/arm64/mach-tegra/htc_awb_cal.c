/* arch/arm/mach-msm/htc_awb_cal.c */
/* Code to extract Camera AWB calibration information from ATAG
set up by the bootloader.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <asm/setup.h>

/* for outputing file to filesystem : /data/awb_calibration_data_hboot.txt */
#include <linux/fs.h>
#include <linux/syscalls.h>

#include <linux/of.h>

/* HTC_START, use dt for calibration data */
#define CALIBRATION_DATA_PATH "/calibration_data"
#define CAM_AWB_CAL_DATA "cam_awb"
#define AWB_CAL_SIZE	889  //size of calibration data

static ssize_t awb_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf, size_t index)
{
    ssize_t ret = AWB_CAL_SIZE;
    int p_size = 0;
    unsigned char *p_data = NULL;
    struct device_node *offset = of_find_node_by_path(CALIBRATION_DATA_PATH);

    if (offset) {
        /* of_get_property will return address of property, */
        /* and fill the length to *p_size */
        p_data = (unsigned char*) of_get_property(offset,
                                                    CAM_AWB_CAL_DATA, &p_size);
    }

	memcpy(buf, p_data + index, ret);

	return ret;
}

static ssize_t awb_calibration_back_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return awb_calibration_show(dev, attr, buf, 0);
}

static ssize_t awb_calibration_front_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return awb_calibration_show(dev, attr, buf, 0x1000);
}

static DEVICE_ATTR(factory, 0444, awb_calibration_back_show, NULL);
static DEVICE_ATTR(factory_front, 0444, awb_calibration_front_show, NULL);

static struct kobject *cam_awb_cal;

static int cam_get_awb_cal(void)
{
	int ret ;

	/* Create /sys/android_camera_awb_cal/awb_cal */
	cam_awb_cal = kobject_create_and_add("android_camera_awb_cal", NULL);
	if (cam_awb_cal == NULL) {
		pr_info("[CAM]%s: subsystem_register failed\n", __func__);
		return -ENOMEM;
	}

	ret = sysfs_create_file(cam_awb_cal, &dev_attr_factory.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		return ret;
	}

	ret = sysfs_create_file(cam_awb_cal, &dev_attr_factory_front.attr);
	if (ret) {
		pr_info("[CAM]cam_get_awb_cal_front:: sysfs_create_file failed\n");
		kobject_del(cam_awb_cal);
		return ret;
	}

	return 0 ;
}

late_initcall(cam_get_awb_cal);
