#!/bin/sh

FILENAME="$1"
if [ "${FILENAME}" = "" ]; then
	exit 1
fi
cp ${FILENAME} ${FILENAME}.bak || exit 1

BUILDROOT="`pwd`"
SRCROOT="`pwd | xargs dirname | xargs dirname`"

sed -i.sedbackup "s#${BUILDROOT}#\${buildroot}#g" ${FILENAME} || exit 1
sed -i.sedbackup "s#${SRCROOT}#\${srcroot}#g" ${FILENAME} || exit 1
sed -i.sedbackup "s#^buildroot=	\${buildroot}#buildroot=	.#" ${FILENAME} || exit 1
sed -i.sedbackup "s#^srcroot=	\${srcroot}#srcroot=	../..#" ${FILENAME} || exit 1

rm ${FILENAME}.bak ${FILENAME}.sedbackup
