#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later */
#
# Copyright (C) 2023 Verisilicon Inc.

export TOP_DIR=$(pwd)
export AQROOT=$TOP_DIR
export AQARCH=$AQROOT/arch/XAQ2
export SDK_DIR=$AQROOT/build/sdk
export VIVANTE_SDK_DIR=$SDK_DIR
export VIVANTE_SDK_INC=$SDK_DIR/include
export VIVANTE_SDK_LIB=$SDK_DIR/drivers
export ARCH_TYPE=x86_64
export CPU_TYPE=
export FIXED_ARCH_TYPE=x86_64
if [ -z "$KERNEL_VERSION" ]; then
  export KERNEL_DIR=/lib/modules/$(uname -r)/build
else
  echo "Building designated version: ${KERNEL_VERSION}"
  export KERNEL_DIR=/lib/modules/${KERNEL_VERSION}/build
fi
export TOOLCHAIN=
export LIB_DIR=./compiler/libVSC/x86_64/
export PATH=$TOOLCHAIN/bin:$PATH

BUILD_OPTIONS="$KERNEL_DRIVER_BUILD_OPTIONS"

if [ -n "$OSAL_INCLUDE" ]; then
  BUILD_OPTIONS+=" OSAL_INCLUDE=$OSAL_INCLUDE"
fi

if [[ $SPARSE == "y" ]]; then
  SPARSE="all C=1 sparse"
fi

export PATH=$TOOLCHAIN/bin:$PATH

cmd_help() {
  echo "command help:"
  echo "./build_driver.sh help                    ---> help"
  echo "./build_driver.sh build                   ---> build driver and shared libraries"
  echo "./build_driver.sh clean                   ---> clean all generated files"
  echo "./build_driver.sh tools                   ---> build tools"
  echo "sudo ./build_driver.sh install [config]   ---> install driver with config"
}

cmd_tools() {
  echo "build tools"
  cd $AQROOT
  make -j4 $BUILD_OPTIONS test
}

cmd_build() {
  set -o pipefail
  cd $AQROOT
  make -j$(nproc) $BUILD_OPTIONS $SPARSE 2>&1 | tee $AQROOT/linux_build.log
  RET=$?
  set +o pipefail
  return $RET
}

cmd_clean() {
  echo "clean all generated files"
  cd $AQROOT
  make -j4 $BUILD_OPTIONS clean
  cd $AQROOT
  rm trans.mod linux_build.log -rf
}

cmd_install() {
  echo "installing driver"
  drv_name=ama_transcoder
  sudo dmesg -C
  if [ "$(lsmod | grep -w $drv_name)" != "" ]; then
    rmmod $drv_name
    if (($? != 0)); then
      echo "driver removing failed"
      exit 1
    fi
  fi
  echo "installing $drv_name.ko with parameters:"${@}""
  insmod $drv_name.ko "${@}"
  ret=$?
  devices=$(ls /dev/$drv_name[0-9]* 2>/dev/null | wc -l)
  if ((ret != 0 || devices == 0)); then
    echo -e "\n=== driver installation is failed, the latest dmesg log: ===\n"
    sudo dmesg >dmesg.log
    tail -n 30 dmesg.log
    exit 1
  fi
  echo "driver had been installed"
}

for ((i = 1; i <= $#; i++)); do
  opt=${!i}
  optarg="${opt#*=}"
  next_opt=$((i + 1))
  case "$opt" in
  --help | help)
    cmd_help
    exit 0
    ;; # These are not errors
  --tools | tools)
    cmd_tools
    exit 0
    ;;
  --install | install)
    cmd_install "${@:next_opt}"
    exit 0
    ;;
  --clean | clean)
    cmd_clean
    exit 0
    ;;
  *)
    echo "invalid input $optarg"
    help
    exit 1
    ;;
  esac
done

cmd_build
