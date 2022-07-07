SUMMARY = "Programmable load app meta-module"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

SRC_URI = "file://test.txt"

S = "${WORKDIR}"

# install our data files
do_install() {
    # root directory for programmable load firmware
    install -d -g load ${D}app/

    # test file
    install -m 0644 ${S}/test.txt ${D}app/
}

# export our created files
FILES:${PN} += "/app/test.txt"
ALLOW_EMPTY:${PN} = "1"

# create groups + users
inherit useradd
USERADD_PACKAGES = "${PN}"

GROUPADD_PARAM:${PN} = "-g 7420 load"

USERADD_PARAM:${PN} = "-u 6900 -d /persistent/appdata/ui -s /bin/false -g load load-ui;\
    -u 6901 -d /persistent/appdata/remote -s /bin/false -g load load-remote\
"
