FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

inherit update-rc.d systemd

# enable php-fpm service
SYSTEMD_SERVICE:${PN} += "php-fpm.service"
SYSTEMD_AUTO_ENABLE = "enable"

# install php-fpm init script
SRC_URI += "file://php-fpm-helper.sh\
    file://php-fpm.service\
    file://php-fpm.conf"

do_install:append() {
    install -d ${D}${sysconfdir}/php
    install -m 0755 ${WORKDIR}/php-fpm-helper.sh ${D}${sysconfdir}/php
}

FILES:${PN} += "${sysconfdir}/php/php-fpm-helper.sh"
