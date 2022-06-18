FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# custom version modifier
UBOOT_LOCALVERSION = "-programmable-load"

# use a newer u-boot (v2022.07-rc4)
SRCREV = "8f527342db4160a1f030de6fe4a1591787cce65a"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=2ca5f2c35c8cc335f0a19756634782f1"

# extlinux config
# TODO: can we make this not hard-code the version? symlink support w/ erofs is broken
UBOOT_EXTLINUX_KERNEL_IMAGE = "/boot/zImage-5.15.46"
UBOOT_EXTLINUX_KERNEL_IMAGE_default = "/boot/zImage-5.15.46"

# patch to disable ADC in u-boot
SRC_URI:append:stm32mp1 = " \
    file://0002-stm32mp1-programmable-load.patch \
    file://0003-programmable-load-dts.patch \
    file://git/arch/arm/dts/stm32mp151a-programmable-load-myir-mx.dts \
    file://git/arch/arm/dts/stm32mp151a-programmable-load-myir-mx-u-boot.dtsi \
    file://git/arch/arm/dts/stm32mp15-mx.dtsi \
    file://git/arch/arm/dts/stm32mp15xa.dtsi \
    file://git/arch/arm/dts/stm32mp15-m4-srm.dtsi \
    file://git/include/dt-bindings/pinctrl/stm32-pinfunc-2.h \
"
