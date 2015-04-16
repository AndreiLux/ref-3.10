###############################################################################
#
# Copyright (c) Imagination Technologies Ltd.
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a 
# copy of this software and associated documentation files (the "Software"), 
# to deal in the Software without restriction, including without limitation 
# the rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the 
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in 
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
# THE SOFTWARE.
# 
# Alternatively, the contents of this file may be used under the terms of the 
# GNU General Public License Version 2 ("GPL")in which case the provisions of
# GPL are applicable instead of those above. 
# 
# If you wish to allow use of your version of this file only under the terms 
# of GPL, and not to allow others to use your version of this file under the 
# terms of the MIT license, indicate your decision by deleting the provisions 
# above and replace them with the notice and other provisions required by GPL 
# as set out in the file called “GPLHEADER” included in this distribution. If 
# you do not delete the provisions above, a recipient may use your version of 
# this file under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called 
# "MIT_COPYING".
#
###############################################################################

#
# kernel makefile template for module imgvideo
#

M :=drivers/vcodec
KBUILD_CFLAGS := $(filter-out -Werror,$(KBUILD_CFLAGS))
ifeq ($(M),)
$(error Set $$M to a correct value to represent your path, ex. M := $$(srctree)/drivers/staging/img/)
endif

obj-y := imgvideo.o

ccflags-y += -g  -DIMG_MEMALLOC_UNIFIED_VMALLOC -D__linux__ -D__PORT_FWRK__ -DSYSBRG_BRIDGING -DPORTFWRK_NO_UMISR -D_LOG_EVENTS_ -DSECURE_TAL -DTARGET_SECURE_TAL -DTARGET_CONF_BY_HEADER_FILE -DPALLOC_EXPOSE_KM_HANDLE -D__TALMMU_USE_SYSAPIS__ -D__TALMMU_NO_TAL__ -DIMG_KERNEL_MODULE  -DANDROID_ION_BUFFERS
ccflags-y +=  -I$M/imgvideo/port_fwrk/include -I$M/imgvideo/system/include -I$M/imgvideo/list_utils/include -I$M/imgvideo/secure_media/target/include -I$M/imgvideo/include -I$M/imgvideo/include/linux-kernel -I$M/imgvideo/imglib/libraries/log_api/include -I$M/imgvideo/rpc/sysbrg/src -I$M/imgvideo/port_fwrk/libraries/sysbrg/linux/code -I$M/imgvideo/secure_media/tal/include -I$M/imgvideo/kernel_comp/include -I$M/imgvideo/osa/include -I$M/imgvideo/imglib/libraries/listman/include -I$M/imgvideo/imglib/libraries/talmmu_api/include -I$M/imgvideo/imglib/libraries/cbman/include -I$M/imgvideo/imglib/libraries/pixelapi/include -I$M/imgvideo/imglib/libraries/pixelapi/code

imgvideo-objs :=  port_fwrk/libraries/dbgevent/code/dbgevent_api.o port_fwrk/libraries/dbgevent/code/dbgevent_api_km.o port_fwrk/libraries/ump/code/ump_api_km.o port_fwrk/libraries/dbgopt/code/dbgopt_api.o port_fwrk/libraries/dbgopt/code/dbgopt_api_km.o port_fwrk/libraries/dman/code/dman_api.o port_fwrk/libraries/dman/code/dman_api_km.o port_fwrk/libraries/pman/code/pman_api.o port_fwrk/libraries/pool/code/pool_api.o port_fwrk/libraries/sysbrg/utils/code/sysbrg_utils.o port_fwrk/libraries/sysdev/utils/code/sysdev_utils.o port_fwrk/libraries/sysenv/utils/code/sysenv_utils.o port_fwrk/libraries/sysmem/utils/code/sysmem_utils.o port_fwrk/libraries/rman/code/rman_api.o port_fwrk/libraries/idgen/code/idgen_api.o port_fwrk/libraries/logman/code/logman_api.o port_fwrk/libraries/sysdev/linux/code/sysdev_api_km.o port_fwrk/libraries/sysenv/linux/code/sysenv_api_km.o port_fwrk/libraries/sysos/linux/code/sysos_api_km.o port_fwrk/libraries/sysbrg/linux/code/sysbrg_api_km.o port_fwrk/libraries/sysbrg/linux/code/sysbrg_drv.o port_fwrk/libraries/perflog/linux/code/perflog_api.o port_fwrk/libraries/sysmem/unified/code/sysmem_api_km.o list_utils/src/dq/dq.o list_utils/src/lst/lst.o list_utils/src/trees/addchild.o list_utils/src/trees/addsib.o list_utils/src/trees/copysub.o list_utils/src/trees/finalsib.o list_utils/src/trees/firstchild.o list_utils/src/trees/init.o list_utils/src/trees/isinsub.o list_utils/src/trees/nextsib.o list_utils/src/trees/prevsib.o list_utils/src/trees/remleaf.o list_utils/src/trees/remlsub.o list_utils/src/trees/remsub.o list_utils/src/trees/subnext.o secure_media/tal/code/tal.o secure_media/target/code/target.o kernel_comp/libraries/page_alloc/code/page_alloc.o kernel_comp/libraries/page_alloc/code/page_alloc_ion.o kernel_comp/libraries/wrap_utils/code/wrap_utils.o imglib/libraries/talmmu_api/code/talmmu_api.o imglib/libraries/talmmu_api/code/addr_alloc1.o imglib/libraries/talmmu_api/code/hash.o imglib/libraries/talmmu_api/code/pool.o imglib/libraries/talmmu_api/code/ra.o imglib/libraries/talmmu_api/code/trace.o imglib/libraries/pixelapi/code/pixel_api.o imglib/libraries/pixelapi/code/pixel_api_internals.o imglib/libraries/log_api/linux_km/code/log_api.o rpc/sysbrg/src/dbgevent_api_server.o rpc/sysbrg/src/ump_api_server.o rpc/sysbrg/src/dman_api_server.o rpc/sysbrg/src/dbgopt_api_server.o rpc/sysbrg/src/logman_api_server.o rpc/sysbrg/src/page_alloc_server.o rpc/sysbrg/src/wrap_utils_server.o
