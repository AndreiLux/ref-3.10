/*
 * SIC LABORATORY, LG ELECTRONICS INC., SEOUL, KOREA
 * Copyright(c) 2014 by LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/async.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include "hw_semaphore_drv.h"
#include "hw_semaphore.h"

static LIST_HEAD(hw_semaphore_head);
static unsigned int hw_semaphore_cnt;

struct hw_semaphore {
	struct list_head list;
	struct list_head elem_head;
	struct device *dev;
	const char *hw_name;
	unsigned int hw_id;
	unsigned int elemcnt;
	struct hw_semaphore_ops *ops;
};

struct hw_sema_element {
	struct device *owner;
	struct list_head list;
	struct list_head uelem_head;
	struct hw_semaphore *hw_sema;
	struct completion sema_comp;
	spinlock_t uelemlock;
	unsigned int id;
	const char *name;
	int type;
};

static struct hw_semaphore *find_hw_semaphore_dev(struct device *dev)
{
	struct hw_semaphore *hw = NULL;
	if (!list_empty(&hw_semaphore_head)) {
		list_for_each_entry(hw, &hw_semaphore_head, list) {
			if (hw->dev == dev)
				return hw;
		}
	}
	return NULL;
}


static struct hw_semaphore *find_hw_semaphore_id(unsigned int id)
{
	struct hw_semaphore *hw = NULL;
	if (!list_empty(&hw_semaphore_head)) {
		list_for_each_entry(hw, &hw_semaphore_head, list) {
			if (hw->hw_id == id)
				return hw;
		}
	}
	return NULL;
}

static struct hw_semaphore *find_hw_semaphore_name(char *name)
{
	struct hw_semaphore *hw = NULL;
	if (!list_empty(&hw_semaphore_head)) {
		list_for_each_entry(hw, &hw_semaphore_head, list) {
			if (!strncmp(hw->hw_name,
					name, strlen(hw->hw_name)))
				return hw;
		}
	}
	return NULL;
}

static struct hw_sema_element *find_sema_element_id(struct hw_semaphore *hw,
						    unsigned int id)
{
	struct hw_sema_element *elem = NULL;
	if (!list_empty(&hw->elem_head)) {
		list_for_each_entry(elem, &hw->elem_head, list) {
			if (elem->id == id)
				return elem;
		}
	}
	return NULL;
}

static struct hw_sema_element *find_sema_element_name(struct hw_semaphore *hw,
						      const char *name)
{
	struct hw_sema_element *elem = NULL;
	if (!list_empty(&hw->elem_head)) {
		list_for_each_entry(elem, &hw->elem_head, list) {
			if (!strncmp(elem->name, name,
					strlen(elem->name)))
				return elem;
		}
	}
	return NULL;
}

static int check_sema_valid(struct hw_sema_uelement *uelem)
{
	struct hw_semaphore *hw = NULL;

	if (!uelem || !uelem->entry) {
		pr_err("%s: uelem or elem is null pointer\n", __func__);
		return -EINVAL;
	}

	if (list_empty(&uelem->ulist)) {
		pr_err("%s: uelem ulist is empty\n", __func__);
		return -ENODEV;
	}

	if (!uelem->entry->hw_sema) {
		pr_err("%s: elem hw is null\n", __func__);
		return -ENODEV;
	}

	hw = find_hw_semaphore_dev(uelem->entry->hw_sema->dev);
	if (!hw) {
		pr_err("%s: can't find hw in all of list\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int allocation_sema_list(struct hw_semaphore *hw,
				const char **hw_sema_list)
{
	struct hw_sema_element *elem_base = NULL;
	struct hw_sema_element *elem = NULL;
	int i;

	if (!hw)
		return -ENOMEM;

	if (!list_empty(&hw->elem_head))
		return -EEXIST;

	elem_base = devm_kzalloc(hw->dev,
				sizeof(*elem_base) * hw->elemcnt,
				GFP_KERNEL);
	if (!elem_base)
		return -ENOMEM;

	for (i = 0; i < hw->elemcnt; i++) {
		elem = (struct hw_sema_element *)(elem_base+i);
		elem->owner = NULL;
		elem->hw_sema = hw;
		elem->id = i;
		elem->type = HW_SEMA_TRY_MODE;
		spin_lock_init(&elem->uelemlock);
		init_completion(&elem->sema_comp);
		INIT_LIST_HEAD(&elem->uelem_head);
		INIT_LIST_HEAD(&elem->list);
		list_add_tail(&elem->list, &hw->elem_head);
		if (hw_sema_list[i] != NULL)
			elem->name = hw_sema_list[i];
		else
			elem->name = "NONAME";

		pr_debug("%s(%d) elem add to %s\n", elem->name, elem->id,
			 hw->hw_name);
	}

	return 0;
}

static int release_all_completion(struct hw_sema_element *elem)
{
	unsigned int waitcnt = 0;
	unsigned long flags;
	struct hw_sema_uelement *uelem = NULL;
	if (elem->type == HW_SEMA_WAITING_MODE) {
		spin_lock_irqsave(&elem->uelemlock, flags);
		if (!list_empty(&elem->uelem_head)) {
			list_for_each_entry(uelem, &elem->uelem_head, ulist) {
				waitcnt += uelem->waiting;
			}
		}
		spin_unlock_irqrestore(&elem->uelemlock, flags);
	}

	while (waitcnt) {
		complete(&elem->sema_comp);
		waitcnt--;
	};

	return 0;
}

static int clean_uelem_head_list(struct hw_sema_element *elem)
{
	unsigned long flags;
	unsigned int waitcnt = 0;
	struct hw_sema_uelement *uelem = NULL;
	struct hw_sema_uelement *tmp = NULL;
	spin_lock_irqsave(&elem->uelemlock, flags);
	if (!list_empty(&elem->uelem_head)) {
		list_for_each_entry_safe(uelem, tmp, &elem->uelem_head, ulist) {
			if (uelem->entry == elem) {
				list_del_init(&uelem->ulist);
				uelem->entry = NULL;
				waitcnt += uelem->waiting;
			}
		}
	}
	spin_unlock_irqrestore(&elem->uelemlock, flags);

	pr_debug("%s(%d) elem clean ulist waitcnt %d\n",
		 elem->name, elem->id, waitcnt);

	while (waitcnt) {
		complete(&elem->sema_comp);
		waitcnt--;
		msleep(1);
	};

	return 0;
}

static int deallocation_sema_list(struct hw_semaphore *hw)
{
	struct hw_sema_element *elem_base = NULL;
	struct hw_sema_element *elem = NULL;
	int i;

	if (!hw)
		return -ENOMEM;

	if (list_empty(&hw->elem_head))
		return 0;

	for (i = 0; i < hw->elemcnt; i++) {
		elem = find_sema_element_id(hw, i);
		if (i == 0)
			elem_base = elem;
		elem->hw_sema = NULL;
		list_del_init(&elem->list);
		clean_uelem_head_list(elem);
		pr_debug("%s(%d) elem remove from to %s\n", elem->name,
			 elem->id, hw->hw_name);
	}

	if (elem_base) {
		memset(elem_base, 0x0,
		       sizeof(struct hw_sema_element) * hw->elemcnt);
		devm_kfree(hw->dev, elem_base);
	}

	return 0;
}

static int hw_sema_hw_read(struct hw_sema_element *elem)
{
	int ret = 1;
	if (elem->hw_sema->ops->hw_sema_read)
		ret = elem->hw_sema->ops->hw_sema_read(elem->id);
	return ret; /* 0 is lock status , 1 is unlock status */
}

static int hw_sema_hw_lock(struct hw_sema_element *elem)
{
	int ret = 0;
	if (elem->hw_sema->ops->hw_sema_lock)
		ret = elem->hw_sema->ops->hw_sema_lock(elem->id);
	return ret;
}

static int hw_sema_hw_unlock(struct hw_sema_element *elem)
{
	int ret = 0;
	if (elem->hw_sema->ops->hw_sema_unlock)
		ret = elem->hw_sema->ops->hw_sema_unlock(elem->id);

	return ret;
}

int hw_sema_put_sema(struct device *dev, struct hw_sema_uelement *uelem)
{
	unsigned long flags;
	if (check_sema_valid(uelem)) {
		pr_err("%s: uelem entry is WRONG or FREE\n", __func__);
	} else {
		if (uelem->entry->owner == dev)
			uelem->entry->owner = NULL;
		spin_lock_irqsave(&uelem->entry->uelemlock, flags);
		list_del_init(&uelem->ulist);
		spin_unlock_irqrestore(&uelem->entry->uelemlock, flags);
		uelem->entry = NULL;
	}
	kfree(uelem);
	return 0;
}
EXPORT_SYMBOL(hw_sema_put_sema);

struct hw_sema_uelement *hw_sema_get_sema_name(struct device *dev,
					       const char *name)
{
	int i;
	struct hw_sema_uelement *uelem = NULL;
	struct hw_sema_element *elem = NULL;
	struct hw_semaphore *hw = NULL;
	unsigned long flags;

	for (i = 0; i < hw_semaphore_cnt; i++) {
		hw = find_hw_semaphore_id(i);
		if (hw) {
			elem = find_sema_element_name(hw, name);
			break;
		}
	}

	if (!elem) {
		pr_err("%s: name %s not found.\n", __func__, name);
		return NULL;
	}

	uelem = kzalloc(sizeof(*uelem), GFP_KERNEL);
	if (!uelem)
		return NULL;

	uelem->entry = elem;
	uelem->waiting = 0;
	INIT_LIST_HEAD(&uelem->ulist);
	spin_lock_irqsave(&uelem->entry->uelemlock, flags);
	list_add_tail(&uelem->ulist, &elem->uelem_head);
	spin_unlock_irqrestore(&uelem->entry->uelemlock, flags);

	if (elem->owner == NULL)
		elem->owner = dev;

	pr_debug("%s: %s(%d) found, list 0x%p to elem\n",
		 __func__, elem->name, elem->id, &uelem->ulist);
	return uelem;
}
EXPORT_SYMBOL(hw_sema_get_sema_name);

int hw_sema_set_sema_type(struct device *dev, struct hw_sema_uelement *uelem,
			  int type)
{
	if (!uelem)
		return -EINVAL;

	if (check_sema_valid(uelem))
		return -ENODEV;

	if (uelem->entry->owner != dev) {
		pr_err("%s: you are not owner of elem\n", __func__);
		return -EPERM;
	}

	if (type >= HW_SEMA_TRY_MODE || type <= HW_SEMA_WAITING_MODE)
		uelem->entry->type = type;

	pr_debug("%s: %s %d\n", __func__, uelem->entry->name, type);

	return 0;
}
EXPORT_SYMBOL(hw_sema_set_sema_type);

int hw_sema_get_sema_type(struct hw_sema_uelement *uelem)
{
	if (check_sema_valid(uelem))
		return -ENODEV;

	return uelem->entry->type;
}
EXPORT_SYMBOL(hw_sema_get_sema_type);

int hw_sema_lock_status(struct hw_sema_uelement *uelem)
{
	if (check_sema_valid(uelem))
		return -ENODEV;

	return hw_sema_hw_read(uelem->entry);
}
EXPORT_SYMBOL(hw_sema_lock_status);

int hw_sema_up(struct hw_sema_uelement *uelem)
{
	int ret;
	if (check_sema_valid(uelem))
		return -ENODEV;

	pr_debug("%s: %s\n", __func__, uelem->entry->name);
	ret = hw_sema_hw_unlock(uelem->entry);
	return ret;
}
EXPORT_SYMBOL(hw_sema_up);

int hw_sema_down(struct hw_sema_uelement *uelem)
{
	int ret;

	do {
		if (check_sema_valid(uelem))
			return -ENODEV;

		ret = hw_sema_hw_lock(uelem->entry);
		/* 0 = acquired, 1 = used anyone, */
		/* 0 = lock,     1 = lock failed */

		pr_debug("%s: %s type: %d result: %d (0-lock)\n", __func__,
			 uelem->entry->name, uelem->entry->type, ret);
		if (uelem->entry->type == HW_SEMA_TRY_MODE)
			break;

		if (ret == 1) {
			uelem->waiting++;
			wait_for_completion(&uelem->entry->sema_comp);
			uelem->waiting--;
		}
	} while (ret);

	return ret; /* 0 is lock success */
}
EXPORT_SYMBOL(hw_sema_down);

int hw_sema_down_interruptible(struct hw_sema_uelement *uelem)
{
	int ret;
	int comp_ret = 0;

	do {
		if (check_sema_valid(uelem))
			return -ENODEV;

		ret = hw_sema_hw_lock(uelem->entry);
		/* 0 = acquired, 1 = used anyone, */
		/* 0 = lock,     1 = lock failed */

		pr_debug("%s: %s type: %d result:%d (0-lock)\n", __func__,
			 uelem->entry->name, uelem->entry->type, ret);
		if (uelem->entry->type == HW_SEMA_TRY_MODE)
			return ret;

		if (!ret) {
			return ret;
		} else {
			uelem->waiting++;
			comp_ret = wait_for_completion_interruptible
				   (&uelem->entry->sema_comp);
			uelem->waiting--;
		}
	} while (!comp_ret); /* 0 if completed */

	return comp_ret;
}
EXPORT_SYMBOL(hw_sema_down_interruptible);

int hw_sema_down_killable(struct hw_sema_uelement *uelem)
{
	int ret;
	int comp_ret = 0;

	do {
		if (check_sema_valid(uelem))
			return -ENODEV;

		ret = hw_sema_hw_lock(uelem->entry);
		/* 0 = acquired, 1 = used anyone, */
		/* 0 = lock,     1 = lock failed */
		pr_debug("%s: %s type: %d result:%d (0-lock)\n", __func__,
			 uelem->entry->name, uelem->entry->type, ret);

		if (uelem->entry->type == HW_SEMA_TRY_MODE)
			return ret;

		if (!ret) {
			return ret;
		} else {
			uelem->waiting++;
			comp_ret = wait_for_completion_killable
				   (&uelem->entry->sema_comp);
			uelem->waiting--;
		}
	} while (!comp_ret); /* 0 if completed */

	return comp_ret;
}
EXPORT_SYMBOL(hw_sema_down_killable);

unsigned long hw_sema_down_timeout(struct hw_sema_uelement *uelem,
				   unsigned long timeout)
{
	int ret;
	int remaintime = timeout;

	do {
		if (check_sema_valid(uelem))
			return -ENODEV;

		ret = hw_sema_hw_lock(uelem->entry);
		/* 0 = acquired, 1 = used anyone, */
		/* 0 = lock,     1 = lock failed */

		pr_debug("%s: %s type: %d result:%d (0-lock)\n", __func__,
			 uelem->entry->name, uelem->entry->type, ret);

		if (uelem->entry->type == HW_SEMA_TRY_MODE) {
			if (!ret)
				break;
			else
				return 0;
		}

		if (!ret) {
			break;
		} else {
			uelem->waiting++;
			remaintime = wait_for_completion_timeout
				     (&uelem->entry->sema_comp, remaintime);
			uelem->waiting--;
		}
	} while (remaintime);

	return remaintime;
}
EXPORT_SYMBOL(hw_sema_down_timeout);

int hw_sema_register(struct device *dev, struct hw_sema_init_data *data)
{
	int ret = 0;
	struct hw_semaphore *hw;

	if (!dev)
		return -ENODEV;

	if (!data->hw_name || !data->hw_sema_list || data->hw_sema_cnt <= 0)
		return -EINVAL;

	hw = devm_kzalloc(dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	hw->dev = dev;
	hw->hw_name = data->hw_name;
	hw->elemcnt = data->hw_sema_cnt;
	hw->ops = data->ops;

	INIT_LIST_HEAD(&hw->elem_head);
	INIT_LIST_HEAD(&hw->list);

	pr_debug("%s(cnt:%d) created. maybe id is %d\n",
		 hw->hw_name, hw->elemcnt, hw_semaphore_cnt);

	ret = allocation_sema_list(hw, data->hw_sema_list);
	if (ret) {
		pr_err("%s(cnt:%d) failed to init semalist %d\n",
		       hw->hw_name, hw->elemcnt, ret);
		devm_kfree(dev, hw);
		return ret;
	}

	list_add_tail(&hw->list, &hw_semaphore_head);
	hw->hw_id = hw_semaphore_cnt++;

	pr_info("%s(cnt:%d,id:%d) hw_semaphore driver register success\n",
		hw->hw_name, hw->elemcnt, hw->hw_id);

	return 0;
}
EXPORT_SYMBOL(hw_sema_register);

int hw_sema_deregister(struct device *dev)
{
	struct hw_semaphore *hw = find_hw_semaphore_dev(dev);
	pr_info("%s(cnt:%d,id:%d) hw_semaphore driver deregister\n",
		hw->hw_name, hw->elemcnt, hw->hw_id);
	list_del_init(&hw->list);
	deallocation_sema_list(hw);
	devm_kfree(dev, hw);
	return 0;
}
EXPORT_SYMBOL(hw_sema_deregister);

static void hw_semaphore_aync(void *data, async_cookie_t cookie)
{
	struct hw_sema_element *elem = (struct hw_sema_element *)data;

	if (!elem)
		return;

	pr_debug("%s : id %d, name %s released\n", elem->hw_sema->hw_name,
		 elem->id, elem->name);

	release_all_completion(elem);
}

int hw_semaphore_interupt_callback(struct device *dev, unsigned int id)
{
	struct hw_sema_element *elem;
	struct hw_semaphore *hw;

	hw = find_hw_semaphore_dev(dev);
	if (!hw)
		return -ENODEV;

	elem = find_sema_element_id(hw, id);
	if (!elem)
		return -EINVAL;

	async_schedule(hw_semaphore_aync, elem);

	return 0;
}
EXPORT_SYMBOL(hw_semaphore_interupt_callback);
