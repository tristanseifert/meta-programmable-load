SUMMARY = "Bonus scripts for rootfs initialization"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

DEPENDS = "sativa-root-overlays"
RDEPENDS:${PN} = "sativa-root-overlays"

SRC_URI = "file://00-make-dirs.sh"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sbindir}/init-overlays.d/format-hooks
    install -m 0744 ${S}/00-make-dirs.sh ${D}/${sbindir}/init-overlays.d/format-hooks
}

FILES:${PN} += "/usr/sbin/init-overlays-format-hook"

