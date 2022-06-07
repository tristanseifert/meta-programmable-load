SUMMARY = "Enable systemd watchdog"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

SRC_URI = "file://watchdog.conf"

S = "${WORKDIR}"

# copy into sytemd config
do_install() {
    install -d ${D}/etc/systemd/system.conf.d/
    install -m 0644 ${S}/watchdog.conf ${D}/etc/systemd/system.conf.d/
}

# export that we created the data fs mountpoints
FILES:${PN} += "/etc/systemd/system.conf.d/watchdog.conf"
