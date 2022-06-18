# Miscellaneous Stuff
In this directory are a few miscellaneous utilities that make dealing with the programmable load firmware/hardware less shitty. These are intended to exist outside the typical Yocto build process.

## Utilities
- `split-image.sh`: Given a binary `.wic` file produced with the eMMC output script, split the image into the individual constituent partitions. The script will write the partitions to the `flash` directory.

## Bonus Content
- `flash`: Contains flash layout files to be used with the [STM32CubeProgrammer](https://wiki.st.com/stm32mpu/wiki/STM32CubeProgrammer) software on the host machine, to program the eMMC of a board. See the `README.md` inside that directory for detailed instructions.
