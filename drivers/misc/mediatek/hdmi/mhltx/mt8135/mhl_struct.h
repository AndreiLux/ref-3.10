#ifdef CONFIG_MTK_INTERNAL_MHL_SUPPORT
#ifndef _MHL_STRUCT_H_
#define _MHL_STRUCT_H_
typedef struct {
	unsigned int u4Addr;
	unsigned int u4Data;
} RW_VALUE;
typedef struct {
	unsigned int u4Cmd;
	unsigned int u4Para;
	unsigned int u4Para1;
	unsigned int u4Para2;
} stMhlCmd_st;
#endif
#endif
