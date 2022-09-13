SUMMARY = "Programmable load specific configuration for confd"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

RDEPENDS:${PN} = "sativa-confd"

SRC_URI = "file://permissions.toml"

S = "${WORKDIR}"

do_install() {
    install -d ${D}/usr/etc/confd.d
    install -m 0644 ${S}/permissions.toml ${D}/usr/etc/confd.d
}

FILES:${PN} += "/usr/etc/confd.d"
