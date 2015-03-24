#ifndef __ARCH_MT_PLATFORM_H_
#define __ARCH_MT_PLATFORM_H_

enum mt_platform_flags {
	/*flags 1..0x80 are reserved for platform-wide use */
	MT_WRAPPER_BUS	= 1,
	/*flags 0x100 are device-specific */
	MT_DRIVER_FIRST = 0x100,
};
typedef u32 addr_t;

#endif /* __ARCH_MT_PLATFORM_H_ */
