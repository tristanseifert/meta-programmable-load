# Linux Kernel
This recipe is responsible for building the Linux kernel. By default, this will pull the latest 5.15.x release (long-term support) and build it from source, with a custom configuration that omits many kernel modules and features we don't really need. It's implemented in two parts:

## Appends
The `.bbappend` file is responsible for bringing in all of our custom device trees into the kernel. The following modifications are of note:

- `arch/arm/boot/dts/stm32mp15-m4-srm.dts`: Allocations for resources usable by the M4
- `arch/arm/boot/dts/stm32mp151xa.dts`: Included by STM32CubeMX device trees; originally contains the frequency scaling data, but we don't use that so it's empty.
- `include/dt-bindings/pinctrl/stm32-pinfunc-2.h`: The upstream file (`stm32-pinfunc.h`) does not define the `RSVD` constant, used for M4 pin reservations. It's easier to just rename this (and then edit our device tree to refer to it) than to get it to be replaced properly by the build system…

## Recipe Config
On the other hand, the regular `.bb` file defines the release version to fetch, as well as what kernel configuration to apply.

