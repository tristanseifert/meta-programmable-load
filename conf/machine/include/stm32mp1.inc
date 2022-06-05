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
	opengl \
	vulkan \
	cramfs \
	alsa \
	x11 \
"

# servers
IMAGE_INSTALL:append = " \
	dbus lighttpd \
"
# openssh-sftp-server

# tools
# f2fs-tools: create f2fs user data partition
# gptfdisk: sgdisk, used to set up partition table (create user data partition)
# util-linux: Various linux-related utilities, to force re-reading partition tables
IMAGE_INSTALL:append = " \
	curl i2c-tools mmc-utils u-boot-env f2fs-tools gptfdisk util-linux \
"

# OpenAMP (for coprocessor communication)
PREFERRED_PROVIDER_libmetal="libmetal"
IMAGE_INSTALL:append = " \
	open-amp \
"

# various generally useful libraries
IMAGE_INSTALL:append = " \
	bzip2 libdaemon libevent libevdev libjpeg-turbo libpng libubootenv libusb1 libxml2 \
	libyaml readline sqlite3 libsocketcan \
"

# UI libraries
# boot-splash: Custom bootup splash screen
# libdrm: Accessing the DRM video drivers' API
# fbset + fbset-modes: utility to change video modes (plus mode database)
# TODO: define custom modes for LCD
# cairo: generic graphics library
# freetype: TTF rendering
# harfbuzz: Text shaping engine
# pango: Text rendering engine
# qrencode: rendering of qr codes
IMAGE_INSTALL:append = " \
	boot-splash \
	libdrm\
	fbset fbset-modes \
	cairo freetype harfbuzz pango \
	qrencode \
"

# fonts for GUI
# TODO: pare down which ones we'll actually use
# gentium: serif-y
# liberation: font family (sans, sans narrow, serif, mono)
# inconsolata: monospaced font
# dejavu: dejavu font family (sans, serif, mono)
IMAGE_INSTALL:append = " \
	ttf-gentium \
	ttf-liberation-sans ttf-liberation-sans-narrow ttf-liberation-serif ttf-liberation-mono \
	ttf-inconsolata \
"
# ttf-dejavu-sans ttf-dejavu-sans-condensed ttf-dejavu-serif ttf-dejavu-sans-mono

# custom programmable load stuff
IMAGE_INSTALL:append = " pl-root-overlays"
