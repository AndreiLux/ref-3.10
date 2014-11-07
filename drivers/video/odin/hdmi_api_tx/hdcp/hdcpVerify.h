/*
 * hdcpVerify.h
 *
 *  Created on: Jul 20, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#ifndef HDCPVERIFY_H_
#define HDCPVERIFY_H_

#include "../util/types.h"

typedef struct
{
	u8 mlength[8];
	u8 mblock[64];
	int mindex;
	int mcomputed;
	int mcorrupted;
	unsigned mdigest[5];
} sha_t;

static const unsigned ksv_len = 5;
static const unsigned ksv_msk = 0x7F;
static const unsigned vrl_length = 0x05;
static const unsigned vrl_header = 5;
static const unsigned vrl_number = 3;
static const unsigned header_g = 10;
static const unsigned shamax = 20;
static const unsigned dsamax = 20;
static const unsigned int_ksv_sha1 = 1;

void sha_reset(sha_t *sha);

int sha_result(sha_t *sha);

void sha_input(sha_t *sha, const u8 * data, unsigned int size);

void sha_processblock(sha_t *sha);

void sha_padmessage(sha_t *sha);

int hdcpverify_dsa(const u8 * M, unsigned int n, const u8 * r, const u8 * s);

int hdcpverify_arrayadd(u8 * r, const u8 * a, const u8 * b, unsigned int n);

int hdcpverify_arraycmp(const u8 * a, const u8 * b, unsigned int n);

void hdcpverify_arraycpy(u8 * dst, const u8 * src, unsigned int n);

int hdcpverify_arraydiv(u8 * r, const u8 * D, const u8 * d, unsigned int n);

int hdcpverify_arraymac(u8 * r, const u8 * M, const u8 m, unsigned int n);

int hdcpverify_arraymul(u8 * r, const u8 * M, const u8 * m, unsigned int n);

void hdcpverify_arrayset(u8 * dst, const u8 src, unsigned int n);

int hdcpverify_arraysub(u8 * r, const u8 * a, const u8 * b, unsigned int n);

void hdcpverify_arrayswp(u8 * r, unsigned int n);

int hdcpverify_arraytst(const u8 * a, const u8 b, unsigned int n);

int hdcpverify_computeext(u8 * c, const u8 * M, const u8 * e, const u8 * p,
		unsigned int n, unsigned int nE);

int hdcpverify_computeinv(u8 * out, const u8 * z, const u8 * a, unsigned int n);

int hdcpverify_computemod(
	u8 * dst, const u8 * src, const u8 * p, unsigned int n);

int hdcpverify_computemul(u8 * p, const u8 * a, const u8 * b, const u8 * m,
		unsigned int n);

int hdcpverify_ksv(const u8 * data, unsigned int size);

int hdcpverify_srm(const u8 * data, unsigned int size);

#endif /* HDCPVERIFY_H_ */
