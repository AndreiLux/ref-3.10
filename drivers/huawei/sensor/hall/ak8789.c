/*
 * Copyright (C) 2013 HUAWEI, Inc.
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

#include "ak8789.h"

static int gpio_index_to_name(int index, char *name)
{
	if(NULL == name) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	switch (index) {
		case 0:
			strncpy(name, NORTH_POLE_NAME, sizeof(NORTH_POLE_NAME));
			break;
		case 1:
			strncpy(name, SOUTH_POLE_NAME, sizeof(SOUTH_POLE_NAME));
			break;
		default:
			hwlog_err("[ak8789][%s] Invalid index!\n", __func__);
	}

	return 0;
}

static int setup_gpio (int gpio_id, const char *gpio_name) {
	int ret = 0;

	if(NULL == gpio_name) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	ret = gpio_request(gpio_id, gpio_name);
	if(ret) {
		hwlog_err("[hall][%s] requset gpio %d err %d\n", __func__, gpio_id, ret);
		return ret;
	}

	ret = gpio_direction_input(gpio_id);
	if(ret) {
		hwlog_err("[hall][%s] gpio %d direction input err %d\n", __func__, gpio_id, ret);
		return ret;
	}

	return ret;
}

static int gpio_irq_trigger_inversion(int irq)
{
        struct irq_desc *desc = NULL;
	int trigger_value = 0;
	int ret = 0;

	desc = irq_to_desc(irq);
	if (!desc) {
		hwlog_err("[%s]irq_desc null!\n", __func__);
		return IRQ_NONE;
	}

	trigger_value = desc->irq_data.state_use_accessors & IRQD_TRIGGER_MASK;
	if (trigger_value & IRQF_TRIGGER_LOW) {
		ret = irq_set_irq_type(irq, IRQF_TRIGGER_HIGH);
	} else {
		ret = irq_set_irq_type(irq, IRQF_TRIGGER_LOW);
	}

	return ret;
}


static int hall_ak8789_irq_top_handler(int irq, struct hall_dev *phall_dev)
{
	int ret = 0;

	if(NULL == phall_dev) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	if(irq)
		ret = gpio_irq_trigger_inversion(irq);

	return ret;
}

static packet_data hall_ak8789_packet_event_data(struct hall_dev *phall_dev)
{
	int i = 0;
	struct hall_ak8789_dev *hall_ak8789 = NULL;
	packet_data event_value = 0;

	if(NULL == phall_dev) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	hall_ak8789 = phall_dev->hall_device;

	for (i = 0; i < AK8789_GPIO_NUM; i++) {
		if (hall_ak8789->gpio_poles[i]) {
			event_value |= !gpio_get_value(hall_ak8789->gpio_poles[i])\
			<< (phall_dev->hall_id * 2 + i);
		}
	}

	return event_value;
}

static int hall_ak8789_release(struct hall_dev *phall_dev)
{
	int i = 0;
	int irq = 0;
	int ret = 0;
	int gpio_id = 0;
	struct hall_ak8789_dev *hall_ak8789 = NULL;

	if(NULL == phall_dev) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	hall_ak8789 = phall_dev->hall_device;
	for (i=0; i < HALL_IRQ_NUM; i++) {
		irq = phall_dev->irq_info[i].irq;
		if(irq) {
			free_irq(irq, phall_dev);
		}
	}


	for (i = 0; i< AK8789_GPIO_NUM; i++) {
		gpio_id = hall_ak8789->gpio_poles[i];
		if(gpio_id) {
			gpio_free(gpio_id);
		}
	}

	kfree(phall_dev->hall_device);
	phall_dev->hall_device = NULL;

	return ret;
}

static int hall_ak8789_device_init(struct platform_device *pdev,
		struct hall_dev *phall_dev)
{
	int err = 0;
	int i = 0;
	int gpio_id = 0;
	int gpio_value = 0;
	int hall_type_err = 0;
	char gpio_name[MAX_GPIO_NAME_LEN];
        struct device_node *hall_ak8789_node = NULL;
	struct device *dev = NULL;
	struct hall_ak8789_dev *hall_ak8789 = NULL;

	if(NULL == phall_dev || NULL == pdev) {
		hwlog_err("[ak8789][%s] invalid pointer!\n", __func__);
		return -EINVAL;
	}

	hall_ak8789 = kzalloc(sizeof(struct hall_ak8789_dev), GFP_KERNEL);
	if(!hall_ak8789) {
		hwlog_err("[hall][%s]hall_dev kmalloc error!\n", __func__);
		err = -ENOMEM;
		return err;
	}

	dev = &pdev->dev;
	hall_ak8789_node = dev->of_node;
	hall_ak8789->hall_ak8789_type = phall_dev->hall_type;
	for (i = 0; i < AK8789_GPIO_NUM; i++) {
		hall_ak8789->gpio_poles[i] = 0;
	}

      switch (hall_ak8789->hall_ak8789_type) {
		case single_north_pole:
			hall_ak8789->gpio_poles[0] = of_get_named_gpio(hall_ak8789_node
				, NORTH_POLE_NAME, 0);;
			break;
		case single_south_pole:
			hall_ak8789->gpio_poles[1] = of_get_named_gpio(hall_ak8789_node
				, SOUTH_POLE_NAME, 0);
			break;
		case double_poles:
			hall_ak8789->gpio_poles[0] = of_get_named_gpio(hall_ak8789_node
				, NORTH_POLE_NAME, 0);
			hall_ak8789->gpio_poles[1] = of_get_named_gpio(hall_ak8789_node
				, SOUTH_POLE_NAME, 0);
			break;
		default:
			hwlog_err("[hall][%s]Invalid hall type !\n", __func__);
			kfree(hall_ak8789);
			hall_type_err = -1;
			return hall_type_err;
	}

	for (i = 0; i < AK8789_GPIO_NUM; i++) {
		gpio_id = hall_ak8789->gpio_poles[i];
		if(gpio_id) {
			gpio_index_to_name(i, gpio_name);
			err = setup_gpio(gpio_id, gpio_name);
			if(err) {
                                hwlog_err("[hall][%s] gpio %d direction input err %d\n",
					__func__, gpio_id, err);
                     }

			gpio_value = gpio_get_value(gpio_id);
			phall_dev->irq_info[i].irq = gpio_to_irq(gpio_id);
			phall_dev->irq_info[i].trigger_value = (gpio_value == GPIO_LOW_VOLTAGE)
				? IRQF_TRIGGER_HIGH : IRQF_TRIGGER_LOW;
			strcpy(phall_dev->irq_info[i].name, gpio_name);
		} else {
			phall_dev->irq_info[i].irq = 0;
			phall_dev->irq_info[i].trigger_value = 0;
		}
	}

	phall_dev->hall_device = hall_ak8789;
	return err;
}

static int hall_ak8789_event_report(packet_data packet_data)
{
	return ap_hall_report(packet_data);
}

struct hall_ops hall_device_ops = {
	.packet_event_data = hall_ak8789_packet_event_data,
	.hall_event_report = hall_ak8789_event_report,
	.hall_device_irq_top_handler = hall_ak8789_irq_top_handler,
	.hall_release = hall_ak8789_release,
	.hall_device_init = hall_ak8789_device_init,
};
