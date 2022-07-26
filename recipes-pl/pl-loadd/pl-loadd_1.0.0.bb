SUMMARY = "Programmable load daemon"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "libcbor systemd git-native libevent fmt plog"
RDEPENDS:${PN} = "pl-app-meta libsystemd"

# package is built using CMake
SRC_URI = "\
    file://lib/ \
    file://src/ \
    file://include/ \
    file://CMakeLists.txt \
"
S = "${WORKDIR}"

inherit pkgconfig cmake

# install an udev rule (to allow the loadd user to access hw)
SRC_URI:append = " file://data/udev.rules "
FILES:${PN} += " ${sysconfdir}/udev/rules.d/loadd.rules "

do_install:append() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/data/udev.rules ${D}${sysconfdir}/udev/rules.d/loadd.rules
}

# create users
inherit useradd
USERADD_PACKAGES = "${PN}"

USERADD_PARAM:${PN} = "-u 6910 -d /persistent/appdata/control -s /bin/false -g daemon -G load loadd"
