FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# custom version modifier
UBOOT_LOCALVERSION = "-programmable-load"

# use a newer u-boot (v2022.07-rc4)
SRCREV = "8f527342db4160a1f030de6fe4a1591787cce65a"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=2ca5f2c35c8cc335f0a19756634782f1"

# extlinux config
# TODO: can we make this not hard-code the version? symlink support w/ erofs is broken
UBOOT_EXTLINUX_KERNEL_IMAGE = "/boot/zImage-5.15.45"
UBOOT_EXTLINUX_KERNEL_IMAGE_default = "/boot/zImage-5.15.45"

# patch to disable ADC in u-boot
SRC_URI:append:stm32mp1 = " \
	file://0002-stm32mp1-programmable-load.patch \
"
