SUMMARY = "Programmable load HMI daemon"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "pl-app-meta libcbor systemd git libevent i2c-tools libgpiod fmt plog"
RDEPENDS:${PN} = "libsystemd liblzma"

# define the CMake source directories
SRC_URI = "\
    file://lib/ \
    file://src/ \
    file://include/ \
    file://CMakeLists.txt \
"

# simply use cmake
S = "${WORKDIR}"

inherit pkgconfig cmake

# # install systemd unit
# REQUIRED_DISTRO_FEATURES= "systemd"
#
# inherit systemd features_check
#
# SYSTEMD_AUTO_ENABLE = "enable"
# SYSTEMD_SERVICE:${PN} = "confd.service"
#
# SRC_URI:append = " file://data/confd.service "
# FILES:${PN} += "${systemd_unitdir}/system/confd.service"
# 
# do_install:append() {
#     install -d ${D}/${systemd_unitdir}/system
#     install -m 0644 ${WORKDIR}/data/confd.service ${D}/${systemd_unitdir}/system
# }

# # install configuration files
# SRC_URI:append = " file://data/confd.toml "
# FILES:${PN} += "/usr/etc/confd.toml"
#
# do_install:append() {
#     install -d ${D}/usr/etc
#     install -m 0644 ${WORKDIR}/data/confd.toml ${D}/usr/etc
# }
#

