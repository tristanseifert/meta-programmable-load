# SoC tune
require conf/machine/include/soc-family.inc
SOC_FAMILY = "stm32mp1"

# CPU type
DEFAULTTUNE = "cortexa7thf-neon-vfpv4"
require conf/machine/include/arm/armv7a/tune-cortexa7.inc

# use mainline kernel
PREFERRED_PROVIDER_virtual/kernel ??= "linux-mainline"

# configure uboot entry
UBOOT_SUFFIX = "img"
UBOOT_ENTRYPOINT = "0xC2000040"

# serial console
SERIAL_CONSOLES = "115200;ttySTM0"
SERIAL_CONSOLES_CHECK = "${SERIAL_CONSOLES}"

KERNEL_IMAGETYPE ?= "zImage"

# Do not update fstab file when using wic images
WIC_CREATE_EXTRA_ARGS ?= "--no-fstab-update"

# Image
# IMAGE_FSTYPES += "tar.bz2 wic wic.gz wic.bmap"
IMAGE_FSTYPES += "wic wic.gz wic.bmap"

WKS_FILE_DEPENDS ?= " \
    virtual/bootloader \
    erofs-utils-native \
    bmap-tools-native \
"

# Wic default support
WKS_FILE ?= "stm32mp1-sdcard.wks"

# log files get persisted to data partition
VOLATILE_LOG_DIR = "false"

# Use systemd as init system
VIRTUAL-RUNTIME_init_manager = "systemd"
DISTRO_FEATURES_BACKFILL_CONSIDERED = "sysvinit"
VIRTUAL-RUNTIME_initscripts = ""
DISTRO_FEATURES:append = " systemd"

DISTRO_FEATURES:remove = " \
	wayland \
	irda \
	ppp \
	pcmcia \
	smbfs \
	pci \
	3g \
	nfc \
	efi \
	ntfs \
	wifi \
	bluetooth \
"

DISTRO_FEATURES:append = " directfb"

# servers
IMAGE_INSTALL:append = " \
	openssh-sftp-server dbus lighttpd \
"

# tools
# f2fs-tools: create f2fs user data partition
# gptfdisk: sgdisk, used to set up partition table (create user data partition)
# util-linux: Various linux-related utilities, to force re-reading partition tables
IMAGE_INSTALL:append = " \
	curl i2c-tools mmc-utils u-boot-env \
    f2fs-tools gptfdisk util-linux \
"

# software components
IMAGE_INSTALL:append = " \
	open-amp \
"

# libraries (at runtime)
IMAGE_INSTALL:append = " \
	bzip2 libdaemon libevent libevdev libjpeg-turbo libpng libubootenv libusb1 libxml2 \
	libyaml readline sqlite3 \
"

# Qt and friends (for UI layer)
# IMAGE_INSTALL:append = " packagegroup-qt6-essentials qtvirtualkeyboard qtcharts"

# force add the SSH server
EXTRA_IMAGE_FEATURES:append = " \
	ssh-server-dropbear \
"

# custom programmable load stuff
IMAGE_INSTALL:append = " pl-root-overlays"
