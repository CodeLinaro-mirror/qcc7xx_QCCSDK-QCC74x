#!/bin/bash

CURRENT_DIR=`pwd`
if [[ "$MATTER_REPO_PATH" == "" ]]; then
    MATTER_REPO_PATH="/home/workplace/matter/matter_qcc74x"
fi

CONFIG_MATTER_MFD=`grep -E "CONFIG_MATTER_MFD[[:space:]]*=[[:space:]]*y" defconfig`
if [[ "$CONFIG_MATTER_MFD" == "" ]]; then
    echo "Please set CONFIG_MATTER_MFD to y for Matter MFD feature"
    echo "And rebuild the project using following commands"
    echo "./matter_build_prepare.sh"
    echo "make build"
    exit -1
fi

cd $MATTER_REPO_PATH

if [ ! -e "${MATTER_REPO_PATH}/out/linux-x64-chip-cert/chip-cert" ]; then
    echo "out/linux-x64-chip-cert/chip-cert is required, but not existed"
    echo "please enter your connectedhomeip project and build as following commands"
    echo "source scripts/activate.sh"
    echo "./scripts/build/build_examples.py --target linux-x64-chip-cert build"

    cd $CURRENT_DIR
    exit -1
fi

rm ${CURRENT_DIR}/build/build_out/out_0x* -f
./scripts/tools/qcc74x/generate_factory_data.py --output ${CURRENT_DIR}/build/build_out

mfd_file=`find ${CURRENT_DIR}/build/build_out -name out_*_mfd.bin`
if [ ! -e "${mfd_file}" ]; then
    echo "failed to generate mfd partition file"
    cd $CURRENT_DIR
    exit -1
fi

echo "[cfg]
erase = 1
skip_mode = 0x0, 0x0
boot2_isp_mode = 0

[boot2]
filedir = ./build/build_out/boot2_*.bin
address = 0x000000

[partition]
filedir = ./build/build_out/partition*.bin
address = 0xE000

[FW]
filedir = ./build/build_out/matter*_\$(CHIPNAME).bin
address = @partition

[MFD]
filedir = ${mfd_file}
address = @partition
" > ${CURRENT_DIR}/flash_prog_cfg.ini

cd $CURRENT_DIR
