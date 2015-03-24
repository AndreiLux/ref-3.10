#ifndef __DDP_PATH_H__
#define __DDP_PATH_H__

#include "ddp_ovl.h"
#include "ddp_rdma.h"
#include "ddp_wdma.h"
#include "ddp_bls.h"
#include "ddp_drv.h"
#include "ddp_scl.h"
#include "ddp_rot.h"
#include "ddp_hal.h"

#define DDP_USE_CLOCK_API
#define DDP_OVL_LAYER_MUN 4
#define DDP_BACKUP_REG_NUM 0x1000
#define DDP_UNBACKED_REG_MEM 0xdeadbeef

/* all move to ddp_hal.h */

#ifdef DDP_USE_CLOCK_API
/* int disp_reg_backup(void); */
/* int disp_reg_restore(void); */
#endif
#endif
