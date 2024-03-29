SUMMARY = "Programmable load user interface"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "sativa-confd libcbor systemd libevent libdrm libpng freetype fontconfig\
    cairo harfbuzz pango fmt plog pl-common"
RDEPENDS:${PN} = "pl-app-meta pl-loadd libsystemd libdrm-kms ttf-dinish ttf-liberation-sans \
    ttf-liberation-mono ttf-liberation-serif pl-pinballd"

# package is built using CMake
SRC_URI = "\
    file://data/ \
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

USERADD_PARAM:${PN} = "-u 6900 -d /persistent/appdata/ui -s /bin/false -g load load-ui"
