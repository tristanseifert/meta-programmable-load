SUMMARY = "Programmable load daemon"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "pl-app-meta libcbor systemd git-native libevent"
RDEPENDS:${PN} = "libsystemd"

# package is built using CMake
SRC_URI = "\
    file://lib/ \
    file://src/ \
    file://include/ \
    file://CMakeLists.txt \
"
S = "${WORKDIR}"

inherit pkgconfig cmake

# create users
inherit useradd
USERADD_PACKAGES = "${PN}"

USERADD_PARAM:${PN} = "-u 6910 -d /persistent/appdata/control -s /bin/false -g daemon -G load loadd"
