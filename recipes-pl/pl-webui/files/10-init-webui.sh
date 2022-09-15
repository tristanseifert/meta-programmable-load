#!/bin/sh

# create storage directories
mkdir -p /persistent/overlay/webui-storage
mkdir -p /persistent/overlay/.webui-storage-work

# update db
cd /usr/local/www/webui

php artisan migrate --seed --force

# TODO: update app key

# fix up permissions
chown -R load-remote /persistent/appdata/remote
chgrp -R load /persistent/appdata/remote
