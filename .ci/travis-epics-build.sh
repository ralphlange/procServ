#!/usr/bin/env sh
set -e -x

# Build procServ using EPICS build system

. "./.env"

mkdir procServApp
ls | grep -v procServApp | xargs mv -t procServApp

EPICS_HOST_ARCH=`${EPICS_BASE}/startup/EpicsHostArch`

${EPICS_BASE}/bin/${EPICS_HOST_ARCH}/makeBaseApp.pl -t example dummy
rm -fr dummyApp

cat > configure/RELEASE.local << EOF
EPICS_BASE=${EPICS_BASE}
EOF

( cd procServApp; make; ./configure --with-epics-top=.. --disable-doc )

make
