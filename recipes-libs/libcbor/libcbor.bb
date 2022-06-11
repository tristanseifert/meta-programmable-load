SUMMARY = "Simple CBOR library for C/C++"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE.md;md5=6f3b3881df62ca763a02d359a6e94071"
# fetch the library from git
SRCBRANCH ?= "master"
PV = "v0.9.0"
SRCREV_base = "25d86a7a30dbee76defc2eb79cf751a1749b4e81"

SRC_URI = "git://github.com/PJK/libcbor.git;protocol=https;branch=${SRCBRANCH};name=base"

# build it as cmake
S = "${WORKDIR}/git"

inherit pkgconfig cmake
