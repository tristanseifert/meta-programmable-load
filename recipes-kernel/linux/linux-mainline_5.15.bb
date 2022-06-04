require linux-mainline-common.inc

# Linux 5.15.x, a longer-term release
LINUX_VERSION ?= "5.15.x"
KERNEL_VERSION_SANITY_SKIP="1"

BRANCH = "linux-5.15.y"

SRCREV = "${AUTOREV}"
SRC_URI = " \
    git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git;branch=${BRANCH} \
    file://overlay.cfg \
    file://erofs.cfg \
    file://f2fs.cfg \
    file://less-drivers.cfg \
"
