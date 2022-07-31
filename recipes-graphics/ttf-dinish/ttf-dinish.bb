SUMMARY = "DINish fonts"
SECTION = "fonts"
HOMEPAGE = "https://github.com/playbeing/dinish"
LICENSE = "OFL-1.1"
LICENSE_URL = "http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&item_id=OFL"
LIC_FILES_CHKSUM = "file://OFL.txt;md5=5d78c340108d7c31a6083696ed33a70d"

# ensure the font cache gets updated
inherit allarch fontcache

# we don't need a compiler nor a c library for these fonts
INHIBIT_DEFAULT_DEPS = "1"

# font source is on github
SRCBRANCH ?= "main"
PV = "v3.002"

SRCREV_base = "62d0f29ba1ee9a7da11018ff16a4252669e39c97"

SRC_URI = "git://github.com/playbeing/dinish.git;protocol=https;branch=${SRCBRANCH};name=base"
# SRC_URI[sha256sum] = "d369db6e6e0ed5a669beec1790ba34c241d4c6db605d1594441c4a45ed3b404a"

# no compilation step needed (this would build fonts from source)
do_compile[noexec] = "1"

# install step: copy font files
S = "${WORKDIR}/git"

do_install() {
    # copy fonts

    # ensure font directory exists and fix permissions
    install -d ${D}${datadir}/fonts/truetype/
    find ./ -name '*.tt[cf]' -exec install -m 0644 {} ${D}${datadir}/fonts/truetype/ \;

    # copy license
    install -d ${D}${datadir}/doc/ttf-dinish/
    install -m 0644 ${S}/OFL.txt ${D}${datadir}/doc/ttf-dinish/
}

# package definitions
PACKAGES = "${PN}"
FONT_PACKAGES = "${PN}"

# define the files we secreted
FILES:${PN} = "\
    ${datadir}/fonts \
    ${datadir}/fonts/truetype \
    ${datadir}/fonts/truetype/DinishCondensed*.ttf \
    ${datadir}/fonts/truetype/DinishExpanded*.ttf \
    ${datadir}/fonts/truetype/Dinish-*.ttf \
    ${datadir}/doc \
    ${datadir}/doc/ttf-dinish/* \
"
