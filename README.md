# Programmable load base layer
The lowest level Yocto layer for the programmable load system image.

This provides stuff like device trees, hardware definitions, and bootloaders, as well as machine definitions.

## Dependencies
Other layers required to build the programmable load are:

- meta-openembedded: specifically, meta-oe and meta-filesystems
- meta-openamp
- meta-qt6
- meta-rauc

## Build configuration
Configure your `build/conf/local.conf` to use one of the following machines:

- stm32mp157a-dk1: STM32MP1 development board
