# Flash Configuration
Contained here are the [flash layout files (.tsv)](https://wiki.st.com/stm32mpu/wiki/STM32CubeProgrammer_flashlayout) which can be used by the STM32MP1 tools to program the eMMC on the board.

## Images
The following flash layouts are provided:

- emmc-4gb.tsv: For a 4GByte eMMC, with redundant rootfs partitions.

## Binaries
Most binaries that will go into the eMMC will be copied into the `rootfs` directory by the `copy-files.sh` script here, given the path to the Yocto work directory. However, the `runtime` directory contains firmware loaded using DFU mode to enable the download -- these are simply copied directly out of the BSP for the SoM we're using, so we needn't bother with building TF-A.
