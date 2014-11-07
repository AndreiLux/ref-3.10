/*
 * Maxim MAX77819 WLED Driver Header
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX77819_WLED_H__
#define __MAX77819_WLED_H__

/* WLEDPWM input use:
 *   MAX77819_WLED_PWM_INPUT_DISABLE  ignoring WLEDPWM input
 *   MAX77819_WLED_PWM_INPUT_CABC     CABC dimming signal
 *   MAX77819_WLED_PWM_INPUT_EXTRA    extra control signal for higher resolution
 *                                    in brightness levels
 */
#define MAX77819_WLED_PWM_INPUT_DISABLE  0
#define MAX77819_WLED_PWM_INPUT_CABC     1
#define MAX77819_WLED_PWM_INPUT_EXTRA    2

struct max77819_wled_platform_data {
    u32  current_map_id;        // TBD ; currently, don't care
    u32  ramp_up_time_msec;
    u32  ramp_dn_time_msec;
    u32  pwm_input_use;
    u32  pwm_id;                /* will be used when "pmw_input_use == EXTRA" */
    u32  pwm_period_nsec;       /* will be used when "pmw_input_use == EXTRA" */
};

int bl_update_status (int val);
int bl_get_brightness(void);

#endif /* !__MAX77819_WLED_H__ */
