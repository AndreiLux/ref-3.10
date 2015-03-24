#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include "x_os.h"


#define restrict
#define strtoull simple_strtoull
extern void *memrchr(const void *s, int c, size_t n);
extern long long strtoll(const char *restrict str, char **restrict endptr, int base);


VOID *x_memcpy(VOID *pv_to, const VOID *pv_from, SIZE_T z_l)
{
	if ((pv_to == (CHAR *) NULL)) {
		return pv_to;
	}
	return memcpy(pv_to, pv_from, z_l);
}
EXPORT_SYMBOL(x_memcpy);


INT32 x_memcmp(const VOID *pv_s1, const VOID *pv_s2, SIZE_T z_l)
{
	return memcmp(pv_s1, pv_s2, z_l);
}
EXPORT_SYMBOL(x_memcmp);


VOID *x_memmove(VOID *pv_to, const VOID *pv_from, SIZE_T z_l)
{
	return memmove(pv_to, pv_from, z_l);
}
EXPORT_SYMBOL(x_memmove);


VOID *x_memset(VOID *pv_to, UINT8 ui1_c, SIZE_T z_l)
{
	return memset(pv_to, ui1_c, z_l);
}
EXPORT_SYMBOL(x_memset);


VOID *x_memchr(const VOID *pv_mem, UINT8 ui1_c, SIZE_T z_len)
{
	return memchr(pv_mem, ui1_c, z_len);
}
EXPORT_SYMBOL(x_memchr);

CHAR *x_strdup(const CHAR *ps_str)
{
	CHAR *ps_dup_str = NULL;

	if (ps_str != NULL) {
		SIZE_T z_len;

		z_len = strlen(ps_str) + 1;
		ps_dup_str = (CHAR *) x_mem_alloc(z_len);
		if (ps_dup_str != NULL) {
			memcpy(ps_dup_str, ps_str, z_len);
		}
	}

	return ps_dup_str;
}
EXPORT_SYMBOL(x_strdup);


CHAR *x_strcpy(CHAR *ps_to, const CHAR *ps_from)
{
	return (CHAR *) strcpy((char *)ps_to, (const char *)ps_from);
}
EXPORT_SYMBOL(x_strcpy);


CHAR *x_strncpy(CHAR *ps_to, const CHAR *ps_from, SIZE_T z_len)
{
	return (CHAR *) strncpy((char *)ps_to, (const char *)ps_from, (size_t) z_len);
}
EXPORT_SYMBOL(x_strncpy);


INT32 x_strcmp(const CHAR *ps_str1, const CHAR *ps_str2)
{
	return (INT32) strcmp((const char *)ps_str1, (const char *)ps_str2);
}
EXPORT_SYMBOL(x_strcmp);


INT32 x_strncmp(const CHAR *ps_s1, const CHAR *ps_s2, SIZE_T z_l)
{
	return (INT32) strncmp((const char *)ps_s1, (const char *)ps_s2, (size_t) z_l);
}
EXPORT_SYMBOL(x_strncmp);


INT32 x_strcasecmp(const CHAR *ps_str1, const CHAR *ps_str2)
{
	return (INT32) strcasecmp((const char *)ps_str1, (const char *)ps_str2);
}
EXPORT_SYMBOL(x_strcasecmp);


INT32 x_strncasecmp(const CHAR *ps_str1, const CHAR *ps_str2, SIZE_T z_len)
{
	return (INT32) strncasecmp((const char *)ps_str1, (const char *)ps_str2, (size_t) z_len);
}
EXPORT_SYMBOL(x_strncasecmp);


CHAR *x_strcat(CHAR *ps_to, const CHAR *ps_append)
{
	return (CHAR *) strcat((char *)ps_to, (const char *)ps_append);
}
EXPORT_SYMBOL(x_strcat);


CHAR *x_strncat(CHAR *ps_to, const CHAR *ps_append, SIZE_T z_len)
{
	return (CHAR *) strncat((char *)ps_to, (const char *)ps_append, (size_t) z_len);
}
EXPORT_SYMBOL(x_strncat);


CHAR *x_strchr(const CHAR *ps_str, CHAR c_char)
{
	return (CHAR *) strchr((const char *)ps_str, (int)c_char);
}
EXPORT_SYMBOL(x_strchr);


CHAR *x_strrchr(const CHAR *ps_str, CHAR c_char)
{
	return (CHAR *) strrchr((const char *)ps_str, (int)c_char);
}
EXPORT_SYMBOL(x_strrchr);


CHAR *x_strstr(const CHAR *ps_str, const CHAR *ps_find)
{
	return (CHAR *) strstr((const char *)ps_str, (const char *)ps_find);
}
EXPORT_SYMBOL(x_strstr);


UINT64 x_strtoull(const CHAR *pc_beg_ptr, CHAR **ppc_end_ptr, UINT8 ui1_base)
{
	return (UINT64) strtoull((const char *)pc_beg_ptr, (char **)ppc_end_ptr, (int)ui1_base);
}
EXPORT_SYMBOL(x_strtoull);

SIZE_T x_strlen(const CHAR *ps_str)
{
	if (ps_str == NULL) {
		return 0;
	}
	return (SIZE_T) strlen((const char *)ps_str);
}
EXPORT_SYMBOL(x_strlen);


SIZE_T x_strspn(const CHAR *ps_str, const CHAR *ps_accept)
{
	return (SIZE_T) strspn((const char *)ps_str, (const char *)ps_accept);
}
EXPORT_SYMBOL(x_strspn);


SIZE_T x_strcspn(const CHAR *ps_str, const CHAR *ps_reject)
{
	return (SIZE_T) strcspn((const char *)ps_str, (const char *)ps_reject);
}
EXPORT_SYMBOL(x_strcspn);


CHAR *x_str_toupper(CHAR *ps_str)
{
	if (ps_str != NULL) {
		CHAR *ps_cursor;
		CHAR c_char;

		ps_cursor = ps_str;

		while ((c_char = *ps_cursor) != '\0') {
			*ps_cursor++ = toupper(c_char);
		}
	}

	return ps_str;
}
EXPORT_SYMBOL(x_str_toupper);


CHAR *x_str_tolower(CHAR *ps_str)
{
	if (ps_str != NULL) {
		CHAR *ps_cursor;
		CHAR c_char;

		ps_cursor = ps_str;

		while ((c_char = *ps_cursor) != '\0') {
			*ps_cursor++ = tolower(c_char);
		}
	}

	return ps_str;
}
EXPORT_SYMBOL(x_str_tolower);


CHAR *x_strtok(CHAR *ps_str, const CHAR *ps_delimiters, CHAR **pps_str, SIZE_T *pz_token_len)
{
	CHAR *ps_token;

	ps_token = NULL;

	if ((ps_str != NULL) && (ps_delimiters != NULL) && (pps_str != NULL)) {
		ps_token = ps_str + x_strspn(ps_str, ps_delimiters);

		if (*ps_token != '\0') {
			SIZE_T z_token_len;

			z_token_len = x_strcspn(ps_token, ps_delimiters);

			*pps_str = ps_token + z_token_len;

			if (pz_token_len != NULL) {
				/* original string is not modified */
				*pz_token_len = z_token_len;
			} else if (ps_token[z_token_len] != '\0') {
				/* a NULL character overwrites part of the original string */
				ps_token[z_token_len] = '\0';
				(*pps_str)++;
			}
		} else {
			*pps_str = NULL;
			ps_token = NULL;
		}
	}

	return ps_token;
}
EXPORT_SYMBOL(x_strtok);


INT32 x_sprintf(CHAR *ps_str, const CHAR *ps_format, ...)
{
	int i4_len;
	va_list t_ap;

	va_start(t_ap, ps_format);
	i4_len = vsprintf((char *)ps_str, (const char *)ps_format, t_ap);
	va_end(t_ap);

	return (INT32) i4_len;
}
EXPORT_SYMBOL(x_sprintf);


INT32 x_vsprintf(CHAR *ps_str, const CHAR *ps_format, VA_LIST va_list)
{
	return (INT32) vsprintf((char *)ps_str, (const char *)ps_format, va_list);
}
EXPORT_SYMBOL(x_vsprintf);


INT32 x_snprintf(CHAR *ps_str, SIZE_T z_size, const CHAR *ps_format, ...)
{
	int i4_len;
	va_list t_ap;

	va_start(t_ap, ps_format);
	i4_len = vsnprintf((char *)ps_str, (size_t) z_size, (const char *)ps_format, t_ap);
	va_end(t_ap);

	return (INT32) i4_len;
}
EXPORT_SYMBOL(x_snprintf);


INT32 x_vsnprintf(CHAR *ps_str, SIZE_T z_size, const CHAR *ps_format, VA_LIST va_list)
{
	return (INT32) vsnprintf((char *)ps_str, (size_t) z_size, (const char *)ps_format, va_list);
}
EXPORT_SYMBOL(x_vsnprintf);


INT32 x_sscanf(const CHAR *ps_buf, const CHAR *ps_fmt, ...)
{
	int i4_len;
	va_list t_ap;

	va_start(t_ap, ps_fmt);
	i4_len = vsscanf((const char *)ps_buf, (const char *)ps_fmt, t_ap);
	va_end(t_ap);

	return (INT32) i4_len;
}
EXPORT_SYMBOL(x_sscanf);


INT32 x_vsscanf(const CHAR *ps_buf, const CHAR *ps_fmt, VA_LIST t_ap)
{
	return (INT32) vsscanf((const char *)ps_buf, (const char *)ps_fmt, t_ap);
}
EXPORT_SYMBOL(x_vsscanf);
