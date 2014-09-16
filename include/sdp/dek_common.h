#ifndef DEK_COMMON_H__
#define DEK_COMMON_H__

#include <linux/limits.h>
#include <linux/ioctl.h>

#define SDPK_DEFAULT_ALGOTYPE (SDPK_ALGOTYPE_ASYMM_DH)
#define SDPK_ALGOTYPE_ASYMM_RSA 0
#define SDPK_ALGOTYPE_ASYMM_DH	1

//#define DEK_ENGINE_LOCAL_KEK
#define KEK_RSA_KEY_BITS        2048
#define KEK_MK_BITS             256
#define DH_DEFAULT_GENERATOR	DH_GENERATOR_2
#define DH_MAXLEN				280
#define DEK_NAME_LEN            256
#define DEK_LEN             	32
#define DEK_MAXLEN          	400 // TODO : need to optimize the length of EDEK DEK_RSA_KEY_BITS/8 : 256 bytes , DH2236 : 280 bytes
#define DEK_PW_LEN              32
#define KEK_MAXLEN         		(KEK_RSA_KEY_BITS/4+4)
#define KEK_MK_LEN              (KEK_MK_BITS/8)
#define DEK_AES_HEADER          44
#define FEK_MAXLEN				256

#define AES_BLOCK_SIZE 			16

#define KNOX_PERSONA_BASE_ID 100
#define BASE_ID KNOX_PERSONA_BASE_ID
#define GET_ARR_IDX(__userid) (__userid - BASE_ID)
#define KEK_MAX_LEN KEK_MAXLEN
#define SDP_MAX_USERS 5

// DEK types
#define DEK_TYPE_PLAIN 		0
#define DEK_TYPE_RSA_ENC 	1
#define DEK_TYPE_AES_ENC 	2
#define DEK_TYPE_DH_PUB 	4
#define DEK_TYPE_SS_ENC		5

// KEK types
#define KEK_TYPE_SYM 		10
#define KEK_TYPE_RSA_PUB 	11
#define KEK_TYPE_RSA_PRIV 	12
#define KEK_TYPE_DH_PUB 	13
#define KEK_TYPE_DH_PRIV 	14
#define KEK_TYPE_DH_SESS 	15

#define SDPK_PATH_MAX	256
#define SDPK_PATH_FMT    "/data/system/users/%d/SDPK_%s"
#define SDPK_DPRI_NAME   "Dpri"
#define SDPK_DPUB_NAME   "Dpub"
#define SDPK_RPRI_NAME   "Rpri"
#define SDPK_RPUB_NAME   "Rpub"
#define SDPK_SYM_NAME     "sym"

typedef struct _password{
	unsigned int len;
	unsigned char buf[DEK_MAXLEN];
}password_t;

typedef struct _key{
	unsigned int type;
	unsigned int len;
	unsigned char buf[DEK_MAXLEN];
}dek_t;

typedef struct _kek{
	unsigned int type;
	unsigned int len;
	unsigned char buf[KEK_MAX_LEN];
}kek_t;

typedef struct _payload{
	unsigned int efek_len;
	unsigned int dh_len;
	unsigned char efek_buf[FEK_MAXLEN];
	unsigned char dh_buf[KEK_MAX_LEN];
}dh_payload;
#endif
