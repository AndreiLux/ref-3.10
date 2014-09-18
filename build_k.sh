##!/bin/bash

export ARCH=arm
export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.6/bin/arm-eabi-
make slteskt_04_defconfig
make
