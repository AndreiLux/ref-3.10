#!/bin/bash

YELLOW='\e[1;33m';
RED='\e[1;31m';
GREEN='\e[1;32m';
NCOL='\e[0m';

T=`pwd`
BOOTIMG_OUT=${T}/out_bootimg
ZIMG_OUT=${T}/arch/arm/boot
TOOLS=${T}/tools/swapbootimg
export SEL_DEFCONFIG=0
export J_OP=4

# clean build
while getopts "cd:j:" opt; do
	case $opt in
		c) echo -e ${YELLOW} "Kernel clean" ${NCOL}
			make clean
			make mrproper
			rm -rf $BOOTIMG_OUT
			;;
		d) SEL_DEFCONFIG=$OPTARG
			;;
		j) J_OP=$OPTARG
			;;
	esac
done
shift $(( $OPTIND -1 ))

# check defconfig
if [ $SEL_DEFCONFIG == "0" ]; then
	echo -e ${RED} "Please select defconfig (ex: liger_lgu_kr_defconfig)" ${NCOL}
	exit 1;
fi

# make a out folder
if [ ! -e $BOOTIMG_OUT ]; then
	mkdir -p $BOOTIMG_OUT
fi

echo -e ${YELLOW} "Did you copy a boot.img in out_bootimg ? [y/N]" ${NCOL}
read answer

if [ $answer != "y" ]; then
	echo -e ${RED} "Please copy a boot.img in out_bootimg" ${NCOL}
	exit 1;
fi

if [ ! -e ${BOOTIMG_OUT}/boot.img ]; then
	echo -e ${RED} "Can not search boot.img" ${NCOL}
	exit 1;
fi

# kernel build
make all -f KernelOnly.mk

if [ $? -ne 0 ]; then
	echo -e ${RED} "FAIL to build kernel" ${NCOL}
	exit 1;
fi

make adtb -f KernelOnly.mk
if [ $? -ne 0 ]; then
	echo -e ${RED} "FAIL to build device tree" ${NCOL}
	exit 1;
fi

# copy result file of build
cp ${T}/System.map $BOOTIMG_OUT/System.map
cp ${T}/vmlinux $BOOTIMG_OUT/vmlinux
cp $ZIMG_OUT/zImage_dtb $BOOTIMG_OUT/zImage_dtb

# split original boot.img and merge to new boot.img
mkdir $BOOTIMG_OUT/split
cd $BOOTIMG_OUT/split
${TOOLS}/split_bootimg.pl $BOOTIMG_OUT/boot.img
${TOOLS}/unpackbootimg -i $BOOTIMG_OUT/boot.img

# re-make ramdisk.gz
#mkdir ramdisk
#cd ramdisk
#gzip -dc ../boot.img-ramdisk.gz |cpio -i
#cd ../
#echo "Entering 1 : "`pwd`
#cd ../
#echo "Entering 2 : `pwd`

# change to new kernel from old kernel
cp ${BOOTIMG_OUT}/zImage_dtb ${BOOTIMG_OUT}/split/boot.img-kernel
cp ${BOOTIMG_OUT}/zImage_dtb ${BOOTIMG_OUT}/split/boot.img-zImage

# make new boot.img
${TOOLS}/mkbootimg --kernel ${BOOTIMG_OUT}/split/boot.img-kernel --ramdisk ${BOOTIMG_OUT}/split/boot.img-ramdisk.gz --cmdline "init=/init rootdelay=5 rootfstype=ext4 console=ttyS0,115200,n8 androidboot.hardware=liger no_console_suspend" --base 0x80000000 --pagesize 2048 --ramdisk_offset 0x01000000 --tags_offset 0x80000100 --output ${BOOTIMG_OUT}/new_boot.img

if [ $? -eq 0 ];then
	cd ../../
	make mrproper
	rm -rf temp
	cd arch/arm/boot/
	git clean -xdf
	echo -e ${GREEN} "SUCCESS to make a new boot.img" ${NCOL}
	exit 0;
fi

