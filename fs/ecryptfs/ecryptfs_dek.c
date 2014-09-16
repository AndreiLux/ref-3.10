#include <linux/crypto.h>
#include <asm/unaligned.h>
#include "ecryptfs_dek.h"

extern int dek_encrypt_dek_efs(int sdp_id, dek_t *plainDek, dek_t *encDek);
extern int dek_decrypt_dek_efs(int sdp_id, dek_t *encDek, dek_t *plainDek);
extern int dek_is_persona_locked(int sdp_id);

static int ecryptfs_update_crypt_flag(struct dentry *dentry, int is_sensitive);

#if ECRYPTFS_DEK_DEBUG
static void ecryptfs_dumpkey(int sdp_id, char *tag, unsigned char *buf, int len)
{
    int     i;
    char	s[512];

    s[0] = 0;
    for(i=0;i<len && i<16;++i) {
        char tmp[8];
        sprintf(tmp, " %02x", buf[i]);
        strcat(s, tmp);
    }

    if (len > 16) {
        char tmp[8];
        sprintf(tmp, " ...");
        strcat(s, tmp);
    }

    ecryptfs_printk(KERN_INFO, "id:%d, %s [%s len=%d]\n", sdp_id, tag, s, len);
}
#endif

int ecryptfs_super_block_get_sdp_id(struct super_block *sb)
{
	int sdp_id = ecryptfs_superblock_to_private(sb)->sdp_id;
#if ECRYPTFS_DEK_DEBUG
	ecryptfs_printk(KERN_INFO, "sdp id is %d\n", sdp_id);
#endif
	return sdp_id;
}

int ecryptfs_is_valid_sdpid(int sdp_id)
{
	if(sdp_id >= 100) { //persona id starts from 100
		return 1;
	}
	return 0;
}

int ecryptfs_is_persona_locked(int sdp_id)
{
	return dek_is_persona_locked(sdp_id);
}

int ecryptfs_get_sdp_dek(unsigned char *sig, int *sig_len, struct ecryptfs_crypt_stat *crypt_stat) 
{
	int rc = 0;
	if(crypt_stat != NULL && (crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED)) {
		if((crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE)) {
			dek_t DEK;

#if ECRYPTFS_DEK_DEBUG
			ecryptfs_printk(KERN_INFO, "get_sdp_dek: sensitive, dek type: %d\n", crypt_stat->sdp_dek.type);
			ecryptfs_dumpkey(crypt_stat->sdp_id, "sdp_dek:", crypt_stat->sdp_dek.buf, crypt_stat->sdp_dek.len);
#endif
			if (crypt_stat->sdp_dek.type != DEK_TYPE_PLAIN) {
				rc = dek_decrypt_dek_efs(crypt_stat->sdp_id, &crypt_stat->sdp_dek, &DEK);
			} else {
				ecryptfs_printk(KERN_ERR, "Error, DEK already plaintext");
				rc = -1;
			}
			if (rc < 0) {
				ecryptfs_printk(KERN_ERR, "Error decypting dek; rc = [%d]\n", rc);
				rc = -1;
				/*
				 * TODO : olic.moon
				 * When we return -1 here, ECRYPTFS_ENCRYPTED is somehow gone
				 * later. then it occurs error while updating EDEK
				 */
				rc = 0;
				memset(&DEK, 0, sizeof(dek_t));
				goto out;
			}
#if ECRYPTFS_DEK_DEBUG
			ecryptfs_dumpkey(crypt_stat->sdp_id, "decrypted:", DEK.buf, DEK.len);
			ecryptfs_printk(KERN_INFO, "decrypted key size is %d\n", DEK.len);
#endif
			memcpy(sig, DEK.buf, DEK.len);
			(*sig_len) = DEK.len;
			memset(&DEK, 0, sizeof(dek_t));
		} else {
#if ECRYPTFS_DEK_DEBUG
			ecryptfs_printk(KERN_INFO, "file is not sensitive\n");
#endif
		}
	}
out:
	return rc;
}

int write_dek_packet(char *dest,
		struct ecryptfs_crypt_stat *crypt_stat,
		size_t *written) {
	*written = 0;
	dest[(*written)++] = ECRYPTFS_DEK_PACKET_TYPE;

	memset(dest + *written, 0, PKG_NAME_SIZE);
	memcpy(dest + *written, current->comm, PKG_NAME_SIZE);
	(*written) += PKG_NAME_SIZE;

	put_unaligned_be32(current_euid(), dest + *written);
	(*written) += 4;

	memset(dest + *written, 0, DEK_MAXLEN);
	if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
		put_unaligned_be32(crypt_stat->sdp_dek.type, dest + *written);
		(*written) += 4;
		put_unaligned_be32(crypt_stat->sdp_dek.len, dest + *written);
		(*written) += 4;
		memcpy(dest + *written, crypt_stat->sdp_dek.buf, crypt_stat->sdp_dek.len);
		(*written) += crypt_stat->sdp_dek.len;
	}

	return 0;
}

int parse_dek_packet(char *data,
		struct ecryptfs_crypt_stat *crypt_stat,
		size_t *packet_size) {
	int rc = 0;
	char temp_comm[PKG_NAME_SIZE]; //test
	int temp_euid;

	(*packet_size) = 0;

	if (data[(*packet_size)++] != ECRYPTFS_DEK_PACKET_TYPE) {
		printk(KERN_ERR "First byte != 0x%.2x; invalid packet\n",
				ECRYPTFS_DEK_PACKET_TYPE);
		rc = -EINVAL;
	}

	memcpy(temp_comm, &data[*packet_size], PKG_NAME_SIZE);
	(*packet_size) += PKG_NAME_SIZE;

	temp_euid = get_unaligned_be32(data + *packet_size);
	(*packet_size) += 4;

	if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
		crypt_stat->sdp_dek.type = get_unaligned_be32(data + *packet_size);
		(*packet_size) += 4;
		crypt_stat->sdp_dek.len = get_unaligned_be32(data + *packet_size);
		(*packet_size) += 4;
		memcpy(crypt_stat->sdp_dek.buf, &data[*packet_size], crypt_stat->sdp_dek.len);
		(*packet_size) += crypt_stat->sdp_dek.len;
	}
	
#if ECRYPTFS_DEK_DEBUG
	printk("%s() : comm : %s [euid:%d]\n",
		__func__, temp_comm, temp_euid);
#endif
	return rc;
}

static int ecryptfs_update_crypt_flag(struct dentry *dentry, int is_sensitive)
{
	int rc = 0;
	struct dentry *lower_dentry;
	struct inode *inode;
	struct inode *lower_inode;
	struct ecryptfs_crypt_stat *crypt_stat;
	struct ecryptfs_mount_crypt_stat *mount_crypt_stat;

	crypt_stat = &ecryptfs_inode_to_private(dentry->d_inode)->crypt_stat;
	if (!(crypt_stat->flags & ECRYPTFS_STRUCT_INITIALIZED))
		ecryptfs_init_crypt_stat(crypt_stat);
	inode = dentry->d_inode;
	lower_inode = ecryptfs_inode_to_lower(inode);
	lower_dentry = ecryptfs_dentry_to_lower(dentry);

	mutex_lock(&crypt_stat->cs_mutex);

	mount_crypt_stat = &ecryptfs_superblock_to_private(
			dentry->d_sb)->mount_crypt_stat;
	rc = ecryptfs_get_lower_file(dentry, inode);
	if (rc) {
		mutex_unlock(&crypt_stat->cs_mutex);
		ecryptfs_printk(KERN_ERR, "ecryptfs_get_lower_file rc=%d\n", rc);
		goto out;
	}

	if (is_sensitive) {
		crypt_stat->flags |= ECRYPTFS_DEK_IS_SENSITIVE;
	}
	else{
		crypt_stat->flags &= ~ECRYPTFS_DEK_IS_SENSITIVE;
	}

	rc = ecryptfs_write_metadata(dentry, inode);
	if (rc) {
		if (!(mount_crypt_stat->flags
				& ECRYPTFS_PLAINTEXT_PASSTHROUGH_ENABLED)) {
			rc = -EIO;
			printk(KERN_WARNING "Either the lower file "
					"is not in a valid eCryptfs format, "
					"or the key could not be retrieved. "
					"Plaintext passthrough mode is not "
					"enabled; returning -EIO\n");
			mutex_unlock(&crypt_stat->cs_mutex);
			ecryptfs_printk(KERN_INFO, "ecryptfs_write_metadata rc=%d\n", rc);
			goto out;
		}
		rc = 0;
		crypt_stat->flags &= ~(ECRYPTFS_I_SIZE_INITIALIZED
				| ECRYPTFS_ENCRYPTED);
	}

	rc = ecryptfs_write_inode_size_to_metadata(inode);
	if (rc) {
		mutex_unlock(&crypt_stat->cs_mutex);
		printk(KERN_ERR "Problem with "
				"ecryptfs_write_inode_size_to_metadata; "
				"rc = [%d]\n", rc);
		goto out;
	}

	ecryptfs_put_lower_file(inode);

	mutex_unlock(&crypt_stat->cs_mutex);

out:
	fsstack_copy_attr_all(inode, lower_inode);
	return rc;
}

long ecryptfs_do_sdp_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	char filename[NAME_MAX+1] = {0};
	void __user *ubuf = (void __user *)arg;
	struct dentry *ecryptfs_dentry = file->f_path.dentry;
	struct inode *inode = ecryptfs_dentry->d_inode;
	struct ecryptfs_crypt_stat *crypt_stat =
			&ecryptfs_inode_to_private(inode)->crypt_stat;
	struct dentry *fp_dentry =
			ecryptfs_inode_to_private(inode)->lower_file->f_dentry;
	if (fp_dentry->d_name.len <= NAME_MAX)
			memcpy(filename, fp_dentry->d_name.name,
					fp_dentry->d_name.len + 1);

	ecryptfs_printk(KERN_INFO, "ecryptfs_do_sdp_ioctl\n");

	if (!(crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED)) {
		printk("dek: SDP not enabled, skip sdp ioctl\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case ECRYPTFS_IOCTL_GET_SDP_INFO: {
		dek_arg_get_sdp_info req;

		printk("dek: ECRYPTFS_IOCTL_GET_SDP_INFO\n");
	
		memset(&req, 0, sizeof(dek_arg_get_sdp_info));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");	
			return -EFAULT;
		} else {
			mutex_lock(&crypt_stat->cs_mutex);
			if (crypt_stat->flags & ECRYPTFS_DEK_SDP_ENABLED) {
				req.sdp_enabled = 1;
			} else {
				req.sdp_enabled = 0;
			}
			if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
				req.is_sensitive = 1;
			} else {
				req.is_sensitive = 0;
			}
			req.type = crypt_stat->sdp_dek.type;
			mutex_unlock(&crypt_stat->cs_mutex);
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			return -EFAULT;
		}

		break;
		}
	case ECRYPTFS_IOCTL_GET_FEK: {
		dek_arg_get_fek req;

		printk("dek: ECRYPTFS_IOCTL_GET_FEK\n");
	
		if (crypt_stat->flags & ECRYPTFS_DEK_IS_SENSITIVE) {
			printk("dek: don't return FEK of sensitive file\n");
			return -EFAULT;
		}

		memset(&req, 0, sizeof(dek_arg_get_fek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_get_fek));
			return -EFAULT;
		} else {
			mutex_lock(&crypt_stat->cs_mutex);
			memcpy(req.dek.buf, crypt_stat->key, crypt_stat->key_size);
			req.dek.len = crypt_stat->key_size;
			req.dek.type = DEK_TYPE_PLAIN;
			mutex_unlock(&crypt_stat->cs_mutex);
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_get_fek));
			return -EFAULT;
		}
		memset(&req, 0, sizeof(dek_arg_get_fek));
		break;
		}

	case ECRYPTFS_IOCTL_GET_EFEK: {
		dek_arg_get_efek req;

		printk("dek: ECRYPTFS_IOCTL_GET_EFEK\n");
	
		memset(&req, 0, sizeof(dek_arg_get_efek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_get_efek));
			return -EFAULT;
		} else {
			mutex_lock(&crypt_stat->cs_mutex);
			memcpy(req.dek.buf, crypt_stat->sdp_dek.buf, crypt_stat->sdp_dek.len);
			req.dek.len = crypt_stat->sdp_dek.len;
			req.dek.type = crypt_stat->sdp_dek.type;
			mutex_unlock(&crypt_stat->cs_mutex);
		}
		if(copy_to_user(ubuf, &req, sizeof(req))) {
			printk("dek: can't copy to user\n");
			memset(&req, 0, sizeof(dek_arg_get_efek));
			return -EFAULT;
		}
		memset(&req, 0, sizeof(dek_arg_get_efek));
		break;
		}

	case ECRYPTFS_IOCTL_SET_EFEK: {
		dek_arg_set_efek req;

		printk("dek: ECRYPTFS_IOCTL_SET_EFEK\n");
	
		memset(&req, 0, sizeof(dek_arg_set_efek));
		if(copy_from_user(&req, ubuf, sizeof(req))) {
			printk("dek: can't copy from user\n");
			memset(&req, 0, sizeof(dek_arg_set_efek));
			return -EFAULT;
		} else {
			if (req.dek.type != DEK_TYPE_PLAIN) {
				mutex_lock(&crypt_stat->cs_mutex);
				memcpy(crypt_stat->sdp_dek.buf, req.dek.buf, req.dek.len);
				crypt_stat->sdp_dek.len = req.dek.len;
				crypt_stat->sdp_dek.type = req.dek.type;
				memset(crypt_stat->key, 0, crypt_stat->key_size);
				crypt_stat->flags &= ~(ECRYPTFS_KEY_SET);
				mutex_unlock(&crypt_stat->cs_mutex);
				ecryptfs_update_crypt_flag(ecryptfs_dentry, 1);
			} else {
				printk("dek: failed to set EFEK\n");
				memset(&req, 0, sizeof(dek_arg_set_efek));
				return -EFAULT;
			}
		}
		memset(&req, 0, sizeof(dek_arg_set_efek));
		break;
		}
	default: {
		return -EINVAL;
		break;
		}

	}
	return 0;
}
