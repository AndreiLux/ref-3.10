#ifndef LOG_PRIORITY
#define LOG_PRIORITY
typedef enum android_log_priority {
        ANDROID_LOG_UNKNOWN = 0,
        ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
        ANDROID_LOG_VERBOSE,
        ANDROID_LOG_DEBUG,
        ANDROID_LOG_INFO,
        ANDROID_LOG_WARN,
        ANDROID_LOG_ERROR,
        ANDROID_LOG_FATAL,
        ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} android_LogPriority;
#endif
