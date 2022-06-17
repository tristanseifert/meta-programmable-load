# install custom device trees
SRC_URI += "\
    file://git/arch/arm/boot/dts/stm32mp15xa.dtsi\
    file://git/arch/arm/boot/dts/stm32mp15-m4-srm.dtsi\
    file://git/arch/arm/boot/dts/stm32mp151a-programmable-load-myir-mx.dts\
    file://git/include/dt-bindings/pinctrl/stm32-pinfunc-2.h\
"

