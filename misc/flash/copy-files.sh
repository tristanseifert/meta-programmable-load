#!/bin/sh
set -e

# ensure argument was provided
if [ "$#" -ne 1 ] || ! [ -d "$1" ]; then
    echo "Usage: $0 DIRECTORY" >&2
    exit 1
fi

# copy u-boot
rm -f rootfs/u-boot-spl.stm32
cp "$1/tmp/deploy/images/load-rev3/u-boot-spl.stm32" rootfs/u-boot-spl.stm32

rm -f rootfs/u-boot.img
cp "$1/tmp/deploy/images/load-rev3/u-boot.img" rootfs/u-boot.img

# copy image
rm -r rootfs/root.erofs
dd if="$1/tmp/deploy/images/load-rev3/core-image-base-load-rev3.wic" of=rootfs/root.erofs bs=512 skip=12288
