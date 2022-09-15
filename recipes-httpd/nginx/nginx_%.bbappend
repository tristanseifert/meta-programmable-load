FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

NGINX_USER = "www-data"

SRC_URI += "file://nginx.conf"
