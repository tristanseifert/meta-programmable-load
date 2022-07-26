SUMMARY = "Programmable load HMI daemon"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "libcbor systemd git libevent i2c-tools libgpiod fmt plog"
RDEPENDS:${PN} = "pl-app-meta libsystemd liblzma udev"

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

# create the pinballd user
inherit useradd
USERADD_PACKAGES = "${PN}"

USERADD_PARAM:${PN} = "-u 6911 -d /persistent/appdata/ui -s /bin/false -g load pinballd"

# TODO: base this on hw revision!
# install an udev rule (to allow the pinballd user to access hw)
SRC_URI:append = " file://data/udev.rules "
FILES:${PN} += " ${sysconfdir}/udev/rules.d/pinballd.rules "

do_install:append() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/data/udev.rules ${D}${sysconfdir}/udev/rules.d/pinballd.rules
}

# install and enable systemd unit
inherit systemd features_check
REQUIRED_DISTRO_FEATURES= "systemd"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "pinballd.service"

SRC_URI:append = " file://data/pinballd.service "
FILES:${PN} += "${systemd_unitdir}/system/pinballd.service"

do_install:append() {
    install -d ${D}/${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/data/pinballd.service ${D}/${systemd_unitdir}/system
}

# # install configuration files
# SRC_URI:append = " file://data/confd.toml "
# FILES:${PN} += "/usr/etc/confd.toml"
#
# do_install:append() {
#     install -d ${D}/usr/etc
#     install -m 0644 ${WORKDIR}/data/confd.toml ${D}/usr/etc
# }
#

