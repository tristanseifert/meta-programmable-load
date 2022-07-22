SUMMARY = "Programmable load app meta-module"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"

S = "${WORKDIR}"

ALLOW_EMPTY:${PN} = "1"

# create groups + users
inherit useradd
USERADD_PACKAGES = "${PN}"

GROUPADD_PARAM:${PN} = "-g 7420 load"

USERADD_PARAM:${PN} = "-u 6901 -d /persistent/appdata/remote -s /bin/false -g load load-remote"
