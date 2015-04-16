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
# kernel makefile template for module topazkm
#

M := drivers/vcodec
KBUILD_CFLAGS := $(filter-out -Werror,$(KBUILD_CFLAGS))
ifeq ($(M),)
$(error Set $$M to a correct value to represent your path, ex. M := $$(srctree)/drivers/staging/img/)
endif

obj-y := topazkm.o

ccflags-y += -g  -D__linux__ -D__IMG_PRIVATE_APIS__ -D_FILE_OFFSET_BITS=64 -DTAL_TARGET_HEADER_NAME=\"target_config.h\" -DPALLOC_EXPOSE_KM_HANDLE -D__PORT_FWRK__ -DSYSBRG_BRIDGING -DPORTFWRK_NO_UMISR -DSECURE_TAL -D_LOG_EVENTS_ -D__TALMMU_USE_SYSAPIS__ -D__TALMMU_NO_TAL__ -DIMG_KERNEL_MODULE -DANDROID_ION_BUFFERS
ccflags-y +=  -I$M/venc/configs -I$M/venc/omx_components/topaz_hp/libraries/encode_api/include -I$M/venc/omx_components/topaz_hp/libraries/encode_api/code -I$M/venc/omx_components/topaz_hp/libraries/encode_api/../../include -I$M/venc/omx_components/topaz_hp/libraries/encode_api/../../../../firmware/topaz_hp/fwlib/include -I$M/imgvideo/include -I$M/imgvideo/include/linux-kernel -I$M/imgvideo/system/include -I$M/venc/system/include -I$M/venc/register_includes/topaz_hp -I$M/imgvideo/kernel_comp/include -I$M/imgvideo/osa/include -I$M/imgvideo/port_fwrk/include -I$M/imgvideo/list_utils/include -I$M/imgvideo/secure_media/target/include -I$M/venc/kernel_device/libraries/topaz_hp/../../../firmware/topaz_hp/fwlib/include -I$M/venc/kernel_device/libraries/topaz_hp/../../../omx_components/topaz_hp/include -I$M/venc/kernel_device/libraries/topaz_hp/../../include -I$M/imgvideo/secure_media/tal/include -I$M/imgvideo/imglib/libraries/log_api/include -I$M/imgvideo/imglib/libraries/RegDefsUtils/include -I$M/imgvideo/imglib/libraries/talmmu_api/include -I$M/imgvideo/imglib/libraries/cbman/include -I$M/imgvideo/imglib/libraries/listman/include -I$M/imgvideo/rpc/sysbrg/src

topazkm-objs :=  kernel_device/libraries/topaz_hp/code/memmgr/memmgr_km.o kernel_device/libraries/topaz_hp/code/dev/goldfish.o kernel_device/libraries/topaz_hp/code/hostutils.o kernel_device/libraries/topaz_hp/code/fwtrace.o kernel_device/libraries/topaz_hp/code/MTX_FwIF.o kernel_device/libraries/topaz_hp/code/topazsc_mmu.o kernel_device/libraries/topaz_hp/code/topaz_device_km.o ../imgvideo/rpc/sysbrg/src/memmgr_api_server.o ../imgvideo/rpc/sysbrg/src/hostutils_api_server.o
