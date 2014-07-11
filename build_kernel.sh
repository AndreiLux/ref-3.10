#!/bin/bash

export ARCH=arm
export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.7/bin/arm-eabi-

make apq8084_sec_defconfig VARIANT_DEFCONFIG=apq8084_sec_lentislte_skt_defconfig SELINUX_DEFCONFIG=selinux_defconfig TIMA_DEFCONFIG=tima_defconfig
make
