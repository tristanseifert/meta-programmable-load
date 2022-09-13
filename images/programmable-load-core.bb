SUMMARY = "Imager supporting all of the programmable load hardware."

IMAGE_FEATURES += "splash"

LICENSE = "MIT"

# add the programmable load utilities
CORE_IMAGE_EXTRA_INSTALL += "pl-app-meta pl-loadd pl-pinballd pl-gui "

# apply various bonus config files to customize Sativa behavior
CORE_IMAGE_EXTRA_INSTALL += "pl-rootfs-overlays pl-confd-config "

# servers
CORE_IMAGE_EXTRA_INSTALL += "lighttpd "

inherit core-image
