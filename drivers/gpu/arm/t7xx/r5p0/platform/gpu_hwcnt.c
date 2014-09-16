/*
 * .... hwcnt
 *
 * Copyright (C) 2013 Samsung Electronics
 *      Sangkyu Kim <skwith.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <mali_kbase.h>
#include "mali_kbase_platform.h"
#include "gpu_hwcnt.h"

mali_error exynos_gpu_hwcnt_update(struct kbase_device *kbdev)
{
	mali_error err = MALI_ERROR_FUNCTION_FAILED;

	KBASE_DEBUG_ASSERT(kbdev);

	if (!kbdev->hwcnt.enable_for_utilization || (!kbdev->hwcnt.prev_mm)) {
		err = MALI_ERROR_NONE;
		goto out;
	}

	if (!hwcnt_check_conditions(kbdev)) {
		err = MALI_ERROR_NONE;
		goto out;
	}

	if (!kbdev->hwcnt.kctx)
		goto out;

	err = kbase_instr_hwcnt_dump(kbdev->hwcnt.kctx);

	if (err != MALI_ERROR_NONE) {
		goto out;
	}

	err = kbase_instr_hwcnt_clear(kbdev->hwcnt.kctx);

	if (err != MALI_ERROR_NONE) {
		goto out;
	}

	err = hwcnt_get_utilization_resouce(kbdev);

	if (err != MALI_ERROR_NONE) {
		goto out;
	}

out:
	return err;
}

bool hwcnt_check_conditions(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	KBASE_DEBUG_ASSERT(kbdev);

	if ((!kbdev->hwcnt.condition_to_dump) && (platform->cur_clock >= platform->gpu_max_clock_limit)) {
		kbase_pm_policy_change(kbdev, 1);

		if (platform->dvs_is_enabled) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "hwcnt error!, tried to enable hwcnt while running dvs\n");
			kbase_pm_policy_change(kbdev, 2);
			goto out;
		}

		kbdev->hwcnt.condition_to_dump = TRUE;

		GPU_LOG(DVFS_INFO, LSI_HWCNT_ON_DVFS, 0u, kbdev->hwcnt.kspace_addr, "hwcnt on dvfs mode\n");

		if (!kbdev->hwcnt.kspace_addr)
			kbase_instr_hwcnt_start(kbdev);

		kbdev->hwcnt.cnt_for_stop = 0;
		kbdev->hwcnt.cnt_for_bt_start = 0;
		kbdev->hwcnt.cnt_for_bt_stop = 0;
	} else if ((kbdev->hwcnt.condition_to_dump) && (platform->cur_clock < platform->gpu_max_clock_limit)) {
		kbdev->hwcnt.cnt_for_stop++;
		if (kbdev->hwcnt.cnt_for_stop > 5) {

			GPU_LOG(DVFS_INFO, LSI_HWCNT_OFF_DVFS, 0u, kbdev->hwcnt.kspace_addr, "hwcnt off dvfs mode\n");

			if (kbdev->hwcnt.kspace_addr)
				kbase_instr_hwcnt_stop(kbdev);

			kbdev->hwcnt.condition_to_dump = FALSE;
			kbase_pm_policy_change(kbdev, 2);
			kbdev->hwcnt.cnt_for_stop = 0;
			platform->hwcnt_bt_clk = FALSE;
		}
	}

out:
	return kbdev->hwcnt.condition_to_dump;
}

void hwcnt_utilization_equation(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int total_util;
	unsigned int debug_util;

	KBASE_DEBUG_ASSERT(kbdev);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%d, %d, %d\n",
			kbdev->hwcnt.resources.arith_words, kbdev->hwcnt.resources.ls_issues,
			kbdev->hwcnt.resources.tex_issues);

	total_util = kbdev->hwcnt.resources.arith_words +
		kbdev->hwcnt.resources.ls_issues + kbdev->hwcnt.resources.tex_issues;
	debug_util = (kbdev->hwcnt.resources.arith_words << 24)
		| (kbdev->hwcnt.resources.ls_issues << 16) | (kbdev->hwcnt.resources.tex_issues << 8);

	if ((kbdev->hwcnt.resources.arith_words * 10 > kbdev->hwcnt.resources.ls_issues * 14) &&
			(kbdev->hwcnt.resources.ls_issues < 40) && (total_util > 80)) {
		kbdev->hwcnt.cnt_for_bt_start++;
		kbdev->hwcnt.cnt_for_bt_stop = 0;
		if (kbdev->hwcnt.cnt_for_bt_start > 5) {
			platform->hwcnt_bt_clk = TRUE;
			kbdev->hwcnt.cnt_for_bt_start = 0;
		}
	} else {
		kbdev->hwcnt.cnt_for_bt_stop++;
		kbdev->hwcnt.cnt_for_bt_start = 0;
		if (kbdev->hwcnt.cnt_for_bt_stop > 5) {
			platform->hwcnt_bt_clk = FALSE;
			kbdev->hwcnt.cnt_for_bt_stop = 0;
		}
	}

	if (platform->hwcnt_bt_clk == TRUE)
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_ON, 0u, debug_util, "hwcnt bt on\n");
	else
		GPU_LOG(DVFS_INFO, LSI_HWCNT_BT_OFF, 0u, debug_util, "hwcnt bt off\n");
}

mali_error hwcnt_get_utilization_resouce(struct kbase_device *kbdev)
{
	int num_cores;
	int mem_offset;
	unsigned int *addr32;
	u32 arith_words = 0, ls_issues = 0, tex_issues = 0, tripipe_active = 0, div_tripipe_active;
	mali_error err = MALI_ERROR_FUNCTION_FAILED;
	int i;

	KBASE_DEBUG_ASSERT(kbdev);

	if (!kbdev->hwcnt.kspace_addr) {
		goto out;
	}

	if (kbdev->gpu_props.num_core_groups != 1) {
		/* num of core groups must be 1 on T76X */
		goto out;
	}

	num_cores = kbdev->gpu_props.num_cores;
	addr32 = (unsigned int *)((unsigned int)kbdev->hwcnt.kspace_addr & 0xffffffff);

	if (num_cores <= 4)
		mem_offset = MALI_SIZE_OF_HWCBLK * 3;
	else
		mem_offset = MALI_SIZE_OF_HWCBLK * 4;

	for (i=0; i < num_cores; i++)
	{
		if ( i == 3 && (num_cores == 5 || num_cores == 6) )
			mem_offset += MALI_SIZE_OF_HWCBLK;
		tripipe_active += *(addr32 + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TRIPIPE_ACTIVE);
		arith_words += *(addr32 + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_ARITH_WORDS);
		ls_issues += *(addr32 + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_LS_ISSUES);
		tex_issues += *(addr32 + mem_offset + MALI_SIZE_OF_HWCBLK * i + OFFSET_TEX_ISSUES);
	}

	div_tripipe_active = tripipe_active / 100;

	if (div_tripipe_active) {
		kbdev->hwcnt.resources.arith_words = arith_words / div_tripipe_active;
		kbdev->hwcnt.resources.ls_issues = ls_issues / div_tripipe_active;
		kbdev->hwcnt.resources.tex_issues = tex_issues / div_tripipe_active;
	}

	hwcnt_utilization_equation(kbdev);

	kbdev->hwcnt.resources.arith_words = 0;
	kbdev->hwcnt.resources.ls_issues = 0;
	kbdev->hwcnt.resources.tex_issues = 0;

	err = MALI_ERROR_NONE;

out:
	return err;
}
