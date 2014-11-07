
/*
 * Copyright @ Open Broadcom Corporation
 */

#ifndef __ODIN_WIFI_H__
#define __ODIN_WIFI_H__


#define WLAN_STATIC_SCAN_BUF0				5
#define WLAN_STATIC_SCAN_BUF1				6

#define PREALLOC_WLAN_SEC_NUM				4
#define PREALLOC_WLAN_BUF_NUM				160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0				(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1				(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2				(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3				(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE					336
#define DHD_SKB_1PAGE_BUFSIZE				((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE				((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE				((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM					17

/*                                                                                     */
#if defined(CONFIG_SIC_WLAN_CHIP_CRASH_RECOVERY)
#define LGE_ODIN_WLAN_REASON_CHIP_HANG	99
#endif

#endif /* __ODIN_WIFI_H__ */
