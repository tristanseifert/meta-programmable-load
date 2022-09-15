# add an overlayfs on the webui storage directory
do_install:append () {
    echo "none  /usr/local/www/webui/storage    overlay lowerdir=/usr/local/www/webui/storage/,upperdir=/persistent/overlay/webui-storage,workdir=/persistent/overlay/.webui-storage-work" >> ${D}${sysconfdir}/fstab
}
