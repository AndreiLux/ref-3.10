#include <mach/mt_typedefs.h>
#include <linux/types.h>
#include "disp_drv.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include "mtkfb_priv.h"
#include <linux/disp_assert_layer.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

/* /common part */
#define DAL_BPP             (2)
#define DAL_WIDTH           (DISP_GetScreenWidth())
#define DAL_HEIGHT          (DISP_GetScreenHeight())

#ifdef CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER

#include <linux/string.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/module.h>

#include "mtkfb_console.h"

#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
#include "disp_ovl_engine_api.h"
#endif

/* --------------------------------------------------------------------------- */
#define DAL_FORMAT          (eRGB565)
#define DAL_BG_COLOR        (dal_bg_color)
#define DAL_FG_COLOR        (dal_fg_color)

#define RGB888_To_RGB565(x) ((((x) & 0xF80000) >> 8) |                      \
			     (((x) & 0x00FC00) >> 5) |                      \
			     (((x) & 0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low)  (((low) << 16) | (high))

#define DAL_LOCK()                                                          \
    do {                                                                    \
	if (down_interruptible(&dal_sem)) {                                 \
	    DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Can't get semaphore in %s()\n",          \
		   __func__);                                           \
	    return DAL_STATUS_LOCK_FAIL;                                    \
	}                                                                   \
    } while (0)

#define DAL_UNLOCK()                                                        \
    do {                                                                    \
	up(&dal_sem);                                                       \
    } while (0)


#define DAL_CHECK_MFC_RET(expr)                                             \
    do {                                                                    \
	MFC_STATUS ret = (expr);                                            \
	if (MFC_STATUS_OK != ret) {                                         \
	    DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call MFC_XXX function failed "           \
		   "in %s(), line: %d, ret: %x\n",                          \
		   __func__, __LINE__, ret);                            \
	    return ret;                                                     \
	}                                                                   \
    } while (0)


#define DAL_CHECK_DISP_RET(expr)                                            \
    do {                                                                    \
	DISP_STATUS ret = (expr);                                           \
	if (DISP_STATUS_OK != ret) {                                        \
	    DISP_LOG_PRINT(ANDROID_LOG_WARN, "DAL", "Warning: call DISP_XXX function failed "          \
		   "in %s(), line: %d, ret: %x\n",                          \
		   __func__, __LINE__, ret);                            \
	    return ret;                                                     \
	}                                                                   \
    } while (0)

#define DAL_LOG(fmt, arg...) \
    do { \
	DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", fmt, ##arg); \
    } while (0)
/* --------------------------------------------------------------------------- */

static MFC_HANDLE mfc_handle;

static void *dal_fb_addr;
static UINT32 dal_fb_pa;
unsigned int isAEEEnabled = 0;
extern BOOL is_early_suspended;
extern struct semaphore sem_early_suspend;

/* static BOOL  dal_shown   = FALSE; */
BOOL dal_shown = FALSE;
static BOOL dal_enable_when_resume = FALSE;
static BOOL dal_disable_when_resume = FALSE;
static unsigned int dal_fg_color = RGB888_To_RGB565(DAL_COLOR_WHITE);
static unsigned int dal_bg_color = RGB888_To_RGB565(DAL_COLOR_RED);

extern struct mutex OverlaySettingMutex;
extern atomic_t OverlaySettingDirtyFlag;
extern atomic_t OverlaySettingApplied;
extern OVL_CONFIG_STRUCT cached_layer_config[DDP_OVL_LAYER_MUN];

#define DAL_LOWMEMORY_ASSERT

#ifdef DAL_LOWMEMORY_ASSERT

static unsigned int dal_lowmemory_fg_color = RGB888_To_RGB565(DAL_COLOR_PINK);
static unsigned int dal_lowmemory_bg_color = RGB888_To_RGB565(DAL_COLOR_YELLOW);
static BOOL dal_enable_when_resume_lowmemory = FALSE;
static BOOL dal_disable_when_resume_lowmemory = FALSE;
static BOOL dal_lowMemory_shown = FALSE;
static const char low_memory_msg[] = { "Low Memory!" };

#define DAL_LOWMEMORY_FG_COLOR (dal_lowmemory_fg_color)
#define DAL_LOWMEMORY_BG_COLOR (dal_lowmemory_bg_color)
#endif


/* DECLARE_MUTEX(dal_sem); */
DEFINE_SEMAPHORE(dal_sem);

static char dal_print_buffer[1024];
/* --------------------------------------------------------------------------- */
typedef struct {
	struct semaphore sem;

	unsigned char *fb_addr;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int fb_bpp;
	unsigned int fg_color;
	unsigned int bg_color;
	unsigned int rows;
	unsigned int cols;
	unsigned int cursor_row;
	unsigned int cursor_col;
	unsigned int font_width;
	unsigned int font_height;
} MFC_CONTEXT;

#ifdef DAL_LOWMEMORY_ASSERT
static DAL_STATUS Show_LowMemory(void)
{
	UINT32 update_width, update_height;
	MFC_CONTEXT *ctxt = (MFC_CONTEXT *) mfc_handle;

	if (!dal_shown) {	/* only need show lowmemory assert */
		update_width = ctxt->font_width * strlen(low_memory_msg);
		update_height = ctxt->font_height;
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "update size:%d,%d", update_width,
			       update_height);
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = ASSERT_LAYER;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.src_offset_x = DAL_WIDTH - update_width;
			layer.src_offset_y = 0;
			layer.src_width = update_width;
			layer.src_height = update_height;
			layer.tgt_offset_x = DAL_WIDTH - update_width;
			layer.tgt_offset_y = 0;
			layer.tgt_width = update_width;
			layer.tgt_height = update_height;
			layer.layer_enable = TRUE;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		mutex_lock(&OverlaySettingMutex);
		cached_layer_config[ASSERT_LAYER].src_x = DAL_WIDTH - update_width;
		cached_layer_config[ASSERT_LAYER].src_y = 0;
		cached_layer_config[ASSERT_LAYER].src_w = update_width;
		cached_layer_config[ASSERT_LAYER].src_h = update_height;
		cached_layer_config[ASSERT_LAYER].dst_x = DAL_WIDTH - update_width;
		cached_layer_config[ASSERT_LAYER].dst_y = 0;
		cached_layer_config[ASSERT_LAYER].dst_w = update_width;
		cached_layer_config[ASSERT_LAYER].dst_h = update_height;
		cached_layer_config[ASSERT_LAYER].layer_en = TRUE;
		cached_layer_config[ASSERT_LAYER].isDirty = true;
		atomic_set(&OverlaySettingDirtyFlag, 1);
		atomic_set(&OverlaySettingApplied, 0);
		mutex_unlock(&OverlaySettingMutex);
#endif
	}
/*
	if(!dal_lowMemory_shown)
		DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0,
					 DAL_WIDTH,
					 DAL_HEIGHT));
*/
	return DAL_STATUS_OK;
}
#endif

UINT32 DAL_GetLayerSize(void)
{
	/* xuecheng, avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

static DAL_STATUS DAL_SetRedScreen(UINT32 *addr)
{
	UINT32 i;
	const UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);

	for (i = 0; i < DAL_GetLayerSize() / sizeof(UINT32); ++i) {
		*addr++ = BG_COLOR;
	}
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_Init(UINT32 layerVA, UINT32 layerPA)
{
	pr_info("%s", __func__);

	dal_fb_addr = (void *)layerVA;
	dal_fb_pa = layerPA;
	DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr,
				   DAL_WIDTH, DAL_HEIGHT, DAL_BPP, DAL_FG_COLOR, DAL_BG_COLOR));

	/* DAL_Clean(); */
	DAL_SetRedScreen((UINT32 *) dal_fb_addr);
	return DAL_STATUS_OK;
}


DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

	DAL_LOCK();
	dal_fg_color = RGB888_To_RGB565(fgColor);
	dal_bg_color = RGB888_To_RGB565(bgColor);
	DAL_CHECK_MFC_RET(MFC_SetColor(mfc_handle, dal_fg_color, dal_bg_color));
	DAL_UNLOCK();

	return DAL_STATUS_OK;
}

DAL_STATUS DAL_Dynamic_Change_FB_Layer(unsigned int isAEEEnabled)
{
	static int ui_layer_tdshp;

	pr_info("[DDP] DAL_Dynamic_Change_FB_Layer, isAEEEnabled=%d\n", isAEEEnabled);

	if (DISP_DEFAULT_UI_LAYER_ID == DISP_CHANGED_UI_LAYER_ID) {
		pr_info("[DDP] DAL_Dynamic_Change_FB_Layer, no dynamic switch\n");
		return DAL_STATUS_OK;
	}

	if (isAEEEnabled == 1) {
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = DISP_DEFAULT_UI_LAYER_ID;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.layer_id = DISP_CHANGED_UI_LAYER_ID;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
			layer.layer_id = DISP_DEFAULT_UI_LAYER_ID;
			layer.isTdshp = 0;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		/* change ui layer from DISP_DEFAULT_UI_LAYER_ID to DISP_CHANGED_UI_LAYER_ID */
		memcpy((void *)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]),
		       (void *)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]),
		       sizeof(OVL_CONFIG_STRUCT));
		ui_layer_tdshp = cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp;
		cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = 0;
#endif
		disp_path_change_tdshp_status(DISP_DEFAULT_UI_LAYER_ID, 0);	/* change global variable value, else error-check will find layer 2, 3 enable tdshp together */
		FB_LAYER = DISP_CHANGED_UI_LAYER_ID;
	} else {
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = DISP_CHANGED_UI_LAYER_ID;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.layer_id = DISP_DEFAULT_UI_LAYER_ID;
			layer.isTdshp = ui_layer_tdshp;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		memcpy((void *)(&cached_layer_config[DISP_DEFAULT_UI_LAYER_ID]),
		       (void *)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]),
		       sizeof(OVL_CONFIG_STRUCT));
		cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isTdshp = ui_layer_tdshp;
#endif
		FB_LAYER = DISP_DEFAULT_UI_LAYER_ID;
		memset((void *)(&cached_layer_config[DISP_CHANGED_UI_LAYER_ID]), 0,
		       sizeof(OVL_CONFIG_STRUCT));
	}

#if !defined(MTK_OVERLAY_ENGINE_SUPPORT)
	/* no matter memcpy or memset, layer ID should not be changed */
	cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].layer = DISP_DEFAULT_UI_LAYER_ID;
	cached_layer_config[DISP_CHANGED_UI_LAYER_ID].layer = DISP_CHANGED_UI_LAYER_ID;
	cached_layer_config[DISP_DEFAULT_UI_LAYER_ID].isDirty = 1;
	cached_layer_config[DISP_CHANGED_UI_LAYER_ID].isDirty = 1;
#endif

	return DAL_STATUS_OK;
}

DAL_STATUS DAL_Clean(void)
{
	const UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);
	DAL_STATUS ret = DAL_STATUS_OK;
	UINT32 i, *ptr;
	static int dal_clean_cnt;

	pr_info("[MTKFB_DAL] DAL_Clean\n");
	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

/* if (LCD_STATE_POWER_OFF == LCD_GetState()) */
/* return DAL_STATUS_LCD_IN_SUSPEND; */

	DAL_LOCK();

	DAL_CHECK_MFC_RET(MFC_ResetCursor(mfc_handle));

	ptr = (UINT32 *) dal_fb_addr;
	for (i = 0; i < DAL_GetLayerSize() / sizeof(UINT32); ++i) {
		*ptr++ = BG_COLOR;
	}
/*
    if (LCD_STATE_POWER_OFF == LCD_GetState()) {
	    DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "dal_clean in power off\n");
	dal_disable_when_resume = TRUE;
	ret = DAL_STATUS_LCD_IN_SUSPEND;
	goto End;
    }
    */
	if (down_interruptible(&sem_early_suspend)) {
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "can't get semaphore in DAL_Clean()\n");
		goto End;
	}
	/* xuecheng, for debug */
#if 0
	if (is_early_suspended) {
		up(&sem_early_suspend);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "dal_clean in power off\n");
		goto End;
	}
#endif
	up(&sem_early_suspend);

#if !defined(MTK_OVERLAY_ENGINE_SUPPORT)
	mutex_lock(&OverlaySettingMutex);
#endif

	/* TODO: if dal_shown=false, and 3D enabled, mtkfb may disable UI layer, please modify 3D driver */
	if (isAEEEnabled == 1) {
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = ASSERT_LAYER;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.layer_enable = FALSE;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		cached_layer_config[ASSERT_LAYER].layer_en = FALSE;
		cached_layer_config[ASSERT_LAYER].isDirty = true;
#endif
		/* DAL disable, switch UI layer to default layer 3 */
		pr_info("[DDP]* isAEEEnabled from 1 to 0, %d\n", dal_clean_cnt++);
		isAEEEnabled = 0;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);	/* restore UI layer to DEFAULT_UI_LAYER */
	}

	dal_shown = FALSE;
#ifdef DAL_LOWMEMORY_ASSERT
	if (dal_lowMemory_shown) {	/* only need show lowmemory assert */
		UINT32 LOWMEMORY_FG_COLOR =
		    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR, DAL_LOWMEMORY_FG_COLOR);
		UINT32 LOWMEMORY_BG_COLOR =
		    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR, DAL_LOWMEMORY_BG_COLOR);

		DAL_CHECK_MFC_RET(MFC_LowMemory_Printf
				  (mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR,
				   LOWMEMORY_BG_COLOR));
		Show_LowMemory();
	}
	dal_enable_when_resume_lowmemory = FALSE;
	dal_disable_when_resume_lowmemory = FALSE;
#endif
	dal_disable_when_resume = FALSE;

#if !defined(MTK_OVERLAY_ENGINE_SUPPORT)
	atomic_set(&OverlaySettingDirtyFlag, 1);
	atomic_set(&OverlaySettingApplied, 0);
	mutex_unlock(&OverlaySettingMutex);
#endif


	DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0, DAL_WIDTH, DAL_HEIGHT));


 End:
	DAL_UNLOCK();
	return ret;
}


DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	va_list args;
	uint i;
	DAL_STATUS ret = DAL_STATUS_OK;
	pr_info("%s", __func__);
	/* pr_info("[MTKFB_DAL] DAL_Printf mfc_handle=0x%08X, fmt=0x%08X\n", mfc_handle, fmt); */
	if (NULL == mfc_handle)
		return DAL_STATUS_NOT_READY;

	if (NULL == fmt)
		return DAL_STATUS_INVALID_ARGUMENT;

	DAL_LOCK();
	if (isAEEEnabled == 0) {
		pr_info("[DDP] isAEEEnabled from 0 to 1, ASSERT_LAYER=%d, dal_fb_pa %x\n",
			ASSERT_LAYER, dal_fb_pa);

		isAEEEnabled = 1;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);	/* default_ui_ layer coniig to changed_ui_layer */

		DAL_CHECK_MFC_RET(MFC_Open(&mfc_handle, dal_fb_addr,
					   DAL_WIDTH, DAL_HEIGHT, DAL_BPP,
					   DAL_FG_COLOR, DAL_BG_COLOR));
		/* DAL_Clean(); */

#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = ASSERT_LAYER;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.src_phy_addr = (void *)dal_fb_pa;
			/* TODO: Alpha has not implement. */
			layer.src_pitch = DAL_WIDTH;
			layer.src_fmt = MTK_FB_FORMAT_RGB565;
			layer.src_offset_x = 0;
			layer.src_offset_y = 0;
			layer.src_width = DAL_WIDTH;
			layer.src_height = DAL_HEIGHT;
			layer.tgt_offset_x = 0;
			layer.tgt_offset_y = 0;
			layer.tgt_width = DAL_WIDTH;
			layer.tgt_height = DAL_HEIGHT;
			layer.layer_enable = TRUE;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		cached_layer_config[ASSERT_LAYER].addr = dal_fb_pa;
		cached_layer_config[ASSERT_LAYER].alpha = 0x80;
		cached_layer_config[ASSERT_LAYER].aen = TRUE;
		cached_layer_config[ASSERT_LAYER].src_pitch = DAL_WIDTH * DAL_BPP;
		cached_layer_config[ASSERT_LAYER].fmt = DAL_FORMAT;
		cached_layer_config[ASSERT_LAYER].src_x = 0;
		cached_layer_config[ASSERT_LAYER].src_y = 0;
		cached_layer_config[ASSERT_LAYER].src_w = DAL_WIDTH;
		cached_layer_config[ASSERT_LAYER].src_h = DAL_HEIGHT;
		cached_layer_config[ASSERT_LAYER].dst_x = 0;
		cached_layer_config[ASSERT_LAYER].dst_y = 0;
		cached_layer_config[ASSERT_LAYER].dst_w = DAL_WIDTH;
		cached_layer_config[ASSERT_LAYER].dst_h = DAL_HEIGHT;
		cached_layer_config[ASSERT_LAYER].layer_en = TRUE;
		cached_layer_config[ASSERT_LAYER].isDirty = true;
#endif
	}
	va_start(args, fmt);
	i = vsprintf(dal_print_buffer, fmt, args);
	BUG_ON(i >= ARRAY_SIZE(dal_print_buffer));
	va_end(args);
	DAL_CHECK_MFC_RET(MFC_Print(mfc_handle, dal_print_buffer));

	flush_cache_all();
/*
    if (LCD_STATE_POWER_OFF == LCD_GetState()) {
	ret = DAL_STATUS_LCD_IN_SUSPEND;
	dal_enable_when_resume = TRUE;
	goto End;
    }
    */
	if (down_interruptible(&sem_early_suspend)) {
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "can't get semaphore in DAL_Printf()\n");
		goto End;
	}
#if 0
	if (is_early_suspended) {
		up(&sem_early_suspend);
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "DAL", "DAL_Printf in power off\n");
		goto End;
	}
#endif
	up(&sem_early_suspend);

#if !defined(MTK_OVERLAY_ENGINE_SUPPORT)
	mutex_lock(&OverlaySettingMutex);
#endif

	if (!dal_shown) {
		dal_shown = TRUE;
	}
	/* DAL enable, switch ui layer from default 3 to 2 */
#if !defined(MTK_OVERLAY_ENGINE_SUPPORT)
	atomic_set(&OverlaySettingDirtyFlag, 1);
	atomic_set(&OverlaySettingApplied, 0);
	mutex_unlock(&OverlaySettingMutex);
#endif

	DAL_CHECK_DISP_RET(DISP_UpdateScreen(0, 0, DAL_WIDTH, DAL_HEIGHT));
 End:
	DAL_UNLOCK();

	return ret;
}


DAL_STATUS DAL_OnDispPowerOn(void)
{
	DAL_LOCK();

	/* Re-enable assertion layer when display resumes */

	if (is_early_suspended) {
		if (dal_enable_when_resume) {
			dal_enable_when_resume = FALSE;
			if (!dal_shown) {
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
				{
					struct fb_overlay_layer layer = { 0 };
					layer.layer_id = ASSERT_LAYER;
					Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
					layer.src_offset_x = 0;
					layer.src_offset_y = 0;
					layer.src_width = DAL_WIDTH;
					layer.src_height = DAL_HEIGHT;
					layer.tgt_offset_x = 0;
					layer.tgt_offset_y = 0;
					layer.tgt_width = DAL_WIDTH;
					layer.tgt_height = DAL_HEIGHT;
					layer.layer_enable = TRUE;
					Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
				}
#else
				mutex_lock(&OverlaySettingMutex);
				cached_layer_config[ASSERT_LAYER].src_x = 0;
				cached_layer_config[ASSERT_LAYER].src_y = 0;
				cached_layer_config[ASSERT_LAYER].src_w = DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].src_h = DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].dst_x = 0;
				cached_layer_config[ASSERT_LAYER].dst_y = 0;
				cached_layer_config[ASSERT_LAYER].dst_w = DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].dst_h = DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].layer_en = TRUE;
				cached_layer_config[ASSERT_LAYER].isDirty = true;
				atomic_set(&OverlaySettingDirtyFlag, 1);
				atomic_set(&OverlaySettingApplied, 0);
				mutex_unlock(&OverlaySettingMutex);
#endif
				dal_shown = TRUE;
			}
#ifdef DAL_LOWMEMORY_ASSERT
			dal_enable_when_resume_lowmemory = FALSE;
			dal_disable_when_resume_lowmemory = FALSE;
#endif
			goto End;
		} else if (dal_disable_when_resume) {
			dal_disable_when_resume = FALSE;
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
			{
				struct fb_overlay_layer layer = { 0 };
				layer.layer_id = ASSERT_LAYER;
				Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
				layer.layer_enable = FALSE;
				Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
			}
#else
			mutex_lock(&OverlaySettingMutex);
			cached_layer_config[ASSERT_LAYER].layer_en = FALSE;
			cached_layer_config[ASSERT_LAYER].isDirty = true;
			atomic_set(&OverlaySettingDirtyFlag, 1);
			atomic_set(&OverlaySettingApplied, 0);
			mutex_unlock(&OverlaySettingMutex);
#endif
			dal_shown = FALSE;
#ifdef DAL_LOWMEMORY_ASSERT
			if (dal_lowMemory_shown) {	/* only need show lowmemory assert */
				UINT32 LOWMEMORY_FG_COLOR =
				    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR,
							  DAL_LOWMEMORY_FG_COLOR);
				UINT32 LOWMEMORY_BG_COLOR =
				    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR,
							  DAL_LOWMEMORY_BG_COLOR);

				DAL_CHECK_MFC_RET(MFC_LowMemory_Printf
						  (mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR,
						   LOWMEMORY_BG_COLOR));
				Show_LowMemory();
			}
			dal_enable_when_resume_lowmemory = FALSE;
			dal_disable_when_resume_lowmemory = FALSE;
#endif
			goto End;

		}
#ifdef DAL_LOWMEMORY_ASSERT
		if (dal_enable_when_resume_lowmemory) {
			dal_enable_when_resume_lowmemory = FALSE;
			if (!dal_shown) {	/* only need show lowmemory assert */
				Show_LowMemory();
			}
		} else if (dal_disable_when_resume_lowmemory) {
			if (!dal_shown) {	/* only low memory assert shown on screen */
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
				{
					struct fb_overlay_layer layer = { 0 };
					layer.layer_id = ASSERT_LAYER;
					Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
					layer.src_offset_x = 0;
					layer.src_offset_y = 0;
					layer.src_width = DAL_WIDTH;
					layer.src_height = DAL_HEIGHT;
					layer.tgt_offset_x = 0;
					layer.tgt_offset_y = 0;
					layer.tgt_width = DAL_WIDTH;
					layer.tgt_height = DAL_HEIGHT;
					layer.layer_enable = FALSE;
					Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
				}
#else
				mutex_lock(&OverlaySettingMutex);
				cached_layer_config[ASSERT_LAYER].src_x = 0;
				cached_layer_config[ASSERT_LAYER].src_y = 0;
				cached_layer_config[ASSERT_LAYER].src_w = DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].src_h = DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].dst_x = 0;
				cached_layer_config[ASSERT_LAYER].dst_y = 0;
				cached_layer_config[ASSERT_LAYER].dst_w = DAL_WIDTH;
				cached_layer_config[ASSERT_LAYER].dst_h = DAL_HEIGHT;
				cached_layer_config[ASSERT_LAYER].layer_en = FALSE;
				cached_layer_config[ASSERT_LAYER].isDirty = true;
				atomic_set(&OverlaySettingDirtyFlag, 1);
				atomic_set(&OverlaySettingApplied, 0);
				mutex_unlock(&OverlaySettingMutex);
#endif
			}
		} else {
		}
#endif
	}

 End:
	DAL_UNLOCK();

	return DAL_STATUS_OK;
}

#ifdef DAL_LOWMEMORY_ASSERT
DAL_STATUS DAL_LowMemoryOn(void)
{
	UINT32 LOWMEMORY_FG_COLOR =
	    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_FG_COLOR, DAL_LOWMEMORY_FG_COLOR);
	UINT32 LOWMEMORY_BG_COLOR =
	    MAKE_TWO_RGB565_COLOR(DAL_LOWMEMORY_BG_COLOR, DAL_LOWMEMORY_BG_COLOR);
	DAL_LOG("Enter DAL_LowMemoryOn()\n");

	DAL_CHECK_MFC_RET(MFC_LowMemory_Printf
			  (mfc_handle, low_memory_msg, LOWMEMORY_FG_COLOR, LOWMEMORY_BG_COLOR));

	Show_LowMemory();

	dal_lowMemory_shown = TRUE;
	DAL_LOG("Leave DAL_LowMemoryOn()\n");
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_LowMemoryOff(void)
{
	UINT32 BG_COLOR = MAKE_TWO_RGB565_COLOR(DAL_BG_COLOR, DAL_BG_COLOR);
	DAL_LOG("Enter DAL_LowMemoryOff()\n");

	DAL_CHECK_MFC_RET(MFC_SetMem(mfc_handle, low_memory_msg, BG_COLOR));

/* what about LCM_PHYSICAL_ROTATION = 180 */
	if (!dal_shown) {	/* only low memory assert shown on screen */
#if defined(MTK_OVERLAY_ENGINE_SUPPORT)
		{
			struct fb_overlay_layer layer = { 0 };
			layer.layer_id = ASSERT_LAYER;
			Disp_Ovl_Engine_Get_layer_info(mtkfb_instance, &layer);
			layer.src_offset_x = 0;
			layer.src_offset_y = 0;
			layer.src_width = DAL_WIDTH;
			layer.src_height = DAL_HEIGHT;
			layer.tgt_offset_x = 0;
			layer.tgt_offset_y = 0;
			layer.tgt_width = DAL_WIDTH;
			layer.tgt_height = DAL_HEIGHT;
			layer.layer_enable = FALSE;
			Disp_Ovl_Engine_Set_layer_info(mtkfb_instance, &layer);
		}
#else
		mutex_lock(&OverlaySettingMutex);
		cached_layer_config[ASSERT_LAYER].src_x = 0;
		cached_layer_config[ASSERT_LAYER].src_y = 0;
		cached_layer_config[ASSERT_LAYER].src_w = DAL_WIDTH;
		cached_layer_config[ASSERT_LAYER].src_h = DAL_HEIGHT;
		cached_layer_config[ASSERT_LAYER].dst_x = 0;
		cached_layer_config[ASSERT_LAYER].dst_y = 0;
		cached_layer_config[ASSERT_LAYER].dst_w = DAL_WIDTH;
		cached_layer_config[ASSERT_LAYER].dst_h = DAL_HEIGHT;
		cached_layer_config[ASSERT_LAYER].layer_en = FALSE;
		cached_layer_config[ASSERT_LAYER].isDirty = true;
		atomic_set(&OverlaySettingDirtyFlag, 1);
		atomic_set(&OverlaySettingApplied, 0);
		mutex_unlock(&OverlaySettingMutex);
#endif
	}
	dal_lowMemory_shown = FALSE;
	DAL_LOG("Leave DAL_LowMemoryOff()\n");
	return DAL_STATUS_OK;
}
#endif
/* ########################################################################## */
/* !CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
/* ########################################################################## */
#else

UINT32 DAL_GetLayerSize(void)
{
	/* xuecheng, avoid lcdc read buffersize+1 issue */
	return DAL_WIDTH * DAL_HEIGHT * DAL_BPP + 4096;
}

DAL_STATUS DAL_Init(UINT32 layerVA, UINT32 layerPA)
{
	NOT_REFERENCED(layerVA);
	NOT_REFERENCED(layerPA);

	return DAL_STATUS_OK;
}

DAL_STATUS DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	NOT_REFERENCED(fgColor);
	NOT_REFERENCED(bgColor);

	return DAL_STATUS_OK;
}

DAL_STATUS DAL_Clean(void)
{
	pr_info("[MTKFB_DAL] DAL_Clean is not implemented\n");
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_Printf(const char *fmt, ...)
{
	NOT_REFERENCED(fmt);
	pr_info("[MTKFB_DAL] DAL_Printf is not implemented\n");
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_OnDispPowerOn(void)
{
	return DAL_STATUS_OK;
}

#ifdef DAL_LOWMEMORY_ASSERT
DAL_STATUS DAL_LowMemoryOn(void)
{
	return DAL_STATUS_OK;
}

DAL_STATUS DAL_LowMemoryOff(void)
{
	return DAL_STATUS_OK;
}
#endif
#endif				/* CONFIG_MTK_FB_SUPPORT_ASSERTION_LAYER */
EXPORT_SYMBOL(DAL_SetColor);
EXPORT_SYMBOL(DAL_Printf);
EXPORT_SYMBOL(DAL_Clean);
#ifdef DAL_LOWMEMORY_ASSERT
EXPORT_SYMBOL(DAL_LowMemoryOn);
EXPORT_SYMBOL(DAL_LowMemoryOff);
#endif
