SUMMARY = "Imager supporting all of the programmable load hardware."

IMAGE_FEATURES += "splash"

LICENSE = "MIT"

# add the programmable load utilities
CORE_IMAGE_EXTRA_INSTALL += "pl-app-meta pl-loadd pl-pinballd pl-gui"

inherit core-image
