#ifndef INCLUDE_ODIN_TIMER_H
#define INCLUDE_ODIN_TIMER_H

#include <linux/clockchips.h>

void odin_timer_retention_suspend(void);
void odin_timer_retention_resume(void);
void __init odin_timer_init(void);

#endif
