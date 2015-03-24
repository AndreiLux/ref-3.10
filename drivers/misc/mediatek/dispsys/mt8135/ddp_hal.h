#ifndef __DDP_HAL_H__
#define __DDP_HAL_H__

#include "linux/dpframework/DpDataType.h"

struct DISP_REGION {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

enum OVL_LAYER_SOURCE {
	OVL_LAYER_SOURCE_MEM = 0,
	OVL_LAYER_SOURCE_RESERVED = 1,
	OVL_LAYER_SOURCE_SCL = 2,
	OVL_LAYER_SOURCE_PQ = 3,
};

enum OVL_LAYER_SECURE_MODE {
	LAYER_NORMAL_BUFFER = 0,
	LAYER_SECURE_BUFFER = 1,
	LAYER_PROTECTED_BUFFER = 2
};

typedef struct _OVL_CONFIG_STRUCT {
	unsigned int layer;
	unsigned int layer_en;
	enum OVL_LAYER_SOURCE source;
	unsigned int fmt;
	unsigned int addr;
	unsigned int vaddr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int src_pitch;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;	/* clip region */
	unsigned int keyEn;
	unsigned int key;
	unsigned int aen;
	unsigned char alpha;

	unsigned int isTdshp;
	unsigned int isDirty;

	int buff_idx;
	int identity;
	int connected_type;
	unsigned int security;
	int fence_fd;
	struct ion_handle *ion_handles;
	bool fgIonHandleImport;
} OVL_CONFIG_STRUCT;

struct disp_path_config_struct {
	unsigned int srcModule;	/* DISP_MODULE_ENUM */

	/* if srcModule=RDMA0, set following value, else do not have to set following value */
	unsigned int addr;
	unsigned int inFormat;
	unsigned int pitch;
	struct DISP_REGION srcROI;	/* ROI */

	OVL_CONFIG_STRUCT ovl_config;

	struct DISP_REGION bgROI;	/* background ROI */
	unsigned int bgColor;	/* background color */

	unsigned int dstModule;	/* DISP_MODULE_ENUM */
	unsigned int outFormat;
	unsigned int dstAddr;	/* only take effect when dstModule=DISP_MODULE_WDMA0 or DISP_MODULE_WDMA1 */

	int srcWidth, srcHeight;
	int dstWidth, dstHeight;
	int dstPitch;
};

struct disp_path_config_mem_out_struct {
	unsigned int enable;
	unsigned int dirty;
	unsigned int outFormat;
	unsigned int dstAddr;
	struct DISP_REGION srcROI;	/* ROI */
	unsigned int security;
};

/* PANDA, WDMA1 DAPC TASK */
struct disp_path_wdma_config_mem_struct {
	unsigned inputFormat;
	unsigned srcWidth;
	unsigned srcHeight;
	unsigned clipX;
	unsigned clipY;
	unsigned clipWidth;
	unsigned clipHeight;
	DpColorFormat out_formt;
	unsigned dstAddress;
	unsigned dstWidth;
	int useSpecifiedAlpha;
	unsigned char alpha;
};

/* int disp_wait_timeout(bool flag, unsigned int timeout); */
int disp_path_config(struct disp_path_config_struct *pConfig);
int disp_path_config_internal_setting(struct disp_path_config_struct *pConfig);
int disp_path_config_internal_mutex(struct disp_path_config_struct *pConfig);
int disp_path_config_mutex(struct disp_path_config_struct *pConfig, int mutexId);
int disp_path_config_setting(struct disp_path_config_struct *pConfig, int mutexId);
int disp_path_config_layer(OVL_CONFIG_STRUCT *pOvlConfig);
int disp_path_config_layer_addr(unsigned int layer, unsigned int addr);
int disp_path_get_mutex(void);
int disp_path_release_mutex(void);
int disp_path_get_mutex_ex(int mutexID, int sw_mutex);
int disp_path_release_mutex_ex(int mutexID, int sw_mutex);
int disp_path_wait_reg_update(void);

int disp_path_get_mutex_(int mutexId);
int disp_path_release_mutex_(int mutexId);
int disp_path_config_(struct disp_path_config_struct *pConfig, int mutexId);

int disp_path_config_mem_out(struct disp_path_config_mem_out_struct *pConfig);
int disp_path_config_mem_out_without_lcd(struct disp_path_config_mem_out_struct *pConfig);
void disp_path_wait_mem_out_done(void);
int disp_path_clock_on(char *name);
int disp_path_clock_off(char *name);
int disp_path_rdma_start(unsigned idx);
int disp_path_change_tdshp_status(unsigned int layer, unsigned int enable);

void disp_path_clear_mem_out_done_flag(void);
int disp_path_query(void);	/* return different functions according to chip type */

int disp_bls_set_max_backlight(unsigned int level);


#ifdef MTK_OVERLAY_ENGINE_SUPPORT
int disp_path_config_layer_ovl_engine_control(int enable);
int disp_path_config_layer_ovl_engine(OVL_CONFIG_STRUCT *pOvlConfig, int OvlSecure);
int disp_path_config_layer_ovl_engine_ex(OVL_CONFIG_STRUCT *pOvlConfig, int OvlSecure, bool fgCouple);
void disp_path_register_ovl_wdma_callback(void (*callback) (unsigned int param),
					  unsigned int param);
void disp_path_register_ovl_rdma_callback(void (*callback) (unsigned int param),
					  unsigned int param);
void disp_path_unregister_ovl_wdma_callback(void (*callback) (unsigned int param),
					    unsigned int param);
void disp_path_unregister_ovl_rdma_callback(void (*callback) (unsigned int param),
					    unsigned int param);
int disp_path_config_OVL_WDMA_path(int mutex_id, int isolated, int connect);
int disp_path_config_OVL_WDMA(struct disp_path_config_mem_out_struct *pConfig, int OvlSecure);
void disp_path_wait_ovl_wdma_done(void);
int disp_path_get_ovl_en(void);
#endif

#endif
