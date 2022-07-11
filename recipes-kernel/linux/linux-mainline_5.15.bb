require linux-mainline-common.inc

# Linux 5.15.x, a longer-term release
LINUX_VERSION ?= "5.15.x"
KERNEL_VERSION_SANITY_SKIP="1"

BRANCH = "linux-5.15.y"

SRCREV = "${AUTOREV}"
SRC_URI = " \
    git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git;branch=${BRANCH} \
    file://0001-add-spidev-compatible.patch \
    file://0002-add-custom-lcd-panel.patch \
    file://filesystems.cfg \
    file://less-drivers.cfg \
    file://base.cfg \
    file://stm32mp1.cfg \
    file://programmable-load-rev3.cfg \
"

# custom device trees

# resource manager driver
SRC_URI += " \
    file://git/drivers/remoteproc/rproc_srm_dev.c \
    file://git/drivers/remoteproc/rproc_srm_core.h \
    file://git/drivers/remoteproc/rproc_srm_core.c \
    file://0003-add-resource-manager.patch \
    file://0004-add-resource-manager-probe.patch \
"
