/*
 * copyright (C) 2013 HUAWEI, Inc.
 * Author: tuhaibo <tuhaibo@huawei.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "hall_sensor.h"

static struct hall_dev *pdev_hall[HALL_MAX_SUPPORT_NUM] = {NULL, NULL};
static int hall_count = 0;
struct workqueue_struct *hall_wq;

static const struct of_device_id hall_match_table[] = {
	{.compatible = "hall,sensor",},
	{},
};
MODULE_DEVICE_TABLE(of, hall_match_table);

int hall_first_report(bool enable)
{
	int ret = 0;
	int ret_val = 0;
	int index;
	int hall_dev_count = 0;
	int enable_value = 0;
	packet_data event_value = 0;
	struct hall_dev *hall_dev = NULL;

	hwlog_info("[hall][%s] hall enable state:%d\n", __func__, enable);
	if (!enable)
		return ret_val;

	hall_dev_count = hall_count < HALL_MAX_SUPPORT_NUM ? hall_count : HALL_MAX_SUPPORT_NUM;
	for (index=0; index<hall_dev_count; index++) {
		hall_dev = pdev_hall[index];
		if(hall_dev->ops && hall_dev->ops->packet_event_data)
			event_value = hall_dev->ops->packet_event_data(hall_dev);

		if(event_value < 0) {
			hwlog_err("[hall][%s] hall packet event failed.\n", __func__);
			ret_val = -1;
			return ret_val;
		}

		enable_value = hall_dev->ops->packet_event_data(hall_dev);

		hwlog_info("[hall][%s] enable_value:%u, event_value:%d.\n", __func__, enable_value, event_value);

		if(enable_value) {
			if(hall_dev->ops && hall_dev->ops->hall_event_report)
				ret = hall_dev->ops->hall_event_report(event_value);

			if(!ret) {
				hwlog_err("[hall][%s] hall report value failed.\n", __func__);
				return -EINVAL;
			}

			hwlog_info("[hall][%s] hall report event value:%d. \n", __func__, event_value);
		}
	}

	return ret_val;
}

static void hall_work(struct work_struct *work)
{
	int ret = 0;

	packet_data event_value = 0;
	struct hall_dev *phall_dev = NULL;

	phall_dev = container_of(work, struct hall_dev, hall_work);

	if (phall_dev->ops && phall_dev->ops->packet_event_data) {
		event_value = phall_dev->ops->packet_event_data(phall_dev);
	} else {
		hwlog_err("[hall][%s] packet event data failed!\n", __func__);
		return ;
	}

	if(phall_dev->ops && phall_dev->ops->hall_event_report)
		ret = phall_dev->ops->hall_event_report(event_value);

	if(!ret) {
		hwlog_err("[hall][%s] hall report value failed.\n", __func__);
		return ;
	}

	hwlog_info("[hall][%s] hall report value :%d.\n", __func__, event_value);
	return ;
}

static irqreturn_t hall_event_handler(int irq, void *pdev)
{
	struct hall_dev *phall_dev = NULL;

	if(NULL == pdev) {
		hwlog_err("[hall][%s] invalid pointer!\n", __func__);
		return IRQ_NONE;
	}

	phall_dev = (struct hall_dev *)pdev;

	if (phall_dev->ops && phall_dev->ops->hall_device_irq_top_handler && hall_work_status == phall_dev->hall_status) {
		phall_dev->ops->hall_device_irq_top_handler(irq, phall_dev);
	} else {
		return IRQ_HANDLED;
	}

	wake_lock_timeout(&phall_dev->hall_lock, HZ);

	queue_work(hall_wq, &phall_dev->hall_work);
	return IRQ_HANDLED;
}

static int hall_request_irq(struct hall_dev *phall_dev)
{
	int i = 0;
	int irq = 0;
	int ret = 0;
	int wake_flag;
	int trigger_value = 0;

	if (NULL == phall_dev) {
		hwlog_err("[hall][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	wake_flag = phall_dev->wake_up;
	for ( i = 0; i < HALL_IRQ_NUM; i++) {
		irq = phall_dev->irq_info[i].irq;
		trigger_value = phall_dev->irq_info[i].trigger_value;
		if (irq) {
			ret = request_irq(irq, hall_event_handler,
				trigger_value | wake_flag, phall_dev->irq_info[i].name, phall_dev);
				hwlog_info("[hall][%s]irq:%d, trigger_value:%d, wake_flag:%d.\n", \
				__func__, irq, trigger_value, wake_flag);
		}
	}

/*
  * report the current magnetic field approach status to the initialized magnetic field status approach identification.
  */
	if (phall_dev->ops && phall_dev->ops->hall_device_irq_top_handler && hall_work_status == phall_dev->hall_status) {
		phall_dev->ops->hall_device_irq_top_handler(irq, phall_dev);
		queue_work(hall_wq, &phall_dev->hall_work);
	}

	return ret;
}

static ssize_t store_enable_hall_sensor(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	int enable_value = 0;
	packet_data event_value = 0;
	struct platform_device *pdev = NULL;
	struct hall_dev *phall_dev = NULL;

	if(NULL == dev || NULL == attr || NULL == buf) {
		hwlog_err("[hall][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	pdev = container_of(dev, struct platform_device, dev);
	phall_dev = (struct hall_dev *)platform_get_drvdata(pdev);

	if(phall_dev->ops && phall_dev->ops->packet_event_data)
		event_value = phall_dev->ops->packet_event_data(phall_dev);
	if(event_value < 0) {
		hwlog_err("[hall][%s] hall packet event failed.\n", __func__);
		return 1;
	}

	enable_value = simple_strtoul(buf, NULL, 10);
	hwlog_info("[hall][%s] enable_value:%u, event_value:%d.\n", __func__, enable_value,
		event_value);

	if(enable_value) {
		if(phall_dev->ops && phall_dev->ops->hall_event_report)
			ret = phall_dev->ops->hall_event_report(event_value);
		if(!ret) {
			hwlog_err("[hall][%s] hall report value failed.\n", __func__);
			return -EINVAL;
		}
		hwlog_info("[hall][%s] hall report event value:%d. \n", __func__, event_value);
	}

	return 1;
}

static ssize_t show_get_hall_sensor_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = NULL;
	struct hall_dev *phall_dev = NULL;

	if(NULL == dev || NULL == attr || NULL == buf) {
		hwlog_err("[hall][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	pdev = container_of(dev, struct platform_device, dev);
	phall_dev = (struct hall_dev *)platform_get_drvdata(pdev);

	return snprintf(buf, PAGE_SIZE, "hall information:\nname:%s, id:%04d, type:%04d\t\
		wakeup flag: %05d.\nx_coordinate:%04d, y_coordinate:%04d.\n", pdev->name,
		phall_dev->hall_id, phall_dev->hall_type, phall_dev->wake_up,
		phall_dev->x_coordinate, phall_dev->y_coordinate);
}

static DEVICE_ATTR(hall_sensor, 0664, show_get_hall_sensor_status, store_enable_hall_sensor);

static struct attribute *hall_sensor_attributes[] = {
	&dev_attr_hall_sensor.attr,
	NULL
};

static const struct attribute_group hall_sensor_attr_group = {
	.attrs = hall_sensor_attributes,
};

static int __init hall_probe(struct platform_device *pdev)
{
	int ret = 0;
	int err = 0;
	struct device *dev = NULL;
	struct device_node *hall_node = NULL;
	struct hall_dev *hall_dev = NULL;

	if (NULL == pdev) {
		hwlog_err("[hall][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	dev = &pdev->dev;
	hall_node = dev->of_node;
        hall_dev = kzalloc(sizeof(struct hall_dev), GFP_KERNEL);
	if(!hall_dev) {
		hwlog_err("[hall][%s]hall_dev kzalloc error!\n", __func__);
		err = -ENOMEM;
		goto hall_dev_kzalloc_err;
	}

	hall_wq = create_singlethread_workqueue("hall_wq");
	if(IS_ERR(hall_wq)) {
		hwlog_err("[hall][%s]work_wq kmalloc error!\n", __func__);
		err = -ENOMEM;
		goto hall_wq_err;
	}

	ret = of_property_read_u32(hall_node, HALL_TYPE, &(hall_dev->hall_type));
	if(ret)	{
		hwlog_err("[hall][%s]hall type flag is  not specified\n", __func__);
		err = -EINVAL;
	 	goto hall_type_parse_err;
	}

	hall_dev->ops = &hall_device_ops;
	INIT_WORK(&(hall_dev->hall_work), hall_work);
	if (hall_dev->ops && hall_dev->ops->hall_device_init) {
		hall_dev->ops->hall_device_init(pdev, hall_dev);
		hwlog_info("[hall][%s]hall_device_init enter!\n", __func__);
	} else {
		hwlog_err("[hall][%s]can't find hall_device_init function!", __func__);
		err = -EINVAL;
		goto hall_device_init_err;
	}

	hall_dev->hall_dev = pdev;

	ret = of_property_read_u32(hall_node, HALL_TYPE, &hall_dev->hall_type);
	if(ret)	{
		hwlog_err("[hall][%s]hall type flag is  not specified\n", __func__);
		err = -EINVAL;
	 	goto dts_parse_err;
	}

	ret = of_property_read_u32(hall_node, HALL_WAKE_UP, &hall_dev->wake_up);
	if(ret) {
		hwlog_err("[ak8789][%s]hall wake up flag is  not specified\n", __func__);
		err = -EINVAL;
		goto dts_parse_err;
	}

	ret = of_property_read_u32(hall_node, HALL_ID, &hall_dev->hall_id);
	if (ret) {
		hwlog_err("[ak8789][%s] hall id is  not specified\n", __func__);
		err = -EINVAL;
		goto dts_parse_err;
	}

	ret = of_property_read_u32(hall_node, X_COR, &hall_dev->x_coordinate);
	if (ret) {
		hwlog_err("[ak8789][%s] hall x coordinate is not specified\n", __func__);
		err = -EINVAL;
		goto dts_parse_err;
	}

	ret = of_property_read_u32(hall_node, Y_COR, &hall_dev->y_coordinate);
	if (ret) {
		hwlog_err("[ak8789][%s] hall y coordinate is not specified\n", __func__);
		err = -EINVAL;
		goto dts_parse_err;
	}

	ret = snprintf(hall_dev->name, HALL_DEV_NAME_LEN, "hall%d", hall_dev->hall_id);
	if (ret<0) {
		hwlog_err("set hall dev name err\n");
		err = -EINVAL;
		goto dts_parse_err;
	}

	wake_lock_init(&hall_dev->hall_lock, WAKE_LOCK_SUSPEND, hall_dev->name);


	platform_set_drvdata(pdev, hall_dev);
	hall_dev->hall_status = hall_work_status;
	ret = hall_request_irq(hall_dev);
	if (ret) {
		hwlog_info("[hall][%s] hall_type:%d, hall_id:%d, wake_up:%d,\
			", __func__, hall_dev->hall_type, hall_dev->hall_id,
			 hall_dev->wake_up);
		goto hall_request_irq_err;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &hall_sensor_attr_group);
	if (err) {
		hwlog_err("[hall][%s] hall sysfs_create_group failed!", __func__);
		goto sysfs_create_group_err;
	}

	if (hall_count < HALL_MAX_SUPPORT_NUM) {
		pdev_hall[hall_count] = hall_dev;
		hall_count++;
		hwlog_info("[hall][%s] hall_count:%d\n", __func__, hall_count);
	} else {
		hwlog_err("[hall][%s] hall count err !\n", __func__);
		err = -ENXIO;
		goto sysfs_create_group_err;
	}

	return ret;

sysfs_create_group_err:
hall_request_irq_err:
dts_parse_err:
	wake_lock_destroy(&hall_dev->hall_lock);
	if( hall_dev->ops && hall_dev->ops->hall_release)
		hall_dev->ops->hall_release(hall_dev);
       destroy_workqueue(hall_wq);
hall_device_init_err:
hall_type_parse_err:
hall_wq_err:
	kfree(hall_dev);
	platform_set_drvdata(pdev, NULL);
hall_dev_kzalloc_err:
	return err;
}

static int __exit hall_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct hall_dev *hall_data = NULL;

	hall_data = (struct hall_dev *)platform_get_drvdata(pdev);
	hall_data->hall_status = hall_removed_status;

	wake_lock_destroy(&hall_data->hall_lock);
	destroy_workqueue(hall_wq);
        if( hall_data->ops && hall_data->ops->hall_release) {
                ret = hall_data->ops->hall_release(hall_data);
        }

	kfree(hall_data);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

struct platform_driver hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
			.name = HALL_SENSOR_NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(hall_match_table),
	},
};

static int __init hall_init(void)
{
	hwlog_info("[hall]hall init!\n");
	return platform_driver_register(&hall_driver);
}

static void __exit hall_exit(void)
{
	hwlog_info("[hall]hall exit!\n");
	platform_driver_unregister(&hall_driver);
}

module_init(hall_init);
module_exit(hall_exit);

MODULE_AUTHOR("Huawei PDU_DRV Group");
MODULE_DESCRIPTION("hall platform driver");
MODULE_LICENSE("GPL");
