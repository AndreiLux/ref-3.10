/*!
 *****************************************************************************
 *
 * @file	   img_sysdefs.h
 *
 * ---------------------------------------------------------------------------
 *
 * Copyright (c) Imagination Technologies Ltd.
 * 
 * The contents of this file are subject to the MIT license as set out below.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 * 
 * Alternatively, the contents of this file may be used under the terms of the 
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above. 
 * 
 * If you wish to allow use of your version of this file only under the terms 
 * of GPL, and not to allow others to use your version of this file under the 
 * terms of the MIT license, indicate your decision by deleting the provisions 
 * above and replace them with the notice and other provisions required by GPL 
 * as set out in the file called “GPLHEADER” included in this distribution. If 
 * you do not delete the provisions above, a recipient may use your version of 
 * this file under the terms of either the MIT license or GPL.
 * 
 * This License is also included in this distribution in the file called 
 * "MIT_COPYING".
 *
 *****************************************************************************/

#ifndef __IMG_SYSDEFS__
#define __IMG_SYSDEFS__

#include <linux/stddef.h>
#include <linux/string.h> // strcpy, ... and memcpy, ...
#include <linux/kernel.h> 
#include <linux/slab.h> // kmalloc, kcalloc, ...
#include <linux/vmalloc.h>
#include <linux/interrupt.h> // to check if in interrupt context when doing kmalloc

#ifdef __cplusplus
extern "C" {
#endif

/*
 * language abstraction
 */
 
#define IMG_CONST const
#define IMG_INLINE inline

// useful?
//#define IMG_CALLCONV __stdcall
//#define IMG_INTERNAL
//#define IMG_EXPORT	__declspec(dllexport)

// not very nice
#define IMG_LITTLE_ENDIAN	(1234)
#define IMG_BIG_ENDIAN		(4321)
/* Win32 is little endian */
#define	IMG_BYTE_ORDER		IMG_LITTLE_ENDIAN

/**
 * @brief 64bit value prefix - e.g. printf("long %" IMG_INT64PRFX "d")
 *
 * @note On linux depends on the platform (__x86_64__ and __ppc64__ are considered to use long while __i386__ uses long long).
 * But so far on 64b machines long long is 64b wide thus 'long long' can be used regarless of the architecture.
 */
#if defined(__x86_64__) || defined(__ppc64__)
    #define IMG_INT64PRFX "l"
#elif defined(__i386__)
#define IMG_INT64PRFX "ll"
#else
    #define IMG_INT64PRFX "ll"
#endif
/** @brief IMG_SIZE (i.e. size_t) format prefix - e.g. IMG_SIZE s -> printf("sizeof= %" IMG_SIZEPRFX "u", s) */
#define IMG_SIZEPRFX "z"
/** @brief IMG_PTRDIFF (i.e. ptrdiff_t) format prefix - e.g. IMG_PTRDIFF diff -> printf(diff= %" IMG_PTRDIFFPRFX "d", s) */
#define IMG_PTRDIFFPRFX "t"

/*
 * memory operation
 */

  /**
   * @brief kmalloc flags used when calling kmalloc in IMG_MALLOC
   *
   * The two most common flags are:
   * @li GFP_ATOMIC is high priority and cannot sleep (use in interrupt handlers and bottom halves) when using a spin lock. As the kernel cannot put the process to sleep it cannot swap pages etc to maybe make memory available therefore it has more chances to fail. Therefore it should be used only in the contexts where memory allocation must be done very fast in a small amount.
   * @li GFP_KERNEL is a normal priority allocation that can sleep, therefore the kernel can liberate memory if possible (unused pages etc). 
   *
   * Therefore GFP_ATOMIC usage should be avoided in most of the cases.
   * It is sensible however to AVOID allocation in the cases GFP_ATOMIC should be used.
   *
   * The allocation checks if the requiered memory fits in a page.
   * If it does not then malloc is attempted but warning is printed).
   */
#define IMG_KMALLOC_FLAG GFP_KERNEL
 
#define	IMG_MEMCPY(dest,src,size)	memcpy	(dest,src,size)
#define	IMG_MEMSET(ptr,val,size)	memset	(ptr,val,size)
#define IMG_MEMCMP(A,B,size)		memcmp	(A,B,size)
#define IMG_MEMMOVE(dest,src,size)	memmove	(dest,src,size)

/** @brief C89 says: space is unitialized */

/// @return 0 if correct, more than 0 otherwise
inline static int img_verifkmalloc(size_t size, const char* fct, const char* file, uint32_t line)
{
    int ret = 0;
    if (irqs_disabled())
    {
        printk(KERN_WARNING "WARNING: %s with irq disabled! %s:%u", fct, file, line);
        if ( IMG_KMALLOC_FLAG != GFP_ATOMIC ) ret++; // so it only prints the warning
    }
    if ( size > 2*PAGE_SIZE )
    {
		// kernel buddy allocator (on kernels >2.6.35) does not suffer from bad fragmentation.
		// 2 contiguous pages are not very difficult to find. 
        printk(KERN_INFO "WARNING: %s (%zu) > 2 pages. Maybe use vmalloc. %s:%u", fct, size, file, line);
    }
    return ret;
}

inline static void* img_sys_malloc(size_t size, const char *fct, uint32_t line) 
{ 
    void* ptr=NULL; 
    if (!img_verifkmalloc(size, "kmalloc", fct, line)) 
    {
        ptr=kmalloc(size, IMG_KMALLOC_FLAG); 
    }
    return ptr; 
}
#define IMG_SYS_MALLOC(size) img_sys_malloc(size, __FILE__, __LINE__)


/** @brief C89 says: space is initialized to zero bytes */
inline static void* img_sys_calloc(size_t nelem, size_t elem_size, const char *fct, uint32_t line) 
{ 
    void* ptr=NULL; 
    if (!img_verifkmalloc(nelem*elem_size, "kcalloc", fct, line)) 
    {
        ptr=kcalloc(nelem, elem_size, IMG_KMALLOC_FLAG); 
    }
    return ptr; 
}
#define IMG_SYS_CALLOC(nelem, elem_size) img_sys_calloc(nelem, elem_size, __FILE__, __LINE__)

/** @brief C89 says: if the size is larger the new space is uninitialized. */
inline static void* img_sys_realloc(void *ptr, size_t size, const char *fct, uint32_t line) 
{ 
    void* reptr=NULL; 
    if (!img_verifkmalloc(size, "krealloc", fct, line))
    {
        reptr=krealloc(ptr, size, IMG_KMALLOC_FLAG);
    }
    return reptr; 
}
#define IMG_SYS_REALLOC(ptr, size) img_sys_realloc(ptr, size, __FILE__, __LINE__)


#define IMG_SYS_FREE(ptr)         kfree(ptr)
#define IMG_SYS_BIG_MALLOC(size)  vmalloc(size)
#define IMG_SYS_BIG_FREE(ptr)     vfree(ptr)
/** @brief With GCC this aligns the memory */
#define IMG_ALIGN(bytes)       __attribute__ ((aligned (bytes)))

/*
 * string operation
 */

inline static char* img_sys_strdup(char *ptr, const char *fct, uint32_t line)
{ 
    char *str = NULL; 
    if (!img_verifkmalloc(strlen(ptr), "kstrdup", fct, line))
    {
        str=kstrdup(ptr, IMG_KMALLOC_FLAG);
    }
    return str; 
}
#define IMG_SYS_STRDUP(ptr) img_sys_strdup(ptr, __FILE__, __LINE__)

#define IMG_STRCMP(A,B)		strcmp(A,B)
#define IMG_STRCPY(dest,src)		strcpy(dest,src)
#define IMG_STRNCPY(dest,src,size)	strncpy(dest,src,size)
#define IMG_STRLEN(ptr) strlen(ptr)
 
/*
 * file operation
 */

/*
 * If IMG_NO_FSEEK64 is defined then the FSEEK64 is not set
 * This allows the projects to choose if they want FSEEK64 support
 *
 * Similar operation for FTELL64
 */

// force to NO
#define IMG_NO_FTELL64
// force to NO
#define IMG_NO_FSEEK64


/*
 * assert behaviour
 *                               NDEBUG
 *                      0                 1
 * EXIT_ON_ASSERT  0   test||print        void
 *                 1   assert()           assert() // ignored  no print, no exit
 */
 
// C89 says: if NDEBUG is defined at the time <assert.h> is included the assert macro is ignored
#ifdef EXIT_ON_ASSERT 
#define IMG_ASSERT(expected) {(WARN_ON(!(expected));0)}

#else // NO_EXIT_ON_ASSERT is defined

// IMG_ASSERT is enabled in release: customer logs are not useful unless asserts are visible
// #define IMG_ASSERT(expected) ({0;})
#define IMG_ASSERT(expected) (void)( (expected) || (printk(KERN_ERR "Assertion failed: %s, file %s, line %d\n", #expected, __FILE__, __LINE__), dump_stack(),0) )

#endif // NO_EXIT_ON_ASSERT

#ifdef __cplusplus
}
#endif

#endif // __IMG_SYSDEFS__
