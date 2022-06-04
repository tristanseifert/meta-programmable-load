FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

# ensure we also enable the "partprobe" command
SRC_URI:append = "\
	file://partprobe.cfg \
"
