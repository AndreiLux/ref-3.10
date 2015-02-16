/* FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION */

#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include "LGSDEncManager.h"

int propertyMediaCheck;	/* whether media ecryption is checked */
char savedfileExtList[MAX_MEDIA_EXT_LENGTH];
char asecExtension[MAX_MEDIA_EXT_LENGTH] = ".ASEC";

/*
* Saves whether system property of media encryption is checked
*/
asmlinkage long sys_set_media_property(int value)
{
/*	printk("%s :: SystemCall value: %d , propertymediacheck : %d\n", __func__,value,propertyMediaCheck); */
	propertyMediaCheck = value;
	return 1;
}

int getMediaProperty(void)
{
	return propertyMediaCheck;
}

/*
* Saves extension list of media file
*/
asmlinkage long sys_set_media_ext(char *mediaExtList)
{
	memset(savedfileExtList, 0, sizeof(savedfileExtList));

	if (mediaExtList != NULL) {
		strncpy(savedfileExtList, mediaExtList, strlen(mediaExtList));
	}

/*	printk("%s :: savedfileExtList: %s\n", __func__,savedfileExtList); */

	return 1;
}
/* #endif //FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION */

char *ecryptfs_Extfilename(const unsigned char *filename)
{
	char *pos = NULL;
	int len , i;

	if (filename == NULL) {
		return pos;
	}

	/* extract extension of file : ex> a.txt -> .txt */
	pos = strrchr(filename, '.');
	if (pos == NULL) {
		return pos;
	}

	/* lowercase -> uppercase */
	len = strlen(pos);
	for (i = 0 ; i < len ; i++) {
		if (*(pos+i) >= 'a' && *(pos+i) <= 'z') {
			*(pos+i) = *(pos+i) - 32;
		}
	}
	return pos+1;
}


int ecryptfs_asecFileSearch(const unsigned char *filename)
{
    char *extP = NULL;

    /* extract extension in filename */
    extP = ecryptfs_Extfilename(filename);
    if (extP == NULL || strlen(extP) < 2) {
		printk(KERN_DEBUG "Extfilename is NULL\n");
		return 0;
    }

    /* check if the extension is asec */
    if (strstr(asecExtension, extP) == NULL) {
		return 0;
    }
    return 1;
}

int ecryptfs_mediaFileSearch(const unsigned char *filename)
{
	char *extP = NULL;

	/* extract extension in filename */
	extP = ecryptfs_Extfilename(filename);
	if (extP == NULL || strlen(extP) < 2) {
		printk(KERN_DEBUG "%s :: Extfilename is NULL\n", __func__);
		return 0;
	}

	printk("%s :: savedfileExtList: %s\n", __func__, savedfileExtList);

    /* check if the extension exists in MediaType
	   if exists status = 1 meaning the file is media file */
	if (sizeof(savedfileExtList) != 0) {
		if (strstr(savedfileExtList, extP) == NULL) {
			printk(KERN_DEBUG "%s :: NOT ECRYPTFS_MEDIA_EXCEPTION\n", __func__);
			return 0;
		}
	} else {
		printk(KERN_DEBUG "%s :: getMediaExtList() = NULL\n", __func__);
		return 0;
	}

	return 1;
}

