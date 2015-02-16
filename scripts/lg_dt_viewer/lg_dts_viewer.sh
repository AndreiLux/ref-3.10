#guilbert.lee@lge.com Mon 28 Jan 2013
#lg dts viewer

#!/bin/sh

DTS_PATH=$1
WORK_PATH=${0%/*}
FILTER="scripts"
KERNEL_PATH=${PWD%$FILTER*}

if [ "$DTS_PATH" = "" ]
then
    echo "usage : ./lg_dts_viewer.sh [dts path]"

    echo "dts path : path of dts file"
    echo "ex(in Kernel root) :  ./scripts/lg_dt_viewer/\
lg_dts_viewer.sh arch/arm/boot/dts/lge/msm8994-s3/msm8994-s3.dts"
    exit
fi

DTS_NAME=${DTS_PATH##*/}

if [ "$DTS_NAME" = "" ]
then
   echo "usage : Invaild DTS path, Cannot find *.dts file"
   exit
fi

OUT_PATH=${DTS_NAME/%.dts/}
OUT_PATH=out_$OUT_PATH
if [ ! -d "$OUT_PATH" ] ; then
	mkdir $OUT_PATH
fi

# For support #include, C-pre processing first to dts.

gcc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
		-I $KERNEL_PATH/arch/arm/boot/dts/ \
		-I $KERNEL_PATH/arch/arm/boot/dts/include/ \
		$DTS_PATH -o $OUT_PATH/$DTS_NAME.preprocessing

# Now, transfer to dts from dts with LG specific

${WORK_PATH}/lg_dtc -o $OUT_PATH/$DTS_NAME\
 -D -I dts -O dts -H specific -s ./$OUT_PATH/$DTS_NAME.preprocessing
${WORK_PATH}/lg_dtc -o $OUT_PATH/$DTS_NAME.2\
 -D -I dts -O dts -H specific2 -s ./$OUT_PATH/$DTS_NAME.preprocessing
