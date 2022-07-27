SUMMARY = "Programmable load common code"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
DEPENDS = "libcbor systemd git libevent fmt plog"
RDEPENDS:${PN} = "libsystemd liblzma"

# define the CMake source directories
SRC_URI = "\
    file://src/ \
    file://include/ \
    file://CMakeLists.txt \
"

# simply use cmake
S = "${WORKDIR}"

inherit pkgconfig cmake
