#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/random.h>
#include <linux/err.h>
#include <sdp/dek_common.h>
#include <sdp/dek_ioctl.h>
#include <sdp/dek_aes.h>


#define DEK_USER_ID_OFFSET	100

#define DEK_DEBUG		1
#define DEK_LOG_COUNT		100

#define PERSONA_KEY_ARR_IDX(personaId) (personaId-DEK_USER_ID_OFFSET)

/* Keys */
kek_t SDPK_sym[SDP_MAX_USERS];
kek_t SDPK_Rpub[SDP_MAX_USERS];
kek_t SDPK_Rpri[SDP_MAX_USERS];
kek_t SDPK_Dpub[SDP_MAX_USERS];
kek_t SDPK_Dpri[SDP_MAX_USERS];

/* Crypto tfms */
struct crypto_blkcipher *sdp_tfm[SDP_MAX_USERS];

/* Log buffer */
struct log_struct
{
	int len;
	char buf[256];
	struct list_head list;
	spinlock_t list_lock;
};
struct log_struct log_buffer;
static int log_count = 0;

/* Wait queue */
wait_queue_head_t wq;
static int flag = 0;

/* Log */
static void dek_add_to_log(int userid, char * buffer);


static int dek_open_evt(struct inode *inode, struct file *file)
{
	printk("dek: dek_open_evt\n");
	return 0;
}

static int dek_release_evt(struct inode *ignored, struct file *file)
{
	printk("dek: dek_release_evt\n");
	return 0;
}

static int dek_open_req(struct inode *inode, struct file *file)
{
	printk("dek: dek_open_req\n");
	return 0;
}

static int dek_release_req(struct inode *ignored, struct file *file)
{
	printk("dek: dek_release_req\n");
	return 0;
}

static int dek_open_kek(struct inode *inode, struct file *file)
{
	printk("dek: dek_open_kek\n");
	return 0;
}

static int dek_release_kek(struct inode *ignored, struct file *file)
{
	printk("dek: dek_release_kek\n");
	return 0;
}

#if DEK_DEBUG
static void dump(unsigned char *buf, int len) {
	int i;

	printk("len=%d: ", len);
	for(i=0;i<len;++i) {
		printk("%02x ", (unsigned char)buf[i]);
	}
	printk("\n");
}

static void dump_all_keys(int key_arr_idx) {
	printk("dek: SDPK_sym: ");
	dump(SDPK_sym[key_arr_idx].buf, SDPK_sym[key_arr_idx].len);

	printk("dek: SDPK_Rpub: ");
	dump(SDPK_Rpub[key_arr_idx].buf, SDPK_Rpub[key_arr_idx].len);
	printk("dek: SDPK_Rpri: ");
	dump(SDPK_Rpri[key_arr_idx].buf, SDPK_Rpri[key_arr_idx].len);

	printk("dek: SDPK_Dpub: ");
	dump(SDPK_Dpub[key_arr_idx].buf, SDPK_Dpub[key_arr_idx].len);
	printk("dek: SDPK_Dpri: ");
	dump(SDPK_Dpri[key_arr_idx].buf, SDPK_Dpri[key_arr_idx].len);
}
#endif

static int dek_is_persona(int userid) {
	if ((userid < 100) || (userid > 100+SDP_MAX_USERS-1)) {
		printk("dek: invalid persona id: %d\n", userid);
		dek_add_to_log(userid, "Invalid persona id");
		return 0;
	}
	return 1;
}

int dek_is_persona_locked(int userid) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(userid);
	if (dek_is_persona(userid)) {
		if (sdp_tfm[key_arr_idx] != NULL) {
			return 0;
		} else {
			return 1;
		}
	} else {
		return -1;
	}
}

int dek_generate_dek(int userid, dek_t *newDek) {
	if (!dek_is_persona(userid)) {
		return -EFAULT;
	}

	newDek->len = DEK_LEN;
	get_random_bytes(newDek->buf, newDek->len);

	if (newDek->len <= 0 || newDek->len > DEK_LEN) {
		memset(newDek, 0, sizeof(dek_t));
		return -EFAULT;
	}
#if DEK_DEBUG
	else {
		printk("dek: DEK: ");
		dump(newDek->buf, newDek->len);
	}
#endif

	return 0;
}

static int dek_encrypt_dek(int userid, dek_t *plainDek, dek_t *encDek) {
	int ret = 0;
	int key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	if (!dek_is_persona(userid)) {
		return -EFAULT;
	}
#if DEK_DEBUG
	printk("dek: plainDek from user: ");
	dump(plainDek->buf, plainDek->len);
#endif
	if (sdp_tfm[key_arr_idx] != NULL) {
		dek_aes_encrypt(sdp_tfm[key_arr_idx], plainDek->buf, encDek->buf, plainDek->len);
		encDek->len = plainDek->len;
		encDek->type = DEK_TYPE_AES_ENC;
	} else {
		printk("dek: no encryption key for id: %d\n", userid);
		dek_add_to_log(userid, "encrypt failed, no key");
		return -EIO;
	}

	if (encDek->len <= 0 || encDek->len > DEK_MAXLEN) {
		printk("dek: dek_encrypt_dek, incorrect len=%d\n", encDek->len);
		memset(encDek, 0, sizeof(dek_t));
		return -EFAULT;
	}
#if DEK_DEBUG
	else {
		printk("dek: encDek to user: ");
		dump(encDek->buf, encDek->len);
	}
#endif

	return ret;
}

int dek_encrypt_dek_efs(int userid, dek_t *plainDek, dek_t *encDek) {
	return dek_encrypt_dek(userid, plainDek, encDek);
}

static int dek_decrypt_dek(int userid, dek_t *encDek, dek_t *plainDek) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(userid);

	if (!dek_is_persona(userid)) {
		return -EFAULT;
	}
#if DEK_DEBUG
	printk("dek: encDek from user: ");
	dump(encDek->buf, encDek->len);
#endif
	if (encDek->type == DEK_TYPE_AES_ENC) {
		if (sdp_tfm[key_arr_idx] != NULL) {
			dek_aes_decrypt(sdp_tfm[key_arr_idx], encDek->buf, plainDek->buf, encDek->len);
			plainDek->len = encDek->len;
			plainDek->type = DEK_TYPE_PLAIN;
		} else {
			printk("dek: no SDPK_sym key for id: %d\n", userid);
			dek_add_to_log(userid, "decrypt failed, persona locked");
			return -EIO;
		}
	} else if (encDek->type == DEK_TYPE_RSA_ENC) {
		printk("dek: Not supported key type: %d\n", encDek->type);
		dek_add_to_log(userid, "decrypt failed, RSA type not supported");
		return -EFAULT;
	} else if (encDek->type == DEK_TYPE_DH_PUB) {
		printk("dek: Not supported key type: %d\n", encDek->type);
		dek_add_to_log(userid, "decrypt failed, DH type not supported");
		return -EFAULT;
	} else {
		printk("dek: Unsupported decrypt key type: %d\n", encDek->type);
		dek_add_to_log(userid, "decrypt failed, unsupported key type");
		return -EFAULT;
	}

	if (plainDek->len <= 0 || plainDek->len > DEK_LEN) {
		printk("dek: dek_decrypt_dek, incorrect len=%d\n", plainDek->len);
		memset(plainDek, 0, sizeof(dek_t));
		return -EFAULT;
	} else {
#if DEK_DEBUG
		printk("dek: plainDek to user: ");
		dump(plainDek->buf, plainDek->len);
#endif
	}
	return 0;
}

int dek_decrypt_dek_efs(int userid, dek_t *encDek, dek_t *plainDek) {
	return dek_decrypt_dek(userid, encDek, plainDek);
}


static int dek_on_boot(dek_arg_on_boot *evt) {
	int ret = 0;
	int key_arr_idx = PERSONA_KEY_ARR_IDX(evt->userid);

	memcpy(SDPK_Rpub[key_arr_idx].buf, evt->SDPK_Rpub.buf, evt->SDPK_Rpub.len);
	SDPK_Rpub[key_arr_idx].len = evt->SDPK_Rpub.len;
	memcpy(SDPK_Dpub[key_arr_idx].buf, evt->SDPK_Dpub.buf, evt->SDPK_Dpub.len);
	SDPK_Dpub[key_arr_idx].len = evt->SDPK_Dpub.len;

#if DEK_DEBUG
	dump_all_keys(key_arr_idx);
#endif

	return ret;
}

static int dek_on_device_locked(dek_arg_on_device_locked *evt) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(evt->userid);

	dek_aes_key_free(sdp_tfm[key_arr_idx]);
	sdp_tfm[key_arr_idx] = NULL;

	memset(&SDPK_sym[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Rpri[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Dpri[key_arr_idx], 0, sizeof(kek_t));

#if DEK_DEBUG
	dump_all_keys(key_arr_idx);
#endif

	return 0;
}

static int dek_on_device_unlocked(dek_arg_on_device_unlocked *evt) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(evt->userid);

	/*
	 * TODO : lock needed
	 */

	memcpy(SDPK_sym[key_arr_idx].buf, evt->SDPK_sym.buf, evt->SDPK_sym.len);
	SDPK_sym[key_arr_idx].len = evt->SDPK_sym.len;
	memcpy(SDPK_Rpri[key_arr_idx].buf, evt->SDPK_Rpri.buf, evt->SDPK_Rpri.len);
	SDPK_Rpri[key_arr_idx].len = evt->SDPK_Rpri.len;
	memcpy(SDPK_Dpri[key_arr_idx].buf, evt->SDPK_Dpri.buf, evt->SDPK_Dpri.len);
	SDPK_Dpri[key_arr_idx].len = evt->SDPK_Dpri.len;

	sdp_tfm[key_arr_idx] = dek_aes_key_setup(evt->SDPK_sym.buf, evt->SDPK_sym.len);
	if (IS_ERR(sdp_tfm[key_arr_idx])) {
		printk("dek: error setting up key\n");
		dek_add_to_log(evt->userid, "error setting up key");
		sdp_tfm[key_arr_idx] = NULL;
	}

#if DEK_DEBUG
	dump_all_keys(key_arr_idx);
#endif

	return 0;
}

static int dek_on_user_added(dek_arg_on_user_added *evt) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(evt->userid);

	memcpy(SDPK_Rpub[key_arr_idx].buf, evt->SDPK_Rpub.buf, evt->SDPK_Rpub.len);
	SDPK_Rpub[key_arr_idx].len = evt->SDPK_Rpub.len;
	memcpy(SDPK_Dpub[key_arr_idx].buf, evt->SDPK_Dpub.buf, evt->SDPK_Dpub.len);
	SDPK_Dpub[key_arr_idx].len = evt->SDPK_Dpub.len;

#if DEK_DEBUG
	dump_all_keys(key_arr_idx);
#endif
	return 0;
}

static int dek_on_user_removed(dek_arg_on_user_removed *evt) {
	int key_arr_idx = PERSONA_KEY_ARR_IDX(evt->userid);

	memset(&SDPK_sym[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Rpub[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Rpri[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Dpub[key_arr_idx], 0, sizeof(kek_t));
	memset(&SDPK_Dpri[key_arr_idx], 0, sizeof(kek_t));

#if DEK_DEBUG
	dump_all_keys(key_arr_idx);
#endif

	dek_aes_key_free(sdp_tfm[key_arr_idx]);
	sdp_tfm[key_arr_idx] = NULL;

	return 0;
}

// I'm thinking... if minor id can represent persona id

static long dek_do_ioctl_evt(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;
	void *cleanup = NULL;
	unsigned int size;

	switch (cmd) {
	/*
	 * Event while booting.
	 *
	 * This event comes per persona, the driver is responsible to
	 * verify things good whether it's compromised.
	 */
	case DEK_ON_BOOT: {
		dek_arg_on_boot *evt = kzalloc(sizeof(dek_arg_on_boot), GFP_KERNEL);
		
		printk("dek: DEK_ON_BOOT\n");

		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_boot);

		if(copy_from_user(evt, ubuf, size)) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_boot(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Boot failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Booted");
		break;
	}
	/*
	 * Event when device is locked.
	 *
	 * Nullify private key which prevents decryption.
	 */
	case DEK_ON_DEVICE_LOCKED: {
		dek_arg_on_device_locked *evt = kzalloc(sizeof(dek_arg_on_device_locked), GFP_KERNEL);
		
		printk("dek: DEK_ON_DEVICE_LOCKED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_device_locked);

		if(copy_from_user(evt, ubuf, size)) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_device_locked(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Lock failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Locked");
		break;
	}
	/*
	 * Event when device unlocked.
	 *
	 * Read private key and decrypt with user password.
	 */
	case DEK_ON_DEVICE_UNLOCKED: {
		dek_arg_on_device_unlocked *evt = kzalloc(sizeof(dek_arg_on_device_unlocked), GFP_KERNEL);

		printk("dek: DEK_ON_DEVICE_UNLOCKED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_device_unlocked);

		if(copy_from_user(evt, ubuf, size)) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_device_unlocked(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Unlock failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Unlocked");
		break;
	}
	/*
	 * Event when new user(persona) added.
	 *
	 * Generate RSA public key and encrypt private key with given
	 * password. Also pub-key and encryped priv-key have to be stored
	 * in a file system.
	 */
	case DEK_ON_USER_ADDED: {
		dek_arg_on_user_added *evt = kzalloc(sizeof(dek_arg_on_user_added), GFP_KERNEL);

		printk("dek: DEK_ON_USER_ADDED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_user_added);

		if(copy_from_user(evt, ubuf, size)) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_user_added(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Add user failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Added user");
		break;
	}
	/*
	 * Event when user is removed.
	 *
	 * Remove pub-key file & encrypted priv-key file.
	 */
	case DEK_ON_USER_REMOVED: {
		dek_arg_on_user_removed *evt = kzalloc(sizeof(dek_arg_on_user_removed), GFP_KERNEL);
		
		printk("dek: DEK_ON_USER_REMOVED\n");
		if (evt == NULL) {
			ret = -ENOMEM;
			goto err;
		}		
		cleanup = evt;
		size = sizeof(dek_arg_on_user_removed);

		if(copy_from_user(evt, ubuf, size)) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		if (!dek_is_persona(evt->userid)) {
			ret = -EFAULT;
			goto err;
		}
		ret = dek_on_user_removed(evt);
		if (ret < 0) {
			dek_add_to_log(evt->userid, "Remove user failed");
			goto err;
		}
		dek_add_to_log(evt->userid, "Removed user");
		break;
	}
	/*
	 * Event when password changed.
	 *
	 * Encrypt SDPK_Rpri with new password and store it.
	 */
	case DEK_ON_CHANGE_PASSWORD: {
		printk("dek: DEK_ON_CHANGE_PASSWORD << deprecated. SKIP\n");
		ret = 0;
		dek_add_to_log(0, "Changed password << deprecated");
		break;
	}
	default:
		printk("dek: %s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

err:
	if (cleanup) {
		printk("dek: cleanup size: %d\n", size);
		memset(cleanup, 0, size);
		kfree(cleanup);
	}
	return ret;
}

static long dek_do_ioctl_req(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;

	switch (cmd) {
	/*
	 * Request to generate DEK.
	 * Generate DEK and return to the user
	 */
	case DEK_GENERATE_DEK: {
		dek_arg_generate_dek req;

		printk("dek: DEK_GENERATE_DEK\n");

		memset(&req, 0, sizeof(dek_arg_generate_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		dek_generate_dek(req.userid, &req.dek);
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_generate_dek));
			ret = -EFAULT;
			goto err;
		}
		memset(&req, 0, sizeof(dek_arg_generate_dek));
		break;
	}
	/*
	 * Request to encrypt given DEK.
	 *
	 * encrypt dek and return to the user
	 */
	case DEK_ENCRYPT_DEK: {
		dek_arg_encrypt_dek req;

		printk("dek: DEK_ENCRYPT_DEK\n");

		memset(&req, 0, sizeof(dek_arg_encrypt_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_encrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		ret = dek_encrypt_dek(req.userid,
				&req.plain_dek, &req.enc_dek);
		if (ret < 0) {
			memset(&req, 0, sizeof(dek_arg_encrypt_dek));
			goto err;
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_encrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		memset(&req, 0, sizeof(dek_arg_encrypt_dek));
		break;
	}
	/*
	 * Request to decrypt given DEK.
	 *
	 * Decrypt dek and return to the user.
	 * When device is locked, private key is not available, so
	 * the driver must return EPERM or some kind of error.
	 */
	case DEK_DECRYPT_DEK: {
		dek_arg_decrypt_dek req;

		printk("dek: DEK_DECRYPT_DEK\n");

		memset(&req, 0, sizeof(dek_arg_decrypt_dek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_decrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		ret = dek_decrypt_dek(req.userid,
				&req.enc_dek, &req.plain_dek);
		if (ret < 0) {
			memset(&req, 0, sizeof(dek_arg_decrypt_dek));
			goto err;
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_decrypt_dek));
			ret = -EFAULT;
			goto err;
		}
		memset(&req, 0, sizeof(dek_arg_decrypt_dek));
		break;
	}

	default:
		printk("dek: %s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
err:
	return ret;
}

static long dek_do_ioctl_kek(unsigned int minor, unsigned int cmd,
		unsigned long arg) {
	long ret = 0;
	void __user *ubuf = (void __user *)arg;
	int key_arr_idx = 0;

	switch (cmd) {
	case DEK_GET_KEK: {
		dek_arg_get_kek req;
		int requested_type = 0;

		printk("dek: DEK_GET_KEK\n");

		memset(&req, 0, sizeof(dek_arg_get_kek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			ret = -EFAULT;
			goto err;
		}
		key_arr_idx = PERSONA_KEY_ARR_IDX(req.userid);
		requested_type = req.kek_type;
		req.key.len = 0;
		req.key.type = -1;

		switch(requested_type) {
		case KEK_TYPE_SYM:
			if (SDPK_sym[key_arr_idx].len > 0) {
				memcpy(req.key.buf, SDPK_sym[key_arr_idx].buf, SDPK_sym[key_arr_idx].len);
				req.key.len = SDPK_sym[key_arr_idx].len;
				req.key.type = KEK_TYPE_SYM;
				printk("dek: SDPK_sym len : %d\n", req.key.len);
			}else{
				printk("dek: SDPK_sym not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_RSA_PUB:
			if (SDPK_Rpub[key_arr_idx].len > 0) {
				memcpy(req.key.buf, SDPK_Rpub[key_arr_idx].buf, SDPK_Rpub[key_arr_idx].len);
				req.key.len = SDPK_Rpub[key_arr_idx].len;
				req.key.type = KEK_TYPE_RSA_PUB;
				printk("dek: SDPK_Rpub len : %d\n", req.key.len);
			} else {
				printk("dek: SDPK_Rpub not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_RSA_PRIV:
			if (SDPK_Rpri[key_arr_idx].len > 0) {
				memcpy(req.key.buf, SDPK_Rpri[key_arr_idx].buf, SDPK_Rpri[key_arr_idx].len);
				req.key.len = SDPK_Rpri[key_arr_idx].len;
				req.key.type = KEK_TYPE_RSA_PRIV;
				printk("dek: SDPK_Rpri len : %d\n", req.key.len);
			} else {
				printk("dek: SDPK_Rpri not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_DH_PUB:
			if (SDPK_Dpub[key_arr_idx].len > 0) {
				memcpy(req.key.buf, SDPK_Dpub[key_arr_idx].buf, SDPK_Dpub[key_arr_idx].len);
				req.key.len = SDPK_Dpub[key_arr_idx].len;
				req.key.type = KEK_TYPE_DH_PUB;
				printk("dek: SDPK_Dpub len : %d\n", req.key.len);
			} else {
				printk("dek: SDPK_Dpub not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		case KEK_TYPE_DH_PRIV:
			if (SDPK_Dpri[key_arr_idx].len > 0) {
				memcpy(req.key.buf, SDPK_Dpri[key_arr_idx].buf, SDPK_Dpri[key_arr_idx].len);
				req.key.len = SDPK_Dpri[key_arr_idx].len;
				req.key.type = KEK_TYPE_DH_PRIV;
				printk("dek: SDPK_Dpri len : %d\n", req.key.len);
			} else {
				printk("dek: SDPK_Dpri not-available\n");
				ret = -EIO;
				goto err;
			}
			break;
		default:
			printk("dek: invalid key type\n");
			ret = -EINVAL;
			goto err;
			break;
		}

		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_get_kek));
			ret = -EFAULT;
			goto err;
		}
		memset(&req, 0, sizeof(dek_arg_get_kek));
		break;
	}
	default:
		printk("dek: %s case default\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
err:
	return ret;
}


static long dek_ioctl_evt(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	printk("dek: dek_ioctl_evt\n");

	minor = iminor(file->f_path.dentry->d_inode);

	return dek_do_ioctl_evt(minor, cmd, arg);
}

static long dek_ioctl_req(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	printk("dek: dek_ioctl_req\n");

	minor = iminor(file->f_path.dentry->d_inode);

	return dek_do_ioctl_req(minor, cmd, arg);
}

static long dek_ioctl_kek(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	unsigned int minor;
	printk("dek: dek_ioctl_kek\n");

	minor = iminor(file->f_path.dentry->d_inode);

	return dek_do_ioctl_kek(minor, cmd, arg);
}

/*
 * DAR engine log
 */

static int dek_open_log(struct inode *inode, struct file *file)
{
	printk("dek: dek_open_log\n");
	return 0;
}

static int dek_release_log(struct inode *ignored, struct file *file)
{
	printk("dek: dek_release_log\n");
	return 0;
}

static ssize_t dek_read_log(struct file *file, char * buffer, size_t len, loff_t *off)
{
	struct log_struct *tmp = NULL;

	printk("dek: dek_read_log, len=%d, off=%ld, buffer_count=%d\n",
			len, (long int)*off, log_count);

	if (log_count == 0) {
		printk("dek: process %i (%s) going to sleep\n",
				current->pid, current->comm);
		wait_event_interruptible(wq, flag != 0);
	}
	flag = 0;

	if (!list_empty(&log_buffer.list)) {
		spin_lock(&log_buffer.list_lock);
		tmp = list_first_entry(&log_buffer.list, struct log_struct, list);
		if (tmp != NULL) {
			if(copy_to_user(buffer, tmp->buf, tmp->len)) {
				printk("dek: dek_read_log, copy_to_user fail\n");
				spin_unlock(&log_buffer.list_lock);
				return -EFAULT;
			}
			len = tmp->len;
			*off = len;

			list_del(&tmp->list);
			kfree(tmp);
			log_count--;
		}
		spin_unlock(&log_buffer.list_lock);
	} else {
		printk("dek: dek_read_log, list empty\n");
		len = 0;
	}

	return len;
}

static void dek_add_to_log(int userid, char * buffer) {
	struct timespec ts;
	struct log_struct *tmp = (struct log_struct*)kmalloc(sizeof(struct log_struct), GFP_KERNEL);

	if (tmp) {
		INIT_LIST_HEAD(&tmp->list);

		getnstimeofday(&ts);
		tmp->len = sprintf(tmp->buf, "%ld.%.3ld|%d|%s|%d|%s\n",
				(long)ts.tv_sec,
				(long)ts.tv_nsec / 1000000,
				current->pid,
				current->comm,
				userid,
				buffer);

		spin_lock(&log_buffer.list_lock);
		list_add_tail(&(tmp->list), &(log_buffer.list));
		spin_unlock(&log_buffer.list_lock);
		log_count++;

		if (log_count > DEK_LOG_COUNT) {
			printk("dek: dek_add_to_log - exceeded DEK_LOG_COUNT\n");
			spin_lock(&log_buffer.list_lock);
			tmp = list_first_entry(&log_buffer.list, struct log_struct, list);
			list_del(&tmp->list);
			spin_unlock(&log_buffer.list_lock);
			kfree(tmp);
			log_count--;
		}

		printk("dek: process %i (%s) awakening the readers, log_count=%d\n",
				current->pid, current->comm, log_count);
		flag = 1;
		wake_up_interruptible(&wq);
	} else {
		printk("dek: dek_add_to_log - failed to allocate buffer\n");
	}
}

const struct file_operations dek_fops_evt = {
		.owner = THIS_MODULE,
		.open = dek_open_evt,
		.release = dek_release_evt,
		.unlocked_ioctl = dek_ioctl_evt,
		.compat_ioctl = dek_ioctl_evt,
};

static struct miscdevice dek_misc_evt = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_evt",
		.fops = &dek_fops_evt,
};

const struct file_operations dek_fops_req = {
		.owner = THIS_MODULE,
		.open = dek_open_req,
		.release = dek_release_req,
		.unlocked_ioctl = dek_ioctl_req,
		.compat_ioctl = dek_ioctl_req,
};

static struct miscdevice dek_misc_req = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_req",
		.fops = &dek_fops_req,
};

const struct file_operations dek_fops_log = {
		.owner = THIS_MODULE,
		.open = dek_open_log,
		.release = dek_release_log,
		.read = dek_read_log,
};

static struct miscdevice dek_misc_log = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_log",
		.fops = &dek_fops_log,
};


const struct file_operations dek_fops_kek = {
		.owner = THIS_MODULE,
		.open = dek_open_kek,
		.release = dek_release_kek,
		.unlocked_ioctl = dek_ioctl_kek,
		.compat_ioctl = dek_ioctl_kek,
};

static struct miscdevice dek_misc_kek = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "dek_kek",
		.fops = &dek_fops_kek,
};


static int __init dek_init(void) {
	int ret;
	int i;

	ret = misc_register(&dek_misc_evt);
	if (unlikely(ret)) {
		printk("dek: failed to register misc_evt device!\n");
		return ret;
	}
	ret = misc_register(&dek_misc_req);
	if (unlikely(ret)) {
		printk("dek: failed to register misc_req device!\n");
		return ret;
	}

	ret = misc_register(&dek_misc_log);
	if (unlikely(ret)) {
		printk("dek: failed to register misc_log device!\n");
		return ret;
	}

	ret = misc_register(&dek_misc_kek);
	if (unlikely(ret)) {
		printk("dek: failed to register misc_kek device!\n");
		return ret;
	}

	for(i = 0; i < SDP_MAX_USERS; i++){
		memset(&SDPK_sym[i], 0, sizeof(kek_t));
		memset(&SDPK_Rpub[i], 0, sizeof(kek_t));
		memset(&SDPK_Rpri[i], 0, sizeof(kek_t));
		memset(&SDPK_Dpub[i], 0, sizeof(kek_t));
		memset(&SDPK_Rpri[i], 0, sizeof(kek_t));
		sdp_tfm[i] = NULL;
	}

	INIT_LIST_HEAD(&log_buffer.list);
	spin_lock_init(&log_buffer.list_lock);
	init_waitqueue_head(&wq);

	printk("dek: initialized\n");
	dek_add_to_log(000, "Initialized");

	return 0;
}

static void __exit dek_exit(void)
{
	printk("dek: unloaded\n");
}

module_init(dek_init)
module_exit(dek_exit)

MODULE_LICENSE("GPL");
