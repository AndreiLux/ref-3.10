#ifndef _PLF_TRACE_H_
#define _PLF_TRACE_H_

#define	HVALUE_SIZE	9	/* 8 chars (max value ffffffff) + 1 char (',' or NULL) */
#define	DVALUE_SIZE	12	/* 10 chars (max value 4,294,967,295) + 1 char (',' or NULL) */

char* ms_formatH(char *__restrict__ buf, unsigned char cnt, unsigned int *__restrict__ value);
char* ms_formatD(char *__restrict__ buf, unsigned char cnt, unsigned int *__restrict__ value);

void ms_emi(unsigned char cnt, unsigned int *value);
void ms_ttype(unsigned char cnt, unsigned int *value);
void ms_bw_limiter(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_smi(unsigned char cnt, unsigned int *value);
void ms_smit(unsigned char cnt, unsigned int *value);

void ms_th(unsigned long long timestamp, unsigned char cnt, unsigned int *value);
void ms_dramc(unsigned long long timestamp, unsigned char cnt, unsigned int *value);

void ms_mali(unsigned int freq, int cnt, unsigned int *value);

#endif // _PLF_TRACE_H_
