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
# kernel makefile template for module vdecdd
#

M := drivers/vcodec

KBUILD_CFLAGS := $(filter-out -Werror,$(KBUILD_CFLAGS))
ifeq ($(M),)
$(error Set $$M to a correct value to represent your path, ex. M := $$(srctree)/drivers/staging/img/)
endif
obj-y := vdecdd.o

ccflags-y += -g  -DVDEC -DUSE_SHARING -DUSE_FW_CTX_TRIMMING -DUSE_FW_RELOC_INFO_PACKING -D__RELEASE__="release" -D__RELEASE_RELEASE__ -DNDEBUG -DHAS_AVS -DHAS_H264 -DHAS_JPEG -DHAS_MPEG2 -DHAS_MPEG4 -DHAS_REAL -DHAS_VC1 -DHAS_VP6 -DHAS_VP8 -DVDEC_TARGET_ARM_GOLDFISH -DVDEC_MSVDX_HARDWARE -D_FILE_OFFSET_BITS=64 -D__linux__ -DNO_REGIO_CHECK_FIELD_VALUE -DFAKE_DEVIF -DENABLE_NULL_DEVIF -DSECURE_USE_SYSOS -DUSE_REAL_FW -DOMX_SHARED -D__PORT_FWRK__ -DSYSBRG_BRIDGING -DPORTFWRK_NO_UMISR -DVXD_BRIDGING -D_LOG_EVENTS_ -DIMG_MEMALLOC_UNIFIED_VMALLOC -DUSE_MTX_DMA_LOAD -DSECURE_MEDIA_REPORTING -DSEC_USE_REAL_FW -DSEC_USE_FW_RELOC_INFO_PACKING -DSEC_USE_FW_CTX_TRIMMING -DSECURE_TAL -DTARGET_ARM_GOLDFISH -D__TALMMU_USE_SYSAPIS__ -D__TALMMU_NO_TAL__ -DTARGET_HEADER_NAME=\"vxd_default.h\" -DIMG_KERNEL_MODULE -DANDROID_ION_BUFFERS

ccflags-y +=  -I$M/vdec/regdefs/msvdx -I$M/vdec/kernel_device/libraries/null/../../include -I$M/imgvideo/include -I$M/imgvideo/include/linux-kernel -I$M/imgvideo/system/include -I$M/imgvideo/port_fwrk/include -I$M/imgvideo/list_utils/include -I$M/imgvideo/secure_media/target/include -I$M/vdec/kernel_device/libraries/vdecdd/../../include -I$M/vdec/system/vdec -I$M/vdec/apis/vdec/include -I$M/vdec/apis/vdec/libraries/include -I$M/vdec/firmware/include -I$M/vdec/firmware/share -I$M/vdec/firmware/share/c99 -I$M/imgvideo/imglib/libraries/pixelapi/include -I$M/imgvideo/imglib/libraries/RegDefsUtils/include -I$M/imgvideo/imglib/libraries/listman/include -I$M/imgvideo/imglib/libraries/log_api/include -I$M/imgvideo/imglib/libraries/cbman/include -I$M/vdec/secure_media/secure_img/libraries/bspp/include -I$M/vdec/secure_media/secure_img/libraries/vxd/include -I$M/vdec/secure_media/secure_img/libraries/vxd/../../../include -I$M/vdec/secure_media/secure_mem/include -I$M/vdec/secure_media/secure_mem/../include -I$M/vdec/secure_media/secure_img/include -I$M/imgvideo/rpc/sysbrg/src -I$M/vdec/kernel_device/libraries/vdecdd/include -I$M/vdec/kernel_device/libraries/vdecdd/libcomp/include -I$M/imgvideo/secure_media/tal/include -I$M/vdec/secure_media/secure_img/libraries/device_interface/include -I$M/imgvideo/imglib/libraries/talmmu_api/include -I$M/vdec/secure_media/secure_img/libraries/vxd/include/vxd -I$M/vdec/firmware/bin

vdecdd-objs :=  kernel_device/libraries/null/code/null_device_km.o kernel_device/libraries/vdecdd/libcomp/decoder/code/decoder.o kernel_device/libraries/vdecdd/libcomp/decoder/code/hwctrl_api.o kernel_device/libraries/vdecdd/libcomp/decoder/code/translation_api.o kernel_device/libraries/vdecdd/libcomp/decoder/code/scaler_setup.o kernel_device/libraries/vdecdd/libcomp/decoder/code/msvdx_int.o kernel_device/libraries/vdecdd/libcomp/decoder/code/dec_resources.o kernel_device/libraries/vdecdd/libcomp/decoder/code/h264_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/h264_idx.o kernel_device/libraries/vdecdd/libcomp/decoder/code/mpeg4_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/mpeg4_idx.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vp8_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vp8_idx.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vp6_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vp6_idx.o kernel_device/libraries/vdecdd/libcomp/decoder/code/mpeg2_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/mpeg2_idx.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vc1_vlc.o kernel_device/libraries/vdecdd/libcomp/decoder/code/vc1_idx.o kernel_device/libraries/vdecdd/code/core_api.o kernel_device/libraries/vdecdd/code/plant.o kernel_device/libraries/vdecdd/code/resource.o kernel_device/libraries/vdecdd/code/scheduler.o kernel_device/libraries/vdecdd/code/vdecdd_mmu.o kernel_device/libraries/vdecdd/code/vdecdd_km.o kernel_device/libraries/vdecdd/libcomp/vdecdd_utils/code/vdecdd_utils.o kernel_device/libraries/vdecdd/libcomp/vdecdd_utils/code/vdecdd_utils_buf.o kernel_device/libraries/vdecdd/devif/memmapped-linux-arm-goldfish.o secure_media/secure_img/libraries/vxd/code/msvdx_io.o secure_media/secure_img/libraries/vxd/code/mtxio.o secure_media/secure_img/libraries/device_interface/code/memmappedio-linux.o firmware/bin/vdecfw_bin.o ../imgvideo/rpc/sysbrg/src/vdecdd_server.o
