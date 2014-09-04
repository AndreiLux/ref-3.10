/* arch/arm/mach-msm/htc_awb_cal.c
 * Code to extract Camera AWB calibration information from ATAG
 * set up by the bootloader.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <asm/setup.h>

#include <linux/syscalls.h>
#include <linux/of.h>

const char awb_cam_cal_data_path[] = "/calibration_data";
const char awb_cam_cal_data_tag[] = "cam_awb";

const loff_t awb_cal_front_offset = 0x1000;	/* Offset of the front cam data */
#define AWB_CAL_SIZE 897	/* size of the calibration data */

static ssize_t awb_calibration_get(char *buf, size_t count, loff_t off,
				   bool is_front)
{
	int p_size = 0;
	unsigned char *p_data = NULL;
	struct device_node *dev_node = NULL;

	dev_node = of_find_node_by_path(awb_cam_cal_data_path);
	if (dev_node)
		/* of_get_property will return address of property,
		 * and fill the length to *p_size */
		p_data = (unsigned char *)of_get_property(dev_node,
							  awb_cam_cal_data_tag,
							  &p_size);

	if (p_data == NULL) {
		pr_err("%s: Unable to get calibration data", __func__);
		return 0;
	}

	if (off >= AWB_CAL_SIZE)
		return 0;

	if (off + count > AWB_CAL_SIZE)
		count = AWB_CAL_SIZE - off;

	p_data += is_front ? awb_cal_front_offset : 0;
	memcpy(buf, p_data + off, count);

	return count;
}

static ssize_t awb_calibration_back_read(struct file *filp,
					 struct kobject *kobj,
					 struct bin_attribute *attr,
					 char *buf, loff_t off, size_t count)
{
	return awb_calibration_get(buf, count, off, false);
}

static ssize_t awb_calibration_front_read(struct file *filp,
					  struct kobject *kobj,
					  struct bin_attribute *attr,
					  char *buf, loff_t off, size_t count)
{
	return awb_calibration_get(buf, count, off, true);
}

static struct bin_attribute factory_back = {
	.attr.name = "factory_back",
	.attr.mode = 0444,
	.size = AWB_CAL_SIZE,
	.read = awb_calibration_back_read,
};

static struct bin_attribute factory_front = {
	.attr.name = "factory_front",
	.attr.mode = 0444,
	.size = AWB_CAL_SIZE,
	.read = awb_calibration_front_read,
};

static int cam_get_awb_cal(void)
{
	int ret;
	struct kobject *cam_awb_cal;

	cam_awb_cal = kobject_create_and_add("camera_awb_cal", firmware_kobj);
	if (cam_awb_cal == NULL) {
		pr_err("%s: failed to create /sys/camera_awb_cal\n",
		       __func__);
		return -ENOMEM;
	}

	ret = sysfs_create_bin_file(cam_awb_cal, &factory_back);
	if (ret) {
		pr_err("%s: failed to create back camera file\n", __func__);
		kobject_del(cam_awb_cal);
		return ret;
	}

	ret = sysfs_create_bin_file(cam_awb_cal, &factory_front);
	if (ret) {
		pr_err("%s: failed to create front camera file\n", __func__);
		kobject_del(cam_awb_cal);
		return ret;
	}

	return 0;
}

late_initcall(cam_get_awb_cal);
