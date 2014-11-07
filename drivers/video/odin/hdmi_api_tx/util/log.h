/*
 * log.h
 *
 *  Created on: Jun 25, 2010
 *
 * Synopsys Inc.
 * SG DWC PT02
 */

#ifndef LOG_H_
#define LOG_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
	VERBOSE_ERROR = 0,
	VERBOSE_WARN,
	VERBOSE_NOTICE,
	VERBOSE_DEBUG,
	VERBOSE_TRACE
} verbose_t;

typedef enum
{
	NUMERIC_DEC = 0, NUMERIC_HEX
} numeric_t;

void log_setverbose(verbose_t verbose);

void log_setnumeric(numeric_t numeric);

void log_setverbosewrite(unsigned state);

void log_printwrite(unsigned a, unsigned b);

void log_print0(verbose_t verbose, const char* functionName);

void log_print1(verbose_t verbose, const char* functionName, const char* a);

void log_print2(verbose_t verbose, const char* functionName, const char* a,
		unsigned b);

void log_print3(verbose_t verbose, const char* functionName, const char* a,
		unsigned b, unsigned c);

void log_printint(verbose_t verbose, const char* functionName, unsigned a);

void log_printint2(verbose_t verbose, const char* functionName, unsigned a,
		unsigned b);

#define LOG_ASSERT(x, a)		while (!(x)) { LOG_ERROR(a); exit(-1); }
#define LOG_EXIT(a)				do { LOG_ERROR(a); exit(-1); } while (0)
#define LOG_WRITE(a, b)			log_printwrite((a), (b))

#define LOG_ERROR(a)			log_print1(VERBOSE_ERROR,  __FUNCTION__, (a))
#define LOG_ERROR2(a, b)		log_print2(VERBOSE_ERROR,  __FUNCTION__, (a), (b))
#define LOG_ERROR3(a, b, c)\
	log_print3(VERBOSE_ERROR,  __FUNCTION__, (a), (b), (c))
#define LOG_WARNING(a)			log_print1(VERBOSE_WARN,   __FUNCTION__, (a))
#define LOG_WARNING2(a, b)		log_print2(VERBOSE_WARN,   __FUNCTION__, (a), (b))
#define LOG_WARNING3(a, b, c)\
	log_print3(VERBOSE_WARN,   __FUNCTION__, (a), (b), (c))
#define LOG_NOTICE(a)			log_print1(VERBOSE_NOTICE, __FUNCTION__, (a))
#define LOG_NOTICE2(a, b)		log_print2(VERBOSE_NOTICE, __FUNCTION__, (a), (b))
#define LOG_NOTICE3(a, b, c)\
	log_print3(VERBOSE_NOTICE, __FUNCTION__, (a), (b), (c))
#define LOG_DEBUG(a)			log_print1(VERBOSE_DEBUG,  __FUNCTION__, (a))
#define LOG_DEBUG2(a, b)		log_print2(VERBOSE_DEBUG,  __FUNCTION__, (a), (b))
#define LOG_DEBUG3(a, b, c)\
	log_print3(VERBOSE_DEBUG,  __FUNCTION__, (a), (b), (c))
#define LOG_TRACE()			log_print0(VERBOSE_TRACE,  __FUNCTION__)
#define LOG_TRACE1(a)			log_printint(VERBOSE_TRACE,  __FUNCTION__, (a))
#define LOG_TRACE2(a, b)		log_printint2(VERBOSE_TRACE,  __FUNCTION__, (a), (b))
#define LOG_TRACE3(a, b, c)\
	log_print3(VERBOSE_TRACE,  __FUNCTION__, (a), (b), (c))

#define LOG_MENU(a)			log_print0(VERBOSE_NOTICE, (a))
#define LOG_MENUSTR(a, b)		log_print1(VERBOSE_NOTICE, (a), (b))
#define LOG_MENUINT(a, b)		log_printint(VERBOSE_NOTICE, (a),(b))
#define LOG_MENUINT2(a, b, c)		log_printint2(VERBOSE_NOTICE, (a),(b), (c))

#ifdef __cplusplus
}
#endif

#endif /* LOG_H_ */
