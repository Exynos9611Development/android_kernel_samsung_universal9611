#/bin/bash
set -e

[ ! -e "scripts/packaging/pack.sh" ] && git submodule init && git submodule update
[ ! -d "toolchain" ] && echo "Make toolchain avaliable at $(pwd)/toolchain" && exit

export KBUILD_BUILD_USER=Royna
export KBUILD_BUILD_HOST=GrassLand

PATH=$PWD/toolchain/bin:$PATH

if [ "$1" = "oneui" ]; then
FLAGS=ONEUI=1
else
CONFIG_AOSP=vendor/aosp.config
fi

if [ -z "$DEVICE" ]; then
export DEVICE=a51
fi

rm -rf out

COMMON_FLAGS=" LLVM=1 -j$(nproc)"

make O=out $COMMON_FLAGS vendor/${DEVICE}_defconfig vendor/grass.config vendor/${DEVICE}.config vendor/ksu.config $CONFIG_AOSP
make O=out $COMMON_FLAGS ${FLAGS} -j$(nproc)
