#!/bin/sh
set -e

/bin/mkdir -p /persistent/appdata/control
/bin/mkdir -p /persistent/appdata/remote
/bin/mkdir -p /persistent/appdata/ui
/bin/mkdir -p /persistent/config
/bin/mkdir -p /persistent/logs
/bin/mkdir -p /persistent/logs/www
/bin/mkdir -p /persistent/logs/system
/bin/mkdir -p /persistent/overlay/.var-log-work

# apply permissions
/bin/chown -R load-fw /persistent/appdata/control
/bin/chown -R load-remote /persistent/appdata/remote
/bin/chown -R load-ui /persistent/appdata/ui

/bin/chgrp -R load /persistent/appdata
/bin/chgrp -R load /persistent/config

# for confd
/bin/mkdir -p /persistent/config/confd-data/
/bin/chown -R confd:daemon /persistent/config/confd-data
/bin/chmod -R 700 /persistent/config/confd-data
