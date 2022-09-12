FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# custom version modifier
UBOOT_LOCALVERSION = "-programmable-load"

# patch to disable ADC in u-boot
SRC_URI:append:stm32mp1 = " \
    file://0002-stm32mp1-programmable-load.patch \
    file://0003-programmable-load-dts.patch \
    file://0004-add-mach-stm32mp-sources.patch \
    file://git/arch/arm/dts/stm32mp151a-programmable-load-myir-mx.dts \
    file://git/arch/arm/dts/stm32mp151a-programmable-load-myir-mx-u-boot.dtsi \
    file://git/arch/arm/mach-stm32mp/conf_prom.c \
    file://git/arch/arm/mach-stm32mp/conf_prom.h \
    file://git/arch/arm/mach-stm32mp/encoding_helpers.c \
    file://git/arch/arm/mach-stm32mp/encoding_helpers.h \
    file://git/arch/arm/mach-stm32mp/hash_helpers.c \
    file://git/arch/arm/mach-stm32mp/hash_helpers.h \
"
