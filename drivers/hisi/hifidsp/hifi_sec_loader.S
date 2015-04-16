#define HIFI_SET_CONFIG_CMD	(0xe0)
#define HIFI_DOWN_LOAD_CMD	(0xe1)
#define HIFI_LOAD_ALL		(0xff)
#define HIFI_LOAD_TCM		(0xfe)
#define HIFI_CONFIG_MISC	(0xfd)
#define HIFI_CONFIG_OCD		(0xfc)
#define HIFI_CONFIG_SEC		(0xfb)

	.globl hifi_load_all_function
	.globl hifi_load_tcm_function
	.globl hifi_set_misc_config
	.globl hifi_set_ocd_config
	.globl hifi_set_sec_config

hifi_load_all_function:
	.arch_extension sec
	stmfd sp!, {r0-r3,r12,lr}
	mov r0, #HIFI_DOWN_LOAD_CMD
	mov r3, #HIFI_LOAD_ALL
	dsb
	smc 0
	ldmfd sp!, {r0-r3,r12,lr}
	mov pc, lr

hifi_load_tcm_function:
	.arch_extension sec
	stmfd sp!, {r0-r3,r12,lr}
	mov r0, #HIFI_DOWN_LOAD_CMD
	mov r3, #HIFI_LOAD_TCM
	dsb
	smc 0
	ldmfd sp!, {r0-r3,r12,lr}
	mov pc, lr

hifi_set_misc_config:
	.arch_extension sec
	stmfd sp!, {r0-r3,r12,lr}
	mov r0, #HIFI_SET_CONFIG_CMD
	mov r3, #HIFI_CONFIG_MISC
	dsb
	smc 0
	ldmfd sp!, {r0-r3,r12,lr}
	mov pc, lr

hifi_set_ocd_config:
	.arch_extension sec
	stmfd sp!, {r0-r3,r12,lr}
	mov r0, #HIFI_SET_CONFIG_CMD
	mov r3, #HIFI_CONFIG_OCD
	dsb
	smc 0
	ldmfd sp!, {r0-r3,r12,lr}
	mov pc, lr

hifi_set_sec_config:
	.arch_extension sec
	stmfd sp!, {r0-r3,r12,lr}
	mov r0, #HIFI_SET_CONFIG_CMD
	mov r3, #HIFI_CONFIG_SEC
	dsb
	smc 0
	ldmfd sp!, {r0-r3,r12,lr}
	mov pc, lr