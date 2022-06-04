SUMMARY = "Read-only rootfs overlay mounter"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

SRC_URI = "file://init-overlays"

S = "${WORKDIR}"

# copy the shell script
do_install() {
	# script
	install -d ${D}${sbindir}
	install -m 0755 ${S}/init-overlays ${D}/${sbindir}
	
	# overlay mount points
	install -d ${D}/persistent
}

# export that we created the data fs mountpoints
FILES:${PN} += "/persistent"

# ensure the uboot config uses this as the new init
UBOOT_EXTLINUX_KERNEL_ARGS += "init=/usr/sbin/init-overlays"
