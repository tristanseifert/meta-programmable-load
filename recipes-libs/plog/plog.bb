SUMMARY="Portable, extensible logging library for C++"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE"

SRC_URI = "git://github.com/SergiusTheBest/plog;branch=master;protocol=https"
# 1.1.8
SRCREV = "89ac49396ae6978a056034d1e34bb170bfd3de33"

S = "${WORKDIR}/git"

inherit cmake
inherit ptest

EXTRA_OECMAKE += "-DPLOG_BUILD_SAMPLES=OFF"

BBCLASSEXTEND = "native nativesdk"
