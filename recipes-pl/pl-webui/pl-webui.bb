SUMMARY = "Coordinator management web interface"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/ISC;md5=f3b90e78ea0cffb20bf5cca7947a896d"
PR = "r0"
PV = "1.0+git${SRCPV}"

# runtime dependencies
DEPENDS = "sativa-confd sqlite3 pl-app-meta "
RDEPENDS:${PN} = "sativa-confd php php-cli php-opcache php-fpm nginx"

# build time dependencies
DEPENDS += "nodejs-native php-native"
# RDEPENDS:${PN} = "nodejs-npm-native"

# fetch the sources via Git
SRCBRANCH ?= "main"
SRCREV_base = "${AUTOREV}"

# TODO: fetch the proper sources for programmable load app
SRC_URI = "git://github.com/tristanseifert/blazenet-coordinator.git;protocol=https;branch=${SRCBRANCH};name=base"

SRC_URI += "file://.env\
    file://10-init-webui.sh\
    file://webui-nginx.conf\
"

S = "${WORKDIR}/git/webui"
B = "${S}"

# important: allow configure step to access network (for NPM)
# this can produce somewhat undeterministic builds (since node/composer versions aren't locked
# precisely here) but it's the only way to make this work that's not a huge pain in the ass :)
do_configure[network] = "1"
# configure step: fetch Node modules and PHP composer packages
do_configure () {
    # do the node stuff
    npm install --with=dev --no-audit

    php --ini

    # fetch and install composer (to a local temp directory)
    mkdir -p ${WORKDIR}/_bin
    php -r "copy('https://getcomposer.org/installer', 'composer-setup.php');"
    php -r "if (hash_file('sha384', 'composer-setup.php') === '55ce33d7678c5a611085589f1f3ddf8b3c52d662cd01d4ba75c0ee0459970c2200a51f492d557530c71c15d8dba01eae') { echo 'Installer verified'; } else { echo 'Installer corrupt'; unlink('composer-setup.php'); exit(-1); } echo PHP_EOL;"
    php composer-setup.php --install-dir ${WORKDIR}/_bin
    php -r "unlink('composer-setup.php');"

    # install composer packages
    ${WORKDIR}/_bin/composer.phar install
}

# compilation step: build the application's resources and produce optimized autoload
do_compile () {
    # compile assets
    npm run production

    # optimize autoloads
    ${WORKDIR}/_bin/composer.phar install --optimize-autoloader --no-dev
}

# installation step: copy the necessary files to the webroot
WWW = "${D}/usr/local/www/webui/"

do_install () {
    install -d ${WWW}

    # copy all parts of the app that we actually need
    cp -r ${S}/app ${WWW}
    cp -r ${S}/bootstrap ${WWW}
    cp -r ${S}/config ${WWW}
    cp -r ${S}/database ${WWW}
    cp -r ${S}/lang ${WWW}
    cp -r ${S}/public ${WWW}
    cp -r ${S}/resources ${WWW}
    cp -r ${S}/routes ${WWW}
    cp -r ${S}/storage ${WWW}
    cp -r ${S}/vendor ${WWW}

    cp -r ${S}/artisan ${WWW}
    cp -r ${S}/composer.json ${WWW}

    # apply permissions
    chown -R load-remote ${WWW}
    chgrp -R load ${WWW}
    chmod 755 ${WWW}

    # copy configuration
    install -m 640 -o load-remote -g load ${WORKDIR}/.env ${WWW}

    # do some final setup
    cd ${WWW}

    cd public
    ln -s ../storage/app/public
    chown load-remote public
    chgrp load public
    cd ..

    # update caches
    # php artisan config:cache
    # php artisan route:cache
    # php artisan view:cache
}
FILES:${PN} += "/usr/local/www/webui"

# install an app data partition formatting hook (fill in db, generate app secret)
do_install:append () {
    install -d ${D}${sbindir}/init-overlays.d/format-hooks
    install -m 0744 ${WORKDIR}/10-init-webui.sh ${D}/${sbindir}/init-overlays.d/format-hooks
}

# install nginx config
do_install:append () {
    install -d ${D}${sysconfdir}/nginx/conf.d
    install -m 0444 ${WORKDIR}/webui-nginx.conf ${D}${sysconfdir}/nginx/conf.d
}
