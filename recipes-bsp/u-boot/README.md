# U-Boot
This recipe provides the custom U-Boot builds needed for the programmable load controller. Mainly, this handles getting the device tree into the build, but also adds a few odds and ends to enable basic interfacing to the front panel LEDs and buttons.

## Device tree remark
Note that several device tree files are copies of their Linux kernel equivalents. They are hardlinked here, but git may not pick up on this; the following files need to be _exactly_ the same as their kernel counterparts:

- `arch/arm/boot/dts/stm32mp15xa.dtsi`
- `arch/arm/boot/dts/stm32mp15-m4-srm.dtsi`
- `arch/arm/boot/dts/stm32mp151a-programmable-load-myir-mx.dts`
- `include/dt-bindings/pinctrl/stm32-pinfunc-2.h`
