#include <linux/module.h>
#include <linux/sysfs.h>
#include "pvr_sysfs.h"
#include "pvrsrv.h"
#include "rgxdevice.h"


/****************       sysfs(freq_max)       *****************/
static ssize_t PVRsysfsMaxFreqShow(struct device *dev, struct device_attribute *attr,char *buf) {

	PVRSRV_DATA *psPVRSRVData;
	PVRSRV_DEVICE_NODE*  psDeviceNode;

	psPVRSRVData=PVRSRVGetPVRSRVData();
	psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

	if(psDeviceNode != NULL){
		/* Write the device status to the sequence file... */
                if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
                {
                        /* Write other useful stats to aid the test cylce... */
                        if (psDeviceNode->pvDevice != NULL)
                        {
                                PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
                                PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;
                                RGX_DATA		*psRGXData = (RGX_DATA *)psDevConfig->hDevData;
                                /* Write other useful stats to aid the test cylce... */
                               return sprintf(buf, "%9u\n", psRGXData->psRGXTimingInfo->ui32GpuMaxFreq);
                        }
                }
		else{
			switch (psDeviceNode->eHealthStatus)
                        {
                                case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                                        return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                                        return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                default:
                                        return sprintf(buf, "Device %d Status: %d\n",
                                                   psDeviceNode->sDevId.ui32DeviceIndex,
                                                   psDeviceNode->eHealthStatus);
                                        break;
                        }
                }
	}
}
static ssize_t PVRsysfsMaxFreqStore(struct device *dev, struct device_attribute *attr,const char *buf, size_t size) {
	PVRSRV_DATA             *psPVRSRVData = PVRSRVGetPVRSRVData();
    PVRSRV_DEVICE_NODE      *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];
    PVRSRV_DEVICE_CONFIG    *psDevConfig;
    RGX_DATA                *psRGXData;

    size_t rc;

    psDevConfig = psDeviceNode->psDevConfig;
    psRGXData = (RGX_DATA *)psDevConfig->hDevData;

    if (sscanf(buf, "%9u", &psRGXData->psRGXTimingInfo->ui32GpuMaxFreq) == 0)
    	return -EINVAL;

	rc = strnlen(buf,size);

    //PVR_DPF((PVR_DBG_ERROR, "Throttling : [%u]hz", psRGXData->psRGXTimingInfo->ui32GpuMaxFreq));

    return rc;
}


/****************       sysfs(freq_min)       *****************/
static ssize_t PVRsysfsMinFreqShow(struct device *dev, struct device_attribute *attr, char *buf) {
	PVRSRV_DATA *psPVRSRVData;
    PVRSRV_DEVICE_NODE*  psDeviceNode;

    psPVRSRVData=PVRSRVGetPVRSRVData();
    psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

    if(psDeviceNode != NULL){
    	/* Write the device status to the sequence file... */
        if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
        {
        	/* Write other useful stats to aid the test cylce... */
            if (psDeviceNode->pvDevice != NULL)
            {
            	PVRSRV_RGXDEV_INFO      *psDevInfo = psDeviceNode->pvDevice;
                PVRSRV_DEVICE_CONFIG    *psDevConfig = psDeviceNode->psDevConfig;
                RGX_DATA                *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
                /* Write other useful stats to aid the test cylce... */
                return sprintf(buf, "%9u\n", psRGXData->psRGXTimingInfo->ui32GpuMinFreq);
            }
        }
        else{
            switch (psDeviceNode->eHealthStatus)
            {
             	case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                	return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                	break;
               	case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                    return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                    break;
                default:
                    return sprintf(buf, "Device %d Status: %d\n", psDeviceNode->sDevId.ui32DeviceIndex, psDeviceNode->eHealthStatus);
                    break;
             }
		}
	}
}
static IMG_INT  PVRsysfsMinFreqStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	PVRSRV_DATA             *psPVRSRVData = PVRSRVGetPVRSRVData();
    PVRSRV_DEVICE_NODE      *psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];
    PVRSRV_DEVICE_CONFIG    *psDevConfig;
    RGX_DATA                *psRGXData;

	size_t rc;

    psDevConfig = psDeviceNode->psDevConfig;
    psRGXData = (RGX_DATA *)psDevConfig->hDevData;

    if (sscanf(buf, "%9u", &psRGXData->psRGXTimingInfo->ui32GpuMinFreq) == 0)
    	return -EINVAL;

	rc = strnlen(buf,size);

	//PVR_DPF((PVR_DBG_ERROR, "Throttling : [%u]hz", psRGXData->psRGXTimingInfo->ui32GpuMinFreq));

	return rc;
}



/******************       sysfs(load)       *******************/
static ssize_t PVRsysfsLoadShow(struct device *dev, struct device_attribute *attr,char *buf) {
	PVRSRV_DATA *psPVRSRVData;
    PVRSRV_DEVICE_NODE*  psDeviceNode;

    psPVRSRVData=PVRSRVGetPVRSRVData();
    psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

    if(psDeviceNode != NULL){
    	/* Write the device status to the sequence file... */
        if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
        {
           /* Write other useful stats to aid the test cylce... */
           if (psDeviceNode->pvDevice != NULL)
           {
           		PVRSRV_RGXDEV_INFO      *psDevInfo = psDeviceNode->pvDevice;
                PVRSRV_DEVICE_CONFIG    *psDevConfig = psDeviceNode->psDevConfig;
                RGX_DATA                *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
                	/* Write other useful stats to aid the test cylce... */
                	return sprintf(buf, "%u\n", psRGXData->psRGXTimingInfo->ui32GpuUtilHistory);
            }
        }
         else{
            switch (psDeviceNode->eHealthStatus)
            {
                case PVRSRV_DEVICE_HEALTH_STATUS_OK:
            	    return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                    break;
                case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                    return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                    break;
                default:
                    return sprintf(buf, "Device %d Status: %d\n", psDeviceNode->sDevId.ui32DeviceIndex, psDeviceNode->eHealthStatus);
                    break;
            }
       }
  }

}
static ssize_t PVRsysfsLoadStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
        printk("PVRsysfsLoadStore is not implemented!!");
        return sprintf(buf,"PVRsysfsLoadStore is not implemented!!\n");
}


/***************       sysfs(freq_current)       **************/
static ssize_t PVRsysfsFreqCurrentShow(struct device *dev, struct device_attribute *attr, char *buf) {
	PVRSRV_DATA *psPVRSRVData;
    PVRSRV_DEVICE_NODE*  psDeviceNode;

    psPVRSRVData=PVRSRVGetPVRSRVData();
    psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

    if(psDeviceNode != NULL){
            /* Write the device status to the sequence file... */
            if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
            {
                    /* Write other useful stats to aid the test cylce... */
                    if (psDeviceNode->pvDevice != NULL)
                    {
                            PVRSRV_RGXDEV_INFO      *psDevInfo = psDeviceNode->pvDevice;
                            PVRSRV_DEVICE_CONFIG    *psDevConfig = psDeviceNode->psDevConfig;
                            RGX_DATA                *psRGXData = (RGX_DATA *)psDevConfig->hDevData;
                            /* Write other useful stats to aid the test cylce... */
                            return sprintf(buf, "%u\n", psRGXData->psRGXTimingInfo->ui32CoreClockSpeed);
                    }
            }
            else{
                    switch (psDeviceNode->eHealthStatus)
                    {
                            case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                                    return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                    break;
                            case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                                    return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                    break;
                            default:
                                    return sprintf(buf, "Device %d Status: %d\n",
                                              psDeviceNode->sDevId.ui32DeviceIndex,
                                              psDeviceNode->eHealthStatus);
                                    break;
                    }
            }
    }
}
static ssize_t PVRsysfsFreqCurrentStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
        printk("PVRsysfsFreqCurrentStore is not implemented!!");
        return sprintf(buf,"PVRsysfsFreqCurrentStore is not implemented!!\n");
}


/*****************       sysfs(avial)       *******************/
static ssize_t PVRsysfsAvailShow(struct device *dev, struct device_attribute *attr,char *buf) {
	PVRSRV_DATA *psPVRSRVData;
    PVRSRV_DEVICE_NODE*  psDeviceNode;

	char *tempBuf;
	int lenBuf;
	int i;


	psPVRSRVData= PVRSRVGetPVRSRVData();
	psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

        if(psDeviceNode != NULL){
                /* Write the device status to the sequence file... */
                if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
                {
                        /* Write other useful stats to aid the test cylce... */
                        if (psDeviceNode->pvDevice != NULL)
                        {
								PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
                                PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;
                                RGX_DATA		*psRGXData = (RGX_DATA *)psDevConfig->hDevData;

								lenBuf=0;
                                for (i = 0; i < RGX_CORE_LEVEL - 1; i++){
									lenBuf+=sprintf(buf+lenBuf, "%u ", psRGXData->psRGXTimingInfo->ui32AvailableClockSpeed[i]);
                                }

								return sprintf(buf,"%s\n", buf);

                        }
                }
                else{
                        switch (psDeviceNode->eHealthStatus)
                        {
                                case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                                        return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                                        return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                default:
                                        return sprintf(buf, "Device %d Status: %d\n",
                                                   psDeviceNode->sDevId.ui32DeviceIndex,
                                                   psDeviceNode->eHealthStatus);
                                        break;
                        }
                }
        }


}
static ssize_t PVRsysfsAvailStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
        printk("PVRsysfsAvailStore is not implemented!!");
        return sprintf(buf,"PVRsysfsAvailStore is not implemented!!\n");
}

/*****************       sysfs(info_max)       *******************/
static ssize_t PVRsysfsInfoMaxShow(struct device *dev, struct device_attribute *attr,char *buf) {
	PVRSRV_DATA *psPVRSRVData;
        PVRSRV_DEVICE_NODE*  psDeviceNode;

	char *tempBuf;
	int lenBuf;
	int i;


	psPVRSRVData= PVRSRVGetPVRSRVData();
	psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

        if(psDeviceNode != NULL){
                /* Write the device status to the sequence file... */
                if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
                {
                        /* Write other useful stats to aid the test cylce... */
                        if (psDeviceNode->pvDevice != NULL)
                        {
								PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
                                PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;
                                RGX_DATA		*psRGXData = (RGX_DATA *)psDevConfig->hDevData;


								return sprintf(buf,"%u\n", RGX_HW_CORE_MAX_CLOCK_SPEED);

                        }
                }
                else{
                        switch (psDeviceNode->eHealthStatus)
                        {
                                case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                                        return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                                        return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                default:
                                        return sprintf(buf, "Device %d Status: %d\n",
                                                   psDeviceNode->sDevId.ui32DeviceIndex,
                                                   psDeviceNode->eHealthStatus);
                                        break;
                        }
                }
        }


}
static ssize_t PVRsysfsInfoMaxStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
        printk("PVRsysfsInfoMaxStore is not implemented!!");
        return sprintf(buf,"PVRsysfsInfoMaxStore is not implemented!!\n");
}

/*****************       sysfs(info_min)       *******************/
static ssize_t PVRsysfsInfoMinShow(struct device *dev, struct device_attribute *attr,char *buf) {
	PVRSRV_DATA *psPVRSRVData;
        PVRSRV_DEVICE_NODE*  psDeviceNode;

	char *tempBuf;
	int lenBuf;
	int i;


	psPVRSRVData= PVRSRVGetPVRSRVData();
	psDeviceNode = psPVRSRVData->apsRegisteredDevNodes[0];

        if(psDeviceNode != NULL){
                /* Write the device status to the sequence file... */
                if (psDeviceNode->sDevId.eDeviceType == PVRSRV_DEVICE_TYPE_RGX)
                {
                        /* Write other useful stats to aid the test cylce... */
                        if (psDeviceNode->pvDevice != NULL)
                        {
								PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
                                PVRSRV_DEVICE_CONFIG	*psDevConfig = psDeviceNode->psDevConfig;
                                RGX_DATA		*psRGXData = (RGX_DATA *)psDevConfig->hDevData;


								return sprintf(buf,"%u\n", RGX_HW_CORE_MIN_CLOCK_SPEED);

                        }
                }
                else{
                        switch (psDeviceNode->eHealthStatus)
                        {
                                case PVRSRV_DEVICE_HEALTH_STATUS_OK:
                                        return sprintf(buf, "Device %d Status: OK\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                case PVRSRV_DEVICE_HEALTH_STATUS_DEAD:
                                        return sprintf(buf, "Device %d Status: DEAD\n", psDeviceNode->sDevId.ui32DeviceIndex);
                                        break;
                                default:
                                        return sprintf(buf, "Device %d Status: %d\n",
                                                   psDeviceNode->sDevId.ui32DeviceIndex,
                                                   psDeviceNode->eHealthStatus);
                                        break;
                        }
                }
        }


}
static ssize_t PVRsysfsInfoMinStore(struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
        printk("PVRsysfsInfoMinStore is not implemented!!");
        return sprintf(buf,"PVRsysfsInfoMinStore is not implemented!!\n");
}


static struct class_attribute pvrInfoClass_attrs[] = {
	__ATTR(freq_max, S_IWUSR | S_IRUGO, PVRsysfsMaxFreqShow, PVRsysfsMaxFreqStore),
	__ATTR(freq_min, S_IWUSR | S_IRUGO, PVRsysfsMinFreqShow, PVRsysfsMinFreqStore),
	__ATTR(load, S_IWUSR | S_IRUGO, PVRsysfsLoadShow, PVRsysfsLoadStore),
	__ATTR(freq_current, S_IWUSR | S_IRUGO, PVRsysfsFreqCurrentShow, PVRsysfsFreqCurrentStore),
	__ATTR(avail, S_IWUSR | S_IRUGO, PVRsysfsAvailShow, PVRsysfsAvailStore),
	__ATTR(info_max, S_IWUSR | S_IRUGO, PVRsysfsInfoMaxShow, PVRsysfsInfoMaxStore),
	__ATTR(info_min, S_IWUSR | S_IRUGO, PVRsysfsInfoMinShow, PVRsysfsInfoMinStore),
	__ATTR_NULL
};

static struct class pvrInfoClass={
	.name="gpu",
	.owner=THIS_MODULE,
	.class_attrs = pvrInfoClass_attrs
};

int PVRInfo_SysfsInit(void){
	int ret=0;
	class_register(&pvrInfoClass);
	return ret;
}

void  PVRInfo_SysfsDeinit(void){
	class_unregister(&pvrInfoClass);
}
