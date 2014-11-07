#ifndef __ODIN_BUS_MONITOR_H__
#define __ODIN_BUS_MONITOR_H__

extern u64 odin_current_mem_bandwidth(void);
extern u64 odin_current_cci_bandwidth(void);
extern u64 odin_current_gpu_bandwidth(void);
extern u64 odin_current_cpu_bandwidth(void);
extern u64 odin_normalized_mem_bandwidth(void);
extern u64 odin_normalized_cci_bandwidth(void);
extern u64 odin_normalized_gpu_bandwidth(void);
extern u64 odin_normalized_cpu_bandwidth(void);
extern void odin_bus_monitor_kick(void);

#endif
