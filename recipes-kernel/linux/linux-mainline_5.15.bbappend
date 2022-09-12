FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# install custom device trees
SRC_URI += "\
    file://git/arch/arm/boot/dts/stm32mp151a-programmable-load-myir-mx.dts\
"

# driver changes
SRC_URI += " \
    file://0001-add-spidev-compatible.patch \
    file://0002-add-custom-lcd-panel.patch \
    file://stm32mp1.cfg \
    file://programmable-load-rev3.cfg \
"
