/*
 * hdcpVerify.c
 *
 *  Created on: Jul 20, 2010
 *
 *  Synopsys Inc.
 *  SG DWC PT02
 */

#include "hdcpVerify.h"
#include "../util/log.h"

#define SIZE	(160/8)
#define KSIZE	(1024/8)

void sha_reset(sha_t *sha)
{
	unsigned int i = 0;
	sha->mindex = 0;
	sha->mcomputed = FALSE;
	sha->mcorrupted = FALSE;
	for (i = 0; i < sizeof(sha->mlength); i++)
	{
		sha->mlength[i] = 0;
	}
	sha->mdigest[0] = 0x67452301;
	sha->mdigest[1] = 0xEFCDAB89;
	sha->mdigest[2] = 0x98BADCFE;
	sha->mdigest[3] = 0x10325476;
	sha->mdigest[4] = 0xC3D2E1F0;
}

int sha_result(sha_t *sha)
{
	if (sha->mcorrupted == TRUE)
	{
		return FALSE;
	}
	if (sha->mcomputed == FALSE)
	{
		sha_padmessage(sha);
		sha->mcomputed = TRUE;
	}
	return TRUE;
}

void sha_input(sha_t *sha, const u8 * data, unsigned int size)
{
	int i = 0;
	unsigned j = 0;
	int rc = TRUE;
	if (data == 0 || size == 0)
	{
		LOG_ERROR("invalid input data");
		return;
	}
	if (sha->mcomputed == TRUE || sha->mcorrupted == TRUE)
	{
		sha->mcorrupted = TRUE;
		return;
	}
	while (size-- && sha->mcorrupted == FALSE)
	{
		sha->mblock[sha->mindex++] = *data;

		for (i = 0; i < 8; i++)
		{
			rc = TRUE;
			for (j = 0; j < sizeof(sha->mlength); j++)
			{
				sha->mlength[j]++;
				if (sha->mlength[j] != 0)
				{
					rc = FALSE;
					break;
				}
			}
			sha->mcorrupted = (sha->mcorrupted == TRUE || rc == TRUE) ? TRUE
					: FALSE;
		}
		/* if corrupted then message is too long */
		if (sha->mindex == 64)
		{
			sha_processblock(sha);
		}
		data++;
	}
}

void sha_processblock(sha_t *sha)
{
#define shaCircularShift(bits,word)\
	((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

	const unsigned k[] =
	{ /* constants defined in SHA-1 */
	0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
	unsigned w[80]; /* word sequence */
	unsigned a, b, c, d, e; /* word buffers */
	unsigned temp = 0;
	int t = 0;

	/* Initialize the first 16 words in the array W */
	for (t = 0; t < 80; t++)
	{
		if (t < 16)
		{
			w[t] = ((unsigned) sha->mblock[t * 4 + 0]) << 24;
			w[t] |= ((unsigned) sha->mblock[t * 4 + 1]) << 16;
			w[t] |= ((unsigned) sha->mblock[t * 4 + 2]) << 8;
			w[t] |= ((unsigned) sha->mblock[t * 4 + 3]) << 0;
		}
		else
		{
			w[t] = shaCircularShift(1, w[t-3] ^ w[t-8] ^ w[t-14] ^ w[t-16]);
		}
	}

	a = sha->mdigest[0];
	b = sha->mdigest[1];
	c = sha->mdigest[2];
	d = sha->mdigest[3];
	e = sha->mdigest[4];

	for (t = 0; t < 80; t++)
	{
		temp = shaCircularShift(5, a);
		if (t < 20)
		{
			temp += ((b & c) | ((~b) & d)) + e + w[t] + k[0];
		}
		else if (t < 40)
		{
			temp += (b ^ c ^ d) + e + w[t] + k[1];
		}
		else if (t < 60)
		{
			temp += ((b & c) | (b & d) | (c & d)) + e + w[t] + k[2];
		}
		else
		{
			temp += (b ^ c ^ d) + e + w[t] + k[3];
		}
		e = d;
		d = c;
		c = shaCircularShift(30,b);
		b = a;
		a = (temp & 0xFFFFFFFF);
	}

	sha->mdigest[0] = (sha->mdigest[0] + a) & 0xFFFFFFFF;
	sha->mdigest[1] = (sha->mdigest[1] + b) & 0xFFFFFFFF;
	sha->mdigest[2] = (sha->mdigest[2] + c) & 0xFFFFFFFF;
	sha->mdigest[3] = (sha->mdigest[3] + d) & 0xFFFFFFFF;
	sha->mdigest[4] = (sha->mdigest[4] + e) & 0xFFFFFFFF;

	sha->mindex = 0;
}

void sha_padmessage(sha_t *sha)
{
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if (sha->mindex > 55)
	{
		sha->mblock[sha->mindex++] = 0x80;
		while (sha->mindex < 64)
		{
			sha->mblock[sha->mindex++] = 0;
		}
		sha_processblock(sha);
		while (sha->mindex < 56)
		{
			sha->mblock[sha->mindex++] = 0;
		}
	}
	else
	{
		sha->mblock[sha->mindex++] = 0x80;
		while (sha->mindex < 56)
		{
			sha->mblock[sha->mindex++] = 0;
		}
	}

	/* Store the message length as the last 8 octets */
	sha->mblock[56] = sha->mlength[7];
	sha->mblock[57] = sha->mlength[6];
	sha->mblock[58] = sha->mlength[5];
	sha->mblock[59] = sha->mlength[4];
	sha->mblock[60] = sha->mlength[3];
	sha->mblock[61] = sha->mlength[2];
	sha->mblock[62] = sha->mlength[1];
	sha->mblock[63] = sha->mlength[0];

	sha_processblock(sha);
}

int hdcpverify_dsa(const u8 * M, unsigned int n, const u8 * r, const u8 * s)
{
	int i = 0;
	sha_t sha;
	static const u8 q[] =
	{ 0xE7, 0x08, 0xC7, 0xF9, 0x4D, 0x3F, 0xEF, 0x97, 0xE2, 0x14, 0x6D, 0xCD,
			0x6A, 0xB5, 0x6D, 0x5E, 0xCE, 0xF2, 0x8A, 0xEE };
	static const u8 p[] =
	{ 0x27, 0x75, 0x28, 0xF3, 0x2B, 0x80, 0x59, 0x8C, 0x11, 0xC2, 0xED, 0x46,
			0x1C, 0x95, 0x39, 0x2A, 0x54, 0x19, 0x89, 0x96, 0xFD, 0x49, 0x8A,
			0x02, 0x3B, 0x73, 0x75, 0x32, 0x14, 0x9C, 0x7B, 0x5C, 0x49, 0x20,
			0x98, 0xB9, 0x07, 0x32, 0x3F, 0xA7, 0x30, 0x15, 0x72, 0xB3, 0x09,
			0x55, 0x71, 0x10, 0x3A, 0x4C, 0x97, 0xD1, 0xBC, 0xA0, 0x04, 0xF4,
			0x35, 0xCF, 0x47, 0x54, 0x0E, 0xA7, 0x2B, 0xE5, 0x83, 0xB9, 0xC6,
			0xD4, 0x47, 0xC7, 0x44, 0xB8, 0x67, 0x76, 0x7C, 0xAE, 0x0C, 0xDC,
			0x34, 0x4F, 0x4B, 0x9E, 0x96, 0x1D, 0x82, 0x84, 0xD2, 0xA0, 0xDC,
			0xE0, 0x00, 0xF5, 0x64, 0xA1, 0x7F, 0x8E, 0xFF, 0x58, 0x70, 0x6A,
			0xC3, 0x4F, 0xA2, 0xA1, 0xB8, 0xC7, 0x52, 0x5A, 0x35, 0x5B, 0x39,
			0x17, 0x6B, 0x78, 0x43, 0x93, 0xF7, 0x75, 0x8D, 0x01, 0xB7, 0x61,
			0x17, 0xFD, 0xB2, 0xF5, 0xC3, 0xD3 };
	static const u8 g[] =
	{ 0xD9, 0x0B, 0xBA, 0xC2, 0x42, 0x24, 0x46, 0x69, 0x5B, 0x40, 0x67, 0x2F,
			0x5B, 0x18, 0x3F, 0xB9, 0xE8, 0x6F, 0x21, 0x29, 0xAC, 0x7D, 0xFA,
			0x51, 0xC2, 0x9D, 0x4A, 0xAB, 0x8A, 0x9B, 0x8E, 0xC9, 0x42, 0x42,
			0xA5, 0x1D, 0xB2, 0x69, 0xAB, 0xC8, 0xE3, 0xA5, 0xC8, 0x81, 0xBE,
			0xB6, 0xA0, 0xB1, 0x7F, 0xBA, 0x21, 0x2C, 0x64, 0x35, 0xC8, 0xF7,
			0x5F, 0x58, 0x78, 0xF7, 0x45, 0x29, 0xDD, 0x92, 0x9E, 0x79, 0x3D,
			0xA0, 0x0C, 0xCD, 0x29, 0x0E, 0xA9, 0xE1, 0x37, 0xEB, 0xBF, 0xC6,
			0xED, 0x8E, 0xA8, 0xFF, 0x3E, 0xA8, 0x7D, 0x97, 0x62, 0x51, 0xD2,
			0xA9, 0xEC, 0xBD, 0x4A, 0xB1, 0x5D, 0x8F, 0x11, 0x86, 0x27, 0xCD,
			0x66, 0xD7, 0x56, 0x5D, 0x31, 0xD7, 0xBE, 0xA9, 0xAC, 0xDE, 0xAF,
			0x02, 0xB5, 0x1A, 0xDE, 0x45, 0x24, 0x3E, 0xE4, 0x1A, 0x13, 0x52,
			0x4D, 0x6A, 0x1B, 0x5D, 0xF8, 0x92 };
#ifndef FACSIMILE
	static const u8 y[] =
	{ 0x99, 0x37, 0xE5, 0x36, 0xFA, 0xF7, 0xA9, 0x62, 0x83, 0xFB, 0xB3, 0xE9,
			0xF7, 0x9D, 0x8F, 0xD8, 0xCB, 0x62, 0xF6, 0x66, 0x8D, 0xDC, 0xC8,
			0x95, 0x10, 0x24, 0x6C, 0x88, 0xBD, 0xFF, 0xB7, 0x7B, 0xE2, 0x06,
			0x52, 0xFD, 0xF7, 0x5F, 0x43, 0x62, 0xE6, 0x53, 0x65, 0xB1, 0x38,
			0x90, 0x25, 0x87, 0x8D, 0xA4, 0x9E, 0xFE, 0x56, 0x08, 0xA7, 0xA2,
			0x0D, 0x4E, 0xD8, 0x43, 0x3C, 0x97, 0xBA, 0x27, 0x6C, 0x56, 0xC4,
			0x17, 0xA4, 0xB2, 0x5C, 0x8D, 0xDB, 0x04, 0x17, 0x03, 0x4F, 0xE1,
			0x22, 0xDB, 0x74, 0x18, 0x54, 0x1B, 0xDE, 0x04, 0x68, 0xE1, 0xBD,
			0x0B, 0x4F, 0x65, 0x48, 0x0E, 0x95, 0x56, 0x8D, 0xA7, 0x5B, 0xF1,
			0x55, 0x47, 0x65, 0xE7, 0xA8, 0x54, 0x17, 0x8A, 0x65, 0x76, 0x0D,
			0x4F, 0x0D, 0xFF, 0xAC, 0xA3, 0xE0, 0xFB, 0x80, 0x3A, 0x86, 0xB0,
			0xA0, 0x6B, 0x52, 0x00, 0x06, 0xC7 };
#else
	static const u8 y[] =
	{
		0x46, 0xB9, 0xC2, 0xE5, 0xBE, 0x57, 0x3B, 0xA6,
		0x22, 0x7B, 0xAA, 0x83, 0x81, 0xA9, 0xD2, 0x0F,
		0x03, 0x2E, 0x0B, 0x70, 0xAC, 0x96, 0x42, 0x85,
		0x4E, 0x78, 0x8A, 0xDF, 0x65, 0x35, 0x97, 0x6D,
		0xE1, 0x8D, 0xD1, 0x7E, 0xA3, 0x83, 0xCA, 0x0F,
		0xB5, 0x8E, 0xA4, 0x11, 0xFA, 0x14, 0x6D, 0xB1,
		0x0A, 0xCC, 0x5D, 0xFF, 0xC0, 0x8C, 0xD8, 0xB1,
		0xE6, 0x95, 0x72, 0x2E, 0xBD, 0x7C, 0x85, 0xDE,
		0xE8, 0x52, 0x69, 0x92, 0xA0, 0x22, 0xF7, 0x01,
		0xCD, 0x79, 0xAF, 0x94, 0x83, 0x2E, 0x01, 0x1C,
		0xD7, 0xEF, 0x86, 0x97, 0xA3, 0xBB, 0xCB, 0x64,
		0xA6, 0xC7, 0x08, 0x5E, 0x8E, 0x5F, 0x11, 0x0B,
		0xC0, 0xE8, 0xD8, 0xDE, 0x47, 0x2E, 0x75, 0xC7,
		0xAA, 0x8C, 0xDC, 0xB7, 0x02, 0xC4, 0xDF, 0x95,
		0x31, 0x74, 0xB0, 0x3E, 0xEB, 0x95, 0xDB, 0xB0,
		0xCE, 0x11, 0x0E, 0x34, 0x9F, 0xE1, 0x13, 0x8D
	};
#endif
	u8 w[SIZE];
	u8 z[SIZE];
	u8 u1[SIZE];
	u8 u2[SIZE];
	u8 gu1[KSIZE];
	u8 yu2[KSIZE];
	u8 pro[KSIZE];
	u8 v[SIZE];

	/* adapt to the expected format by aritmetic functions */
	u8 r1[SIZE];
	u8 s1[SIZE];
	sha_reset(&sha);
	hdcpverify_arraycpy(r1, r, sizeof(r1));
	hdcpverify_arraycpy(s1, s, sizeof(s1));
	hdcpverify_arrayswp(r1, sizeof(r1));
	hdcpverify_arrayswp(s1, sizeof(s1));

	hdcpverify_computeinv(w, s1, q, sizeof(w));
	sha_input(&sha, M, n);
	if (sha_result(&sha) == TRUE)
	{
		for (i = 0; i < 5; i++)
		{
			z[i * 4 + 0] = sha.mdigest[i] >> 24;
			z[i * 4 + 1] = sha.mdigest[i] >> 16;
			z[i * 4 + 2] = sha.mdigest[i] >> 8;
			z[i * 4 + 3] = sha.mdigest[i] >> 0;
		}
		hdcpverify_arrayswp(z, sizeof(z));
	}
	else
	{
		LOG_ERROR("cannot digest message");
		return FALSE;
	}
	if (hdcpverify_computemul(u1, z, w, q, sizeof(u1)) == FALSE)
	{
		return FALSE;
	}
	if (hdcpverify_computemul(u2, r1, w, q, sizeof(u2)) == FALSE)
	{
		return FALSE;
	}
	if (hdcpverify_computeext(gu1, g, u1, p, sizeof(gu1), sizeof(u1)) == FALSE)
	{
		return FALSE;
	}
	if (hdcpverify_computeext(yu2, y, u2, p, sizeof(yu2), sizeof(u2)) == FALSE)
	{
		return FALSE;
	}
	if (hdcpverify_computemul(pro, gu1, yu2, p, sizeof(pro)) == FALSE)
	{
		return FALSE;
	}
	if (hdcpverify_computemod(v, pro, q, sizeof(v)) == FALSE)
	{
		return FALSE;
	}
	return (hdcpverify_arraycmp(v, r1, sizeof(v)) == 0);
}

int hdcpverify_arrayadd(u8 * r, const u8 * a, const u8 * b, unsigned int n)
{
	u8 c = 0;
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		u16 s = a[i] + b[i] + c;
		c = (u8) (s >> 8);
		r[i] = (u8) s;
	}
	return c;
}

int hdcpverify_arraycmp(const u8 * a, const u8 * b, unsigned int n)
{
	int i = 0;
	for (i = n; i > 0; i--)
	{
		if (a[i - 1] > b[i - 1])
		{
			return 1;
		}
		else if (a[i - 1] < b[i - 1])
		{
			return -1;
		}
	}
	return 0;
}

void hdcpverify_arraycpy(u8 * dst, const u8 * src, unsigned int n)
{
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		dst[i] = src[i];
	}
}

int hdcpverify_arraydiv(u8 * r, const u8 * D, const u8 * d, unsigned int n)
{
	int i = 0;
	if (r == D || r == d || !hdcpverify_arraytst(d, 0, n) == TRUE)
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	hdcpverify_arrayset(&r[n], 0, n);
	hdcpverify_arraycpy(r, D, n);
	for (i = n; i > 0; i--)
	{
		r[i - 1 + n] = 0;
		while (hdcpverify_arraycmp(&r[i - 1], d, n) >= 0)
		{
			hdcpverify_arraysub(&r[i - 1], &r[i - 1], d, n);
			r[i - 1 + n] += 1;
		}
	}
	return TRUE;
}

int hdcpverify_arraymac(u8 * r, const u8 * M, const u8 m, unsigned int n)
{
	u16 c = 0;
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		u16 p = (M[i] * m) + c + r[i];
		c = p >> 8;
		r[i] = (u8) p;
	}
	return (u8) c;
}

int hdcpverify_arraymul(u8 * r, const u8 * M, const u8 * m, unsigned int n)
{
	unsigned int i = 0;
	if (r == M || r == m)
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	hdcpverify_arrayset(r, 0, n);
	for (i = 0; i < n; i++)
	{
		if (m[i] == 0)
		{
			continue;
		}
		else if (m[i] == 1)
		{
			hdcpverify_arrayadd(&r[i], &r[i], M, n - i);
		}
		else
		{
			hdcpverify_arraymac(&r[i], M, m[i], n - i);
		}
	}
	return TRUE;
}

void hdcpverify_arrayset(u8 * dst, const u8 src, unsigned int n)
{
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		dst[i] = src;
	}
}

int hdcpverify_arraysub(u8 * r, const u8 * a, const u8 * b, unsigned int n)
{
	u8 c = 1;
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		u16 s = ((u8) a[i] + (u8) (~b[i])) + c;
		c = (u8) (s >> 8);
		r[i] = (u8) s;
	}
	return c;
}

void hdcpverify_arrayswp(u8 * r, unsigned int n)
{
	unsigned int i = 0;
	for (i = 0; i < (n / 2); i++)
	{
		u8 tmp = r[i];
		r[i] = r[n - 1 - i];
		r[n - 1 - i] = tmp;
	}
}

int hdcpverify_arraytst(const u8 * a, const u8 b, unsigned int n)
{
	unsigned int i = 0;
	for (i = 0; i < n; i++)
	{
		if (a[i] != b)
		{
			return FALSE;
		}
	}
	return TRUE;
}

int hdcpverify_computeext(u8 * c, const u8 * M, const u8 * e, const u8 * p,
		unsigned int n, unsigned int nE)
{
	int i = 8* nE - 1;
	int rc = TRUE;

	/* LR Binary Method */
	if ((e[i / 8] & (1 << (i % 8))) != 0)
	{
		hdcpverify_arraycpy(c, M, n);
	}
	else
	{
		hdcpverify_arrayset(c, 0, n);
		c[0] = 1;
	}
	for (i -= 1; i >= 0; i--)
	{
		rc |= hdcpverify_computemul(c, c, c, p, n);
		if ((e[i / 8] & (1 << (i % 8))) != 0)
		{
			rc &= hdcpverify_computemul(c, c, M, p, n);
		}
	}
	return rc;
}

int hdcpverify_computeinv(u8 * out, const u8 * z, const u8 * a, unsigned int n)
{
	u8 w[2][SIZE];
	u8 x[2][SIZE];
	u8 y[2][SIZE];
	u8 r[2*SIZE];
	u8 *i, *j, *q, *t;
	u8 *x1, *x2;
	u8 *y1, *y2;

	if ((n > SIZE) || (hdcpverify_arraytst(z, 0, n) == TRUE)
			|| (hdcpverify_arraytst(a, 0, n) == TRUE)
			|| (hdcpverify_arraycmp(z, a, n) >= 0))
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}

	hdcpverify_arraycpy(w[0], a, n);
	hdcpverify_arraycpy(w[1], z, n);
	i = w[0];
	j = w[1];

	hdcpverify_arrayset(x[1], 0, n);
	x[1][0] = 1;
	hdcpverify_arrayset(x[0], 0, n);
	x2 = x[1];
	x1 = x[0];

	hdcpverify_arrayset(y[1], 0, n);
	hdcpverify_arrayset(y[0], 0, n);
	y[0][0] = 1;
	y2 = y[1];
	y1 = y[0];

	do
	{
		hdcpverify_arraydiv(r, i, j, n);
		hdcpverify_arraycpy(i, r, n);
		q = &r[n];
		t = i; /* swap i <-> j */
		i = j;
		j = t;

		hdcpverify_arraymul(r, x1, q, n);
		hdcpverify_arraysub(x2, x2, r, n);
		t = x2; /* swap x1 <-> x2 */
		x2 = x1;
		x1 = t;

		hdcpverify_arraymul(r, y1, q, n);
		hdcpverify_arraysub(y2, y2, r, n);
		t = y2; /* swap y1 <-> y2 */
		y2 = y1;
		y1 = t;

	} while (hdcpverify_arraytst(j, 0, n) == FALSE);

	j[0] = 1;
	if (hdcpverify_arraycmp(i, j, n) != 0)
	{
		LOG_ERROR("i != 1");
		return FALSE;
	}
	hdcpverify_arraycpy(out, y2, n);
	return TRUE;
}

int hdcpverify_computemod(
	u8 * dst, const u8 * src, const u8 * p, unsigned int n)
{
	u8 aux[KSIZE];
	u8 ext[SIZE + 1];
	u8 tmp[2* (KSIZE +1)];
	int i = 0;

	if (n > SIZE)
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	hdcpverify_arraycpy(aux, src, sizeof(aux));
	/* TODO: remove extension */
	hdcpverify_arraycpy(ext, p, n);
	ext[n] = 0;
	for (i = sizeof(aux)-n-1; i >= 0; i--)
	{
		hdcpverify_arraydiv(tmp, &aux[i], ext, n+1);
		hdcpverify_arraycpy(&aux[i], tmp, n+1);
	}
	hdcpverify_arraycpy(dst, aux, n);
	return TRUE;
}

int hdcpverify_computemul(u8 * p, const u8 * a, const u8 * b, const u8 * m,
		unsigned int n)
{
	u8 aux[2*KSIZE + 1];
	u8 ext[KSIZE + 1];
	u8 tmp[2* (KSIZE +1)];
	unsigned int i = 0;
	int j = 0;
	if (n > KSIZE)
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	hdcpverify_arrayset(aux, 0, sizeof(aux));
	for (i = 0; i < n; i++)
	{
		/* TODO: extension was faster */
		aux[n+i] = hdcpverify_arraymac(&aux[i], a, b[i], n);
	}
	/* TODO: reuse ComputeMOD */
	hdcpverify_arraycpy(ext, m, n);
	ext[n] = 0;
	for (j = n; j >= 0; j--)
	{
		hdcpverify_arraydiv(tmp, &aux[j], ext, n+1);
		hdcpverify_arraycpy(&aux[j], tmp, n+1);
	}
	hdcpverify_arraycpy(p, aux, n);
	return TRUE;
}

int hdcpverify_ksv(const u8 * data, unsigned int size)
{
	unsigned int i = 0;
	sha_t sha;
	LOG_TRACE1(size);

	if (data == 0 || size < (header_g + shamax))
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	sha_reset(&sha);
	sha_input(&sha, data, size - shamax);
	if (sha_result(&sha) == FALSE)
	{
		LOG_ERROR("cannot process SHA digest");
		return FALSE;
	}
	for (i = 0; i < shamax; i++)
	{
		if (data[size - shamax + i] != (u8) (sha.mdigest[i / 4]
				>> ((i % 4) * 8)))
		{
			LOG_ERROR("SHA digest does not match");
			return FALSE;
		}
	}
	return TRUE;
}

int hdcpverify_srm(const u8 * data, unsigned int size)
{
	LOG_TRACE1(size);
	if (data == 0 || size < (vrl_header + vrl_number + 2* dsamax ))
	{
		LOG_ERROR("invalid input data");
		return FALSE;
	}
	/* M, n, r, s */
	return hdcpverify_dsa(data, size - 2* dsamax , &data[size - 2* dsamax ],
			&data[size - dsamax]);
}

