#ifndef __DRM_DEBUG_H__
#define __DRM_DEBUG_H__

#define TZM_DBG_ERROR(txt, ...) \
	pr_err("ODIN_TZ %s() ERROR: " txt "\n", \
			__func__, ##__VA_ARGS__)

#ifndef DEBUG

#define TZM_DBG(...)		do{} while (0)
#define TZM_DBG_WARN(...)	do{} while (0)

#else
#define TZM_DBG(txt, ...) \
	pr_info("ODIN_TZ %s(): " txt "\n", \
			__func__, \
			##__VA_ARGS__)

#define TZM_DBG_WARN(txt, ...) \
	pr_warn("ODIN_TZ %s() WARNING: " txt "\n", \
			__func__, \
			##__VA_ARGS__)

#endif

#endif /* __DRM_DEBUG_H__ */
