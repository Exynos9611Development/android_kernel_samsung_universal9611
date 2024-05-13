#!/bin/bash
[ ! -e "KernelSU/kernel/setup.sh" ] && git submodule init && git submodule update
[ ! -d "toolchain" ] && echo "Toolchain not avaliable at $(pwd)/toolchain... installing toolchain" && bash setup.sh

export KBUILD_BUILD_USER=Royna
export KBUILD_BUILD_HOST=GrassLand
export LLVM=1

PATH=$PWD/toolchain/bin:$PATH
export LLVM_DIR=$PWD/toolchain/bin
export kerneldir=$PWD
export ANYKERNEL=$PWD/scripts/packaging/AnyKernel3
export TIME="$(date "+%Y%m%d")"

if [[ -z "$ROM" || "$ROM" = "aosp" ]]; then
CONFIG_AOSP=vendor/aosp.config
SUFFIX=AOSP
elif [ "$ROM" = "oneui" ]; then
FLAGS=ONEUI=1
SUFFIX=ONEUI
else
echo "Error: Set ROM to aosp or oneui to build"
fi
export SUFFIX

if [ -z "$DEVICE" ]; then
export DEVICE=m21
fi

if [[ -z "$KSU" || "$KSU" = "0" ]]; then
KSU=0
export KSUSTAT=_
elif [ "$KSU" = "1" ]; then
CONFIG_KSU=vendor/ksu.config
export KSUSTAT=-KSU_
else
echo "Error: Set KSU to 0 or 1 to build"
exit 1
fi
export KSU

if [[ -z "$SELINUX" || "$SELINUX" = "e" ]]; then
SELINUX=e
export SESTAT=E
elif [ "$SELINUX" = "p" ]; then
CONFIG_SELINUX=vendor/permissive.config
export SESTAT=P
else
echo "Error: Set SLINUX to e or p to build."
exit 1
fi
export SELINUX

rm -rf out

COMMON_FLAGS='CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi CC=clang LD=ld.lld ARCH=arm64 LLVM=1 AR='${LLVM_DIR}/llvm-ar' NM='${LLVM_DIR}/llvm-nm' AS='${LLVM_DIR}/llvm-as' OBJCOPY='${LLVM_DIR}/llvm-objcopy' OBJDUMP='${LLVM_DIR}/llvm-objdump' READELF='${LLVM_DIR}/llvm-readelf' OBJSIZE='${LLVM_DIR}/llvm-size' STRIP='${LLVM_DIR}/llvm-strip' LLVM_AR='${LLVM_DIR}/llvm-ar' LLVM_DIS='${LLVM_DIR}/llvm-dis' LLVM_NM='${LLVM_DIR}/llvm-nm''

mkdir -p kernel_zip
make O=out $COMMON_FLAGS vendor/${DEVICE}_defconfig vendor/grass.config vendor/${DEVICE}.config $CONFIG_AOSP $CONFIG_KSU $CONFIG_SELINUX
clear
echo ""
echo " -----------------"
echo "  BUILD CONFIG"
echo " -----------------"
echo "  $DEVICE"
echo "  $SUFFIX"
echo "  KSU=$KSU"
echo "  SELINUX=$SELINUX"
echo " -----------------"
echo ""

make O=out $COMMON_FLAGS ${FLAGS} -j$(nproc)

echo "  Cleaning Stuff"
rm -rf ${ANYKERNEL}/Image
rm -rf ${ANYKERNEL}/config
echo "  done"
echo ""
echo "  Copying Stuff"

cp -r out/arch/arm64/boot/Image ${ANYKERNEL}/Image
cp -r out/.config ${ANYKERNEL}/config
echo "  done"
echo ""
echo "  Zipping Stuff"
cd ${ANYKERNEL}
rm -rf GRASS*.zip
zip -r9 kernel.zip * -x .git README.md *placeholder
mv kernel.zip GRASS-${SUFFIX}-${DEVICE}${KSUSTAT}${SESTAT}_${TIME}.zip
cd ${kerneldir}
cp ${ANYKERNEL}/GRASS*.zip kernel_zip/
echo "  Kernel zip in ${kerneldir}/kernel_zip"