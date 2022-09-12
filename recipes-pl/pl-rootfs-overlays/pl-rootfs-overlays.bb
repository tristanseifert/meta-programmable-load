SUMMARY = "Bonus scripts for rootfs initialization"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

RDEPENDS:${PN} = "sativa-root-overlays"

SRC_URI = "file://init-overlays-format-hook"

S = "${WORKDIR}"

do_install() {
	install -d ${D}${sbindir}
	install -m 0755 ${S}/init-overlays-format-hook ${D}/${sbindir}
}

FILES:${PN} += "/usr/sbin/init-overlays-format-hook"

