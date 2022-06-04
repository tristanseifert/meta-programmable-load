FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# custom version modifier
UBOOT_LOCALVERSION = "-programmable-load"

# use a newer u-boot (v2022.07-rc3)
SRCREV = "b6d46d951f1092f810e5d5971fb9a3dee8e87e86"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=2ca5f2c35c8cc335f0a19756634782f1"

# extlinux config
UBOOT_EXTLINUX_KERNEL_IMAGE = "/boot/zImage-5.15.44"
UBOOT_EXTLINUX_KERNEL_IMAGE_default = "/boot/zImage-5.15.44"

# patch to disable ADC in u-boot
SRC_URI:append:stm32mp1 = " \
	file://0002-stm32mp1-programmable-load.patch \
"
