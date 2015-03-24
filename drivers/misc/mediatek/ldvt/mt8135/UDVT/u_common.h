/*-----------------------------------------------------------------------------
 * $RCSfile: u_common.h,v $
 * $Revision: #6 $
 * $Date: 2011/11/30 $
 * $Author: shengnan.wang $
 *
 * Description:
 *         This header file contains type definitions common to the whole
 *         Middleware.
 *---------------------------------------------------------------------------*/

#ifndef _U_COMMON_H_
#define _U_COMMON_H_


/*-----------------------------------------------------------------------------
		    include files
 ----------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stddef.h>


/*-----------------------------------------------------------------------------
		    macros, defines, typedefs, enums
 ----------------------------------------------------------------------------*/

#if !defined(_NO_TYPEDEF_UCHAR_) && !defined(_TYPEDEF_UCHAR_)
typedef unsigned char UCHAR;

#define _TYPEDEF_UCHAR_
#endif

#if !defined(_NO_TYPEDEF_UINT8_) && !defined(_TYPEDEF_UINT8_)
typedef unsigned char UINT8;

#define _TYPEDEF_UINT8_
#endif

#if !defined(_NO_TYPEDEF_UINT16_) && !defined(_TYPEDEF_UINT16_)
typedef unsigned short UINT16;

#define _TYPEDEF_UINT16_
#endif

#if !defined(_NO_TYPEDEF_UINT32_) && !defined(_TYPEDEF_UINT32_)

#ifndef EXT_UINT32_TYPE
typedef unsigned long UINT32;
#else
typedef EXT_UINT32_TYPE UINT32;
#endif

#define _TYPEDEF_UINT32_
#endif

#if !defined(_NO_TYPEDEF_UINT64_) && !defined(_TYPEDEF_UINT64_)

#ifndef EXT_UINT64_TYPE
typedef unsigned long long UINT64;
#else
typedef EXT_UINT64_TYPE UINT64;
#endif

#define _TYPEDEF_UINT64_
#endif

#if !defined(_NO_TYPEDEF_CHAR_) && !defined(_TYPEDEF_CHAR_)
typedef char CHAR;

#define _TYPEDEF_CHAR_
#endif

#if !defined(_NO_TYPEDEF_INT8_) && !defined(_TYPEDEF_INT8_)
typedef signed char INT8;

#define _TYPEDEF_INT8_
#endif

#if !defined(_NO_TYPEDEF_INT16_) && !defined(_TYPEDEF_INT16_)
typedef signed short INT16;

#define _TYPEDEF_INT16_
#endif

#if !defined(_NO_TYPEDEF_INT32_) && !defined(_TYPEDEF_INT32_)

#ifndef EXT_INT32_TYPE
typedef signed long INT32;
#else
typedef EXT_INT32_TYPE INT32;
#endif

#define _TYPEDEF_INT32_
#endif

#if !defined(_NO_TYPEDEF_INT64_) && !defined(_TYPEDEF_INT64_)

#ifndef EXT_INT64_TYPE
typedef signed long long INT64;
#else
typedef EXT_INT64_TYPE INT64;
#endif

#define _TYPEDEF_INT64_
#endif

#if !defined(_NO_TYPEDEF_SIZE_T_) && !defined(_TYPEDEF_SIZE_T_)

#ifndef EXT_SIZE_T_TYPE
typedef size_t SIZE_T;
#else
typedef EXT_SIZE_T_TYPE SIZE_T;
#endif

#define _TYPEDEF_SIZE_T_
#endif

#if !defined(_NO_TYPEDEF_UTF16_T_) && !defined(_TYPEDEF_UTF16_T_)
typedef unsigned short UTF16_T;

#define _TYPEDEF_UTF16_T_
#endif

#if !defined(_NO_TYPEDEF_UTF32_T_) && !defined(_TYPEDEF_UTF32_T_)
typedef unsigned long UTF32_T;

#define _TYPEDEF_UTF32_T_
#endif

#if !defined(_NO_TYPEDEF_FLOAT_) && !defined(_TYPEDEF_FLOAT_)
typedef float FLOAT;

#define _TYPEDEF_FLOAT_
#endif

#if !defined(_NO_TYPEDEF_DOUBLE_)  && !defined(_TYPEDEF_DOUBLE_)
typedef double DOUBLE;

#define _TYPEDEF_DOUBLE_
#endif

/* Do not typedef VOID but use define. Will keep some */
/* compilers very happy.                              */
#if !defined(_NO_TYPEDEF_VOID_)  && !defined(_TYPEDEF_VOID_)
#undef VOID
#define VOID  void

#define _TYPEDEF_VOID_
#endif

/* Define a boolean as 8 bits. */
#if !defined(_NO_TYPEDEF_BOOL_) && !defined(_TYPEDEF_BOOL_)

#ifndef EXT_BOOL_TYPE
typedef UINT8 BOOL;
#else
typedef signed EXT_BOOL_TYPE BOOL;
#endif

#define _TYPEDEF_BOOL_

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define TRUE ((BOOL) 1)
#define FALSE ((BOOL) 0)
#endif

/* Variable argument macros. The ones named "va_..." are defined */
/* in the header file "stdarg.h".                                */
#ifdef VA_LIST
#undef VA_LIST
#endif

#ifdef VA_START
#undef VA_START
#endif

#ifdef VA_END
#undef VA_END
#endif

#ifdef VA_ARG
#undef VA_ARG
#endif

#ifdef VA_COPY
#undef VA_COPY
#endif

#define VA_LIST  va_list
#define VA_START va_start
#define VA_END   va_end
#define VA_ARG   va_arg
#define VA_COPY  va_copy


/* Min and max macros. Watch for side effects! */
#define X_MIN(_x, _y)  (((_x) < (_y)) ? (_x) : (_y))
#define X_MAX(_x, _y)  (((_x) > (_y)) ? (_x) : (_y))

/* The following macros are useful to create bit masks. */
#define MAKE_BIT_MASK_8(_val)  (((UINT8)  1) << _val)
#define MAKE_BIT_MASK_16(_val) (((UINT16) 1) << _val)
#define MAKE_BIT_MASK_32(_val) (((UINT32) 1) << _val)
#define MAKE_BIT_MASK_64(_val) (((UINT64) 1) << _val)

/* The following macros can be used to convert to and from standard    */
/* endian. Standard endian is defined as BIG ENDIAN in the middleware. */
/* Note that one, and only one, of the definitions _CPU_BIG_ENDIAN_ or */
/* _CPU_LITTLE_ENDIAN_ must be set.                                    */
#ifndef __BIG_ENDIAN
#ifndef _CPU_LITTLE_ENDIAN_
#define _CPU_LITTLE_ENDIAN_        1
#undef _CPU_BIG_ENDIAN_
#endif
#else
#define _CPU_BIG_ENDIAN_
#undef _CPU_LITTLE_ENDIAN_
#endif

/* The following macros swap 16, 32 or 64 bit values from big */
/* to little or from little to big endian.                    */
#define SWAP_END_16(_val)                             \
    (((((UINT16) _val) & ((UINT16) 0x00ff)) << 8)  |  \
     ((((UINT16) _val) & ((UINT16) 0xff00)) >> 8))

#define SWAP_END_32(_val)                                  \
    (((((UINT32) _val) & ((UINT32) 0x000000ff)) << 24)  |  \
     ((((UINT32) _val) & ((UINT32) 0x0000ff00)) <<  8)  |  \
     ((((UINT32) _val) & ((UINT32) 0x00ff0000)) >>  8)  |  \
     ((((UINT32) _val) & ((UINT32) 0xff000000)) >> 24))

#define SWAP_END_64(_val)                                             \
    (((((UINT64) _val) & ((UINT64) 0x00000000000000ffLL)) << 56)   |  \
     ((((UINT64) _val) & ((UINT64) 0x000000000000ff00LL)) << 40)   |  \
     ((((UINT64) _val) & ((UINT64) 0x0000000000ff0000LL)) << 24)   |  \
     ((((UINT64) _val) & ((UINT64) 0x00000000ff000000LL)) <<  8)   |  \
     ((((UINT64) _val) & ((UINT64) 0x000000ff00000000LL)) >>  8)   |  \
     ((((UINT64) _val) & ((UINT64) 0x0000ff0000000000LL)) >> 24)   |  \
     ((((UINT64) _val) & ((UINT64) 0x00ff000000000000LL)) >> 40)   |  \
     ((((UINT64) _val) & ((UINT64) 0xff00000000000000LL)) >> 56))


/* The following macros return a 16, 32 or 64 bit value given a reference */
/* to a memory location and the endian of the data representation.        */
#define GET_UINT16_FROM_PTR_BIG_END(_ptr)      \
    ((((UINT16) (((UINT8 *) _ptr)[0])) << 8) | \
     ((UINT16)  (((UINT8 *) _ptr)[1])))

#define GET_UINT32_FROM_PTR_BIG_END(_ptr)       \
    ((((UINT32) (((UINT8 *) _ptr)[0])) << 24) | \
     (((UINT32) (((UINT8 *) _ptr)[1])) << 16) | \
     (((UINT32) (((UINT8 *) _ptr)[2])) <<  8) | \
     ((UINT32)  (((UINT8 *) _ptr)[3])))

#define GET_UINT64_FROM_PTR_BIG_END(_ptr)       \
    ((((UINT64) (((UINT8 *) _ptr)[0])) << 56) | \
     (((UINT64) (((UINT8 *) _ptr)[1])) << 48) | \
     (((UINT64) (((UINT8 *) _ptr)[2])) << 40) | \
     (((UINT64) (((UINT8 *) _ptr)[3])) << 32) | \
     (((UINT64) (((UINT8 *) _ptr)[4])) << 24) | \
     (((UINT64) (((UINT8 *) _ptr)[5])) << 16) | \
     (((UINT64) (((UINT8 *) _ptr)[6])) <<  8) | \
     ((UINT64)  (((UINT8 *) _ptr)[7])))

#define GET_INT16_FROM_PTR_BIG_END(_ptr)  ((INT16) GET_UINT16_FROM_PTR_BIG_END(_ptr))
#define GET_INT32_FROM_PTR_BIG_END(_ptr)  ((INT32) GET_UINT32_FROM_PTR_BIG_END(_ptr))
#define GET_INT64_FROM_PTR_BIG_END(_ptr)  ((INT64) GET_UINT64_FROM_PTR_BIG_END(_ptr))

#define GET_UINT16_FROM_PTR_LITTLE_END(_ptr)   \
    ((((UINT16) (((UINT8 *) _ptr)[1])) << 8) | \
     ((UINT16)  (((UINT8 *) _ptr)[0])))

#define GET_UINT32_FROM_PTR_LITTLE_END(_ptr)    \
    ((((UINT32) (((UINT8 *) _ptr)[3])) << 24) | \
     (((UINT32) (((UINT8 *) _ptr)[2])) << 16) | \
     (((UINT32) (((UINT8 *) _ptr)[1])) <<  8) | \
     ((UINT32)  (((UINT8 *) _ptr)[0])))

#define GET_UINT64_FROM_PTR_LITTLE_END(_ptr)    \
    ((((UINT64) (((UINT8 *) _ptr)[7])) << 56) | \
     (((UINT64) (((UINT8 *) _ptr)[6])) << 48) | \
     (((UINT64) (((UINT8 *) _ptr)[5])) << 40) | \
     (((UINT64) (((UINT8 *) _ptr)[4])) << 32) | \
     (((UINT64) (((UINT8 *) _ptr)[3])) << 24) | \
     (((UINT64) (((UINT8 *) _ptr)[2])) << 16) | \
     (((UINT64) (((UINT8 *) _ptr)[1])) <<  8) | \
     ((UINT64)  (((UINT8 *) _ptr)[0])))

#define GET_INT16_FROM_PTR_LITTLE_END(_ptr)  ((INT16) GET_UINT16_FROM_PTR_LITTLE_END(_ptr))
#define GET_INT32_FROM_PTR_LITTLE_END(_ptr)  ((INT32) GET_UINT32_FROM_PTR_LITTLE_END(_ptr))
#define GET_INT64_FROM_PTR_LITTLE_END(_ptr)  ((INT64) GET_UINT64_FROM_PTR_LITTLE_END(_ptr))

#define GET_UINT16_FROM_PTR_STD_END(_ptr)  GET_UINT16_FROM_PTR_BIG_END(_ptr)
#define GET_UINT32_FROM_PTR_STD_END(_ptr)  GET_UINT32_FROM_PTR_BIG_END(_ptr)
#define GET_UINT64_FROM_PTR_STD_END(_ptr)  GET_UINT64_FROM_PTR_BIG_END(_ptr)
#define GET_INT16_FROM_PTR_STD_END(_ptr)   ((INT16) GET_UINT16_FROM_PTR_BIG_END(_ptr))
#define GET_INT32_FROM_PTR_STD_END(_ptr)   ((INT32) GET_UINT32_FROM_PTR_BIG_END(_ptr))
#define GET_INT64_FROM_PTR_STD_END(_ptr)   ((INT64) GET_UINT64_FROM_PTR_BIG_END(_ptr))


/* The following macros place a 16, 32 or 64 bit value in big or */
/* little endian format into a memory location.                  */
#define PUT_UINT16_TO_PTR_BIG_END(_val, _ptr)                        \
    ((UINT8 *) _ptr)[0] = (UINT8) ((((UINT16) _val) & 0xff00) >> 8); \
    ((UINT8 *) _ptr)[1] = (UINT8) (((UINT16)  _val) & 0x00ff);

#define PUT_UINT32_TO_PTR_BIG_END(_val, _ptr)                             \
    ((UINT8 *) _ptr)[0] = (UINT8) ((((UINT32) _val) & 0xff000000) >> 24); \
    ((UINT8 *) _ptr)[1] = (UINT8) ((((UINT32) _val) & 0x00ff0000) >> 16); \
    ((UINT8*) _ptr) [2] = (UINT8) ((((UINT32) _val) & 0x0000ff00) >> 8); \
    ((UINT8 *) _ptr)[3] = (UINT8) (((UINT32)  _val) & 0x000000ff);

#define PUT_UINT64_TO_PTR_BIG_END(_val, _ptr)                                       \
    ((UINT8 *) _ptr)[0] = (UINT8) ((((UINT64) _val) & 0xff00000000000000LL) >> 56); \
    ((UINT8 *) _ptr)[1] = (UINT8) ((((UINT64) _val) & 0x00ff000000000000LL) >> 48); \
    ((UINT8 *) _ptr)[2] = (UINT8) ((((UINT64) _val) & 0x0000ff0000000000LL) >> 40); \
    ((UINT8 *) _ptr)[3] = (UINT8) ((((UINT64) _val) & 0x000000ff00000000LL) >> 32); \
    ((UINT8 *) _ptr)[4] = (UINT8) ((((UINT64) _val) & 0x00000000ff000000LL) >> 24); \
    ((UINT8 *) _ptr)[5] = (UINT8) ((((UINT64) _val) & 0x0000000000ff0000LL) >> 16); \
    ((UINT8 *) _ptr)[6] = (UINT8) ((((UINT64) _val) & 0x000000000000ff00LL) >>  8); \
    ((UINT8 *) _ptr)[7] = (UINT8) (((UINT64)  _val) & 0x00000000000000ffLL);

#define PUT_INT16_TO_PTR_BIG_END(_val, _ptr)  PUT_UINT16_TO_PTR_BIG_END(_val, _ptr)
#define PUT_INT32_TO_PTR_BIG_END(_val, _ptr)  PUT_UINT32_TO_PTR_BIG_END(_val, _ptr)
#define PUT_INT64_TO_PTR_BIG_END(_val, _ptr)  PUT_UINT64_TO_PTR_BIG_END(_val, _ptr)

#define PUT_UINT16_TO_PTR_LITTLE_END(_val, _ptr)                     \
    ((UINT8 *) _ptr)[1] = (UINT8) ((((UINT16) _val) & 0xff00) >> 8); \
    ((UINT8 *) _ptr)[0] = (UINT8) (((UINT16)  _val) & 0x00ff);

#define PUT_UINT32_TO_PTR_LITTLE_END(_val, _ptr)                          \
    ((UINT8 *) _ptr)[3] = (UINT8) ((((UINT32) _val) & 0xff000000) >> 24); \
    ((UINT8 *) _ptr)[2] = (UINT8) ((((UINT32) _val) & 0x00ff0000) >> 16); \
    ((UINT8*) _ptr) [1] = (UINT8) ((((UINT32) _val) & 0x0000ff00) >> 8); \
    ((UINT8 *) _ptr)[0] = (UINT8) (((UINT32)  _val) & 0x000000ff);

#define PUT_UINT64_TO_PTR_LITTLE_END(_val, _ptr)                                    \
    ((UINT8 *) _ptr)[7] = (UINT8) ((((UINT64) _val) & 0xff00000000000000LL) >> 56); \
    ((UINT8 *) _ptr)[6] = (UINT8) ((((UINT64) _val) & 0x00ff000000000000LL) >> 48); \
    ((UINT8 *) _ptr)[5] = (UINT8) ((((UINT64) _val) & 0x0000ff0000000000LL) >> 40); \
    ((UINT8 *) _ptr)[4] = (UINT8) ((((UINT64) _val) & 0x000000ff00000000LL) >> 32); \
    ((UINT8 *) _ptr)[3] = (UINT8) ((((UINT64) _val) & 0x00000000ff000000LL) >> 24); \
    ((UINT8 *) _ptr)[2] = (UINT8) ((((UINT64) _val) & 0x0000000000ff0000LL) >> 16); \
    ((UINT8 *) _ptr)[1] = (UINT8) ((((UINT64) _val) & 0x000000000000ff00LL) >>  8); \
    ((UINT8 *) _ptr)[0] = (UINT8) (((UINT64)  _val) & 0x00000000000000ffLL);

#define PUT_INT16_TO_PTR_LITTLE_END(_val, _ptr)  PUT_UINT16_TO_PTR_LITTLE_END(_val, _ptr)
#define PUT_INT32_TO_PTR_LITTLE_END(_val, _ptr)  PUT_UINT32_TO_PTR_LITTLE_END(_val, _ptr)
#define PUT_INT64_TO_PTR_LITTLE_END(_val, _ptr)  PUT_UINT64_TO_PTR_LITTLE_END(_val, _ptr)

#define PUT_UINT16_TO_PTR_STD_END(_val, _ptr)  PUT_UINT16_TO_PTR_BIG_END(_val, _ptr)
#define PUT_UINT32_TO_PTR_STD_END(_val, _ptr)  PUT_UINT32_TO_PTR_BIG_END(_val, _ptr)
#define PUT_UINT64_TO_PTR_STD_END(_val, _ptr)  PUT_UINT64_TO_PTR_BIG_END(_val, _ptr)
#define PUT_INT16_TO_PTR_STD_END(_val, _ptr)   PUT_UINT16_TO_PTR_BIG_END(_val, _ptr)
#define PUT_INT32_TO_PTR_STD_END(_val, _ptr)   PUT_UINT32_TO_PTR_BIG_END(_val, _ptr)
#define PUT_INT64_TO_PTR_STD_END(_val, _ptr)   PUT_UINT64_TO_PTR_BIG_END(_val, _ptr)


#ifdef _CPU_BIG_ENDIAN_
#define CONV_BIG_END_TO_UINT16(_val)  ((UINT16) _val)
#define CONV_BIG_END_TO_UINT32(_val)  ((UINT32) _val)
#define CONV_BIG_END_TO_UINT64(_val)  ((UINT64) _val)
#define CONV_BIG_END_TO_INT16(_val)   ((INT16)  _val)
#define CONV_BIG_END_TO_INT32(_val)   ((INT32)  _val)
#define CONV_BIG_END_TO_INT64(_val)   ((INT64)  _val)

#define CONV_LITTLE_END_TO_UINT16(_val)  ((UINT16) SWAP_END_16(_val))
#define CONV_LITTLE_END_TO_UINT32(_val)  ((UINT32) SWAP_END_32(_val))
#define CONV_LITTLE_END_TO_UINT64(_val)  ((UINT64) SWAP_END_64(_val))
#define CONV_LITTLE_END_TO_INT16(_val)   ((INT16)  SWAP_END_16(_val))
#define CONV_LITTLE_END_TO_INT32(_val)   ((INT32)  SWAP_END_32(_val))
#define CONV_LITTLE_END_TO_INT64(_val)   ((INT64)  SWAP_END_64(_val))

#define CONV_STD_END_TO_UINT16(_val)  ((UINT16) _val)
#define CONV_STD_END_TO_UINT32(_val)  ((UINT32) _val)
#define CONV_STD_END_TO_UINT64(_val)  ((UINT64) _val)
#define CONV_STD_END_TO_INT16(_val)   ((INT16)  _val)
#define CONV_STD_END_TO_INT32(_val)   ((INT32)  _val)
#define CONV_STD_END_TO_INT64(_val)   ((INT64)  _val)

#endif

#ifdef _CPU_LITTLE_ENDIAN_
#define CONV_STD_END_TO_UINT16(_val)  ((UINT16) SWAP_END_16(_val))
#define CONV_STD_END_TO_UINT32(_val)  ((UINT32) SWAP_END_32(_val))
#define CONV_STD_END_TO_UINT64(_val)  ((UINT64) SWAP_END_64(_val))
#define CONV_STD_END_TO_INT16(_val)   ((INT16)  SWAP_END_16(_val))
#define CONV_STD_END_TO_INT32(_val)   ((INT32)  SWAP_END_32(_val))
#define CONV_STD_END_TO_INT64(_val)   ((INT64)  SWAP_END_64(_val))

#define CONV_BIG_END_TO_UINT16(_val)  ((UINT16) SWAP_END_16(_val))
#define CONV_BIG_END_TO_UINT32(_val)  ((UINT32) SWAP_END_32(_val))
#define CONV_BIG_END_TO_UINT64(_val)  ((UINT64) SWAP_END_64(_val))
#define CONV_BIG_END_TO_INT16(_val)   ((INT16)  SWAP_END_16(_val))
#define CONV_BIG_END_TO_INT32(_val)   ((INT32)  SWAP_END_32(_val))
#define CONV_BIG_END_TO_INT64(_val)   ((INT64)  SWAP_END_64(_val))

#define CONV_LITTLE_END_TO_UINT16(_val)  ((UINT16) _val)
#define CONV_LITTLE_END_TO_UINT32(_val)  ((UINT32) _val)
#define CONV_LITTLE_END_TO_UINT64(_val)  ((UINT64) _val)
#define CONV_LITTLE_END_TO_INT16(_val)   ((INT16)  _val)
#define CONV_LITTLE_END_TO_INT32(_val)   ((INT32)  _val)
#define CONV_LITTLE_END_TO_INT64(_val)   ((INT64)  _val)

#endif

/* The following type definition is used to store a STC / PTS value, which is */
/* in multiples of 90 kHz. Note that such a value is 33 bit in size. Hence,   */
/* the bits [32:0] will contain the actual STC / PTS value and the bits       */
/* [63:33] must be set '0'.                                                   */
typedef UINT64 STC_T;
typedef UINT64 PTS_T;

/* The following type is used to carry system or local time and date information. */
typedef INT64 TIME_T;

/* The following define the value of Null Time. */
#define NULL_TIME  ((TIME_T) 0)

/* The following definitions are used to define a MPEG-2 PID variable. */
typedef UINT16 MPEG_2_PID_T;

#define MPEG_2_NULL_PID  ((MPEG_2_PID_T) 0x1fff)


/* The following definitions are used to define a MPEG-2 stream id variable. */
typedef UINT8 MPEG_2_STREAM_ID_T;

#define MPEG_2_NULL_STREAM_ID  ((MPEG_2_STREAM_ID_T) 0x00)


/* The following definition is used for ISO-639 language definitions. Note that */
/* such a language string MUST always be zero terminated so that comparisons or */
/* copying will become simpler. Note that the minimum value of ISO_639_LANG_LEN */
/* is '4'. It MUST NOT be set to a lower value.                                 */
#define ISO_639_LANG_LEN  4

typedef CHAR ISO_639_LANG_T[ISO_639_LANG_LEN];

#define ISO_639_DEFAULT  ""


/* The following definition is used for ISO-3166 coutry code definitions. Note that  */
/* such a language string MUST always be zero terminated so that comparisons or      */
/* copying will become simpler. Note that the minimum value of ISO_3166_COUNT_LEN is */
/* '4'. It MUST NOT be set to a lower value.                                         */
#define ISO_3166_COUNT_LEN  4

typedef CHAR ISO_3166_COUNT_T[ISO_3166_COUNT_LEN];

#define ISO_3166_DEFAULT  ""


/* The following structures and definitions are used to convey text style info.  */
/* Structure STYLE_T contains a single style which may be applicable to a single */
/* character or a complete string. Structure STYLE_STR_T contains a style which  */
/* may be applicable only to a few characters within a string. It is pretty      */
/* common to convey an array of STYLE_STR_T elements. However, that is not a     */
/* requirement.                                                                  */
#define STYLE_EFFECT_BOLD          ((UINT16) 0x0001)
#define STYLE_EFFECT_UNDERLINE     ((UINT16) 0x0002)
#define STYLE_EFFECT_SUB_SCRIPT    ((UINT16) 0x0004)
#define STYLE_EFFECT_SUPER_SCRIPT  ((UINT16) 0x0008)

typedef struct _STYLE_T {
	UINT16 ui2_style;
} STYLE_T;

typedef struct _STYLE_STR_T {
	UINT32 ui4_first_char;
	UINT32 ui4_num_char;

	STYLE_T t_style;
} STYLE_STR_T;


/* The following stream type enumeration and structure is used to identify an     */
/* individual stream component within the whole middleware. Note that the number  */
/* of stream types may never exceed 32 else we have an overflow in the stream     */
/* type bitmask. The values 24 to 31 (inclusive) are reserved for internal usage. */
typedef enum {
	ST_UNKNOWN = -1,	/* Must be set to '-1' else I loose an entry in the bit mask. */
	ST_AUDIO,
	ST_VIDEO,
	/* fleet temp */
	ST_ISDB_CAPTION,
	ST_ISDB_TEXT,
	/* ///////////// */
	ST_CLOSED_CAPTION,
	ST_TELETEXT,
	ST_SUBTITLE,
	ST_DATA,
	ST_EXTRACTION
} STREAM_TYPE_T;

#if 1				/* shrink au fifo for 3d256 */
typedef enum {
	DMX_UNKNOWN = 0,
	DMX_PRIMARY_AUDIO,
	DMX_SECONDARY_AUDIO,
	DMX_PRIMARY_VIDEO,
	DMX_SECONDARY_VIDEO,
	DMX_RE_VIDEO,
	DMX_IG,
	DMX_PG,
	DMX_TS,
	DMX_CLOSED_CAPTION
} STREAM_DEMUX_TYPE_T;
#endif

#define ST_MASK_AUDIO           MAKE_BIT_MASK_32(ST_AUDIO)
#define ST_MASK_VIDEO           MAKE_BIT_MASK_32(ST_VIDEO)

    /* fleet temp */
#define ST_MASK_ISDB_CAPTION    MAKE_BIT_MASK_32(ST_ISDB_CAPTION)
#define ST_MASK_ISDB_TEXT       MAKE_BIT_MASK_32(ST_ISDB_TEXT)
/* ///////////// */


#define ST_MASK_CLOSED_CAPTION  MAKE_BIT_MASK_32(ST_CLOSED_CAPTION)
#define ST_MASK_TELETEXT        MAKE_BIT_MASK_32(ST_TELETEXT)
#define ST_MASK_SUBTITLE        MAKE_BIT_MASK_32(ST_SUBTITLE)
#define ST_MASK_DATA            MAKE_BIT_MASK_32(ST_DATA)
#define ST_MASK_EXTRACTION      MAKE_BIT_MASK_32(ST_EXTRACTION)

typedef struct _STREAM_COMP_ID_T {
	STREAM_TYPE_T e_type;	/* Specifies audio, video etc. */

	VOID *pv_stream_tag;
} STREAM_COMP_ID_T;


/* The folowing structure is used during initialization to specify */
/* a components thread priority and stack size.                    */
#define DEFAULT_PRIORITY    ((UINT8)  0)
#define DEFAULT_STACK_SIZE  ((SIZE_T) 0)
#define DEFAULT_NUM_MSGS    ((UINT16) 0)

typedef struct _THREAD_DESCR_T {
	SIZE_T z_stack_size;

	UINT8 ui1_priority;

	UINT16 ui2_num_msgs;
} THREAD_DESCR_T;

/* The following structure contains a generic transponder description. */
typedef UINT8 BRDCST_TYPE_T;

#define BRDCST_TYPE_UNKNOWN  ((BRDCST_TYPE_T) 0)
#define BRDCST_TYPE_ANALOG   ((BRDCST_TYPE_T) 1)
#define BRDCST_TYPE_DVB      ((BRDCST_TYPE_T) 2)
#define BRDCST_TYPE_ATSC     ((BRDCST_TYPE_T) 3)
#define BRDCST_TYPE_SCTE     ((BRDCST_TYPE_T) 4)


typedef UINT8 BRDCST_MEDIUM_T;

#define BRDCST_MEDIUM_UNKNOWN          ((BRDCST_MEDIUM_T) 0)
#define BRDCST_MEDIUM_DIG_TERRESTRIAL  ((BRDCST_MEDIUM_T) 1)
#define BRDCST_MEDIUM_DIG_CABLE        ((BRDCST_MEDIUM_T) 2)
#define BRDCST_MEDIUM_DIG_SATELLITE    ((BRDCST_MEDIUM_T) 3)
#define BRDCST_MEDIUM_ANA_TERRESTRIAL  ((BRDCST_MEDIUM_T) 4)
#define BRDCST_MEDIUM_ANA_CABLE        ((BRDCST_MEDIUM_T) 5)
#define BRDCST_MEDIUM_ANA_SATELLITE    ((BRDCST_MEDIUM_T) 6)
#define BRDCST_MEDIUM_1394             ((BRDCST_MEDIUM_T) 7)


#define TS_DESCR_FLAG_NW_ID_SOFT_DEFINED  ((UINT8) 0x01)
#define TS_DESCR_FLAG_ON_ID_SOFT_DEFINED  ((UINT8) 0x02)
#define TS_DESCR_FLAG_TS_ID_SOFT_DEFINED  ((UINT8) 0x04)

#define TS_DESCR_FLAG_IDS_SOFT_DEFINED  (TS_DESCR_FLAG_NW_ID_SOFT_DEFINED | \
					 TS_DESCR_FLAG_ON_ID_SOFT_DEFINED | \
					 TS_DESCR_FLAG_TS_ID_SOFT_DEFINED)

typedef struct _TS_DESCR_T {
	BRDCST_TYPE_T e_bcst_type;
	BRDCST_MEDIUM_T e_bcst_medium;

	UINT16 ui2_nw_id;	/* Some form of network identifier. */

	UINT16 ui2_on_id;	/* Original network id. */
	UINT16 ui2_ts_id;	/* Transport stream id. */

	UINT16 ui2_res_1;	/* Reserved for future use. */

	UINT8 ui1_res_2;
	UINT8 ui1_flags;	/* Some flags. */
} TS_DESCR_T;


/* The following enumeration and bitmasks are used to convey */
/* TV-System type information.                               */
typedef enum
{
	TV_SYS_UNKNOWN = -1,	/* Must be set to '-1' else I loose an entry in the bit mask. */
	TV_SYS_A,
	TV_SYS_B,
	TV_SYS_C,
	TV_SYS_D,
	TV_SYS_E,
	TV_SYS_F,
	TV_SYS_G,
	TV_SYS_H,
	TV_SYS_I,
	TV_SYS_J,
	TV_SYS_K,
	TV_SYS_K_PRIME,
	TV_SYS_L,
	TV_SYS_L_PRIME,
	TV_SYS_M,
	TV_SYS_N
} TV_SYS_T;

#define TV_SYS_MASK_A        MAKE_BIT_MASK_32(TV_SYS_A)
#define TV_SYS_MASK_B        MAKE_BIT_MASK_32(TV_SYS_B)
#define TV_SYS_MASK_C        MAKE_BIT_MASK_32(TV_SYS_C)
#define TV_SYS_MASK_D        MAKE_BIT_MASK_32(TV_SYS_D)
#define TV_SYS_MASK_E        MAKE_BIT_MASK_32(TV_SYS_E)
#define TV_SYS_MASK_F        MAKE_BIT_MASK_32(TV_SYS_F)
#define TV_SYS_MASK_G        MAKE_BIT_MASK_32(TV_SYS_G)
#define TV_SYS_MASK_H        MAKE_BIT_MASK_32(TV_SYS_H)
#define TV_SYS_MASK_I        MAKE_BIT_MASK_32(TV_SYS_I)
#define TV_SYS_MASK_J        MAKE_BIT_MASK_32(TV_SYS_J)
#define TV_SYS_MASK_K        MAKE_BIT_MASK_32(TV_SYS_K)
#define TV_SYS_MASK_K_PRIME  MAKE_BIT_MASK_32(TV_SYS_K_PRIME)
#define TV_SYS_MASK_L        MAKE_BIT_MASK_32(TV_SYS_L)
#define TV_SYS_MASK_L_PRIME  MAKE_BIT_MASK_32(TV_SYS_L_PRIME)
#define TV_SYS_MASK_M        MAKE_BIT_MASK_32(TV_SYS_M)
#define TV_SYS_MASK_N        MAKE_BIT_MASK_32(TV_SYS_N)

#define TV_SYS_MASK_NONE     ((UINT32) 0)


/* The following enumeration and bitmasks are used to convey */
/* Color-System type information.                            */
typedef enum {
	COLOR_SYS_UNKNOWN = -1,	/* Must be set to '-1' else I loose an entry in the bit mask. */
	COLOR_SYS_NTSC,
	COLOR_SYS_PAL,
	COLOR_SYS_SECAM
} COLOR_SYS_T;

#define COLOR_SYS_MASK_NTSC   MAKE_BIT_MASK_16(COLOR_SYS_NTSC)
#define COLOR_SYS_MASK_PAL    MAKE_BIT_MASK_16(COLOR_SYS_PAL)
#define COLOR_SYS_MASK_SECAM  MAKE_BIT_MASK_16(COLOR_SYS_SECAM)

#define COLOR_SYS_MASK_NONE   ((UINT16) 0)


/* The following enumeration and bitmasks are used to convey */
/* Audio-System type information.                            */
typedef enum {
	AUDIO_SYS_UNKNOWN = -1,	/* Must be set to '-1' else I loose an entry in the bit mask. */
	AUDIO_SYS_AM,
	AUDIO_SYS_FM_MONO,
	AUDIO_SYS_FM_EIA_J,
	AUDIO_SYS_FM_A2,
	AUDIO_SYS_FM_A2_DK1,
	AUDIO_SYS_FM_A2_DK2,
	AUDIO_SYS_FM_RADIO,
	AUDIO_SYS_NICAM,
	AUDIO_SYS_BTSC
} AUDIO_SYS_T;

#define AUDIO_SYS_MASK_AM         MAKE_BIT_MASK_16(AUDIO_SYS_AM)
#define AUDIO_SYS_MASK_FM_MONO    MAKE_BIT_MASK_16(AUDIO_SYS_FM_MONO)
#define AUDIO_SYS_MASK_FM_EIA_J   MAKE_BIT_MASK_16(AUDIO_SYS_FM_EIA_J)
#define AUDIO_SYS_MASK_FM_A2      MAKE_BIT_MASK_16(AUDIO_SYS_FM_A2)
#define AUDIO_SYS_MASK_FM_A2_DK1  MAKE_BIT_MASK_16(AUDIO_SYS_FM_A2_DK1)
#define AUDIO_SYS_MASK_FM_A2_DK2  MAKE_BIT_MASK_16(AUDIO_SYS_FM_A2_DK2)
#define AUDIO_SYS_MASK_FM_RADIO   MAKE_BIT_MASK_16(AUDIO_SYS_FM_RADIO)
#define AUDIO_SYS_MASK_NICAM      MAKE_BIT_MASK_16(AUDIO_SYS_NICAM)
#define AUDIO_SYS_MASK_BTSC       MAKE_BIT_MASK_16(AUDIO_SYS_BTSC)

#define AUDIO_SYS_MASK_NONE   ((UINT16) 0)

/* Data formats */
typedef UINT8 DATA_FMT_T;

#define DATA_FMT_UNKNOWN    ((DATA_FMT_T)  0)
#define DATA_FMT_ANALOG     ((DATA_FMT_T)  1)
#define DATA_FMT_MPEG_2     ((DATA_FMT_T)  2)
#define DATA_FMT_PES        ((DATA_FMT_T)  3)
#define DATA_FMT_MP3        ((DATA_FMT_T)  4)
#define DATA_FMT_AAC        ((DATA_FMT_T)  5)

#endif				/* _U_COMMON_H_ */
