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

# servers
IMAGE_INSTALL:append = " \
    lighttpd \
"

# tools
IMAGE_INSTALL:append = " \
    curl u-boot-env \
"

# OpenAMP (for coprocessor communication)
PREFERRED_PROVIDER_libmetal="libmetal"
IMAGE_INSTALL:append = " \
    open-amp \
"

# u-boot support
IMAGE_INSTALL:append = " libubootenv libubootenv-bin "

# various generally useful libraries
IMAGE_INSTALL:append = " \
    libjpeg-turbo libpng libusb1 libsocketcan \
"

# UI libraries
# boot-splash: Custom bootup splash screen
# TODO: define custom modes for LCD (fbset-modes overlay)
# cairo: generic graphics library
# freetype: TTF rendering
# harfbuzz: Text shaping engine
# pango: Text rendering engine
# qrencode: rendering of qr codes
IMAGE_INSTALL:append = " \
    boot-splash \
    cairo freetype harfbuzz pango \
    qrencode \
"

# fonts for GUI
# TODO: pare down which ones we'll actually use
# gentium: serif-y
# liberation: font family (sans, sans narrow, serif, mono)
# inconsolata: monospaced font
# dinish: DIN-style font
IMAGE_INSTALL:append = " \
    ttf-gentium \
    ttf-liberation-sans ttf-liberation-sans-narrow ttf-liberation-serif ttf-liberation-mono \
    ttf-inconsolata \
    ttf-dinish \
"

##### custom programmable load stuff
# root fs overlays (for app data)
IMAGE_INSTALL:append = " pl-root-overlays"
# general programmable load firmware stuff
IMAGE_INSTALL:append = " pl-app-meta pl-confd"
# real-time control firmware

# user interface and remote
IMAGE_INSTALL:append = " pl-pinballd"
