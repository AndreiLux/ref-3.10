#ifndef __ODIN_TZ_H_
#define __ODIN_TZ_H_

#define ODIN_PWR_CPU_OFF			0x00000001
#define ODIN_PWR_CLUSTER_OFF			0x00000002
#define ODIN_PWR_DEEP_SLEEP			0x00000003


#define ODIN_POWER_CLEAN_L1			0x00000100
#define ODIN_POWER_CLEAN_L2			0x00000200
#define ODIN_POWER_CACHE_CLEAN_MASK		(ODIN_POWER_CLEAN_L1 \
						|ODIN_POWER_CLEAN_L2)

#define ODIN_POWER_SAVE_STATE			0x00000400
#define ODIN_POWER_MARK_CPU_OFF			0x00000800
#define ODIN_POWER_DISABLE_CCI			0x00001000
#define ODIN_POWER_WFI				0x00008000

#define ODIN_POWER_SEND_MSG			0x00010000

#define ODIN_POWER_RECOVER                      0x00020000

#define ODIN_POWER_LOCK                         0x00040000
#define ODIN_POWER_UNLOCK                       0x00080000

#define ODIN_POWER_CPU_OFF			(ODIN_POWER_CLEAN_L1 \
						| ODIN_POWER_MARK_CPU_OFF \
						| ODIN_POWER_WFI)
#define ODIN_POWER_CLUSTER_OFF			(ODIN_POWER_CLEAN_L2 \
						| ODIN_POWER_MARK_CPU_OFF \
						| ODIN_POWER_WFI)
#define ODIN_POWER_DEEP_SLEEP			(ODIN_POWER_CLEAN_L2 | \
						ODIN_POWER_MARK_CPU_OFF \
						| ODIN_POWER_WFI \
						| ODIN_POWER_SAVE_STATE)

#define FASTCALL_OWNER_SIP			(0x81000000)
#define ODIN_FC_NEON_ROTATE			(FASTCALL_OWNER_SIP | 0x200)
#define ODIN_FC_NEON_MAP			(FASTCALL_OWNER_SIP | 0x201)
#define ODIN_FC_NEON_UNMAP			(FASTCALL_OWNER_SIP | 0x202)

enum {
	ODIN_SES_CC = 0,
	ODIN_SES_PE,
};

enum {
	ODIN_NIU_INIT = 0,
	ODIN_IOMMU_INIT,
};

#ifdef CONFIG_ODIN_TEE

struct fc_neon_rot {
	u32 src_first_base;
	u32 src_first_offset;
	u32 src_second_base;
	u32 src_second_offset;
	u32 dst_first_base;
	u32 dst_first_offset;
	u32 dst_second_base;
	u32 dst_second_offset;
	u32 width;
	u32 height;
	u32 whole_height;
	u32 msg;
};
typedef struct fc_neon_rot fc_neon_rot_t;

u32 tz_sleep(u32 type, void *data);
u32 tz_secondary(u32 cpu_id, u32 phy_addr);
u32 tz_ops(u32 type, void *data);
u32 tz_write(u32 value, u32 addr);
u32 tz_read(u32 addr);
u32 tz_ses_pm(u32 module, u32 state);
u32 tz_rotator(struct fc_neon_rot *data, u32 degree);

#else

#define tz_write(val, addr)	__raw_writel(val, __va(addr))
#define tz_read(addr)		__raw_readl(__va(addr))

#endif

#endif
