#!/bin/bash

set -ex

source ${DEB_BUILDER_DIR}/extensions/functions.sh

echo "Building stackdriver for gcp-php${SHORT_VERSION}"

PNAME="gcp-php${SHORT_VERSION}-stackdriver"
EXT_VERSION="0.1.1"
PACKAGE_VERSION="${EXT_VERSION}-${PHP_VERSION}"
PACKAGE_FULL_VERSION="${EXT_VERSION}-${FULL_VERSION}"
PACKAGE_DIR=${PNAME}-${PACKAGE_VERSION}
cp -R ${DEB_BUILDER_DIR}/extensions/stackdriver/src/stackdriver/ext/stackdriver ${PACKAGE_DIR}
cp -R ${DEB_BUILDER_DIR}/extensions/stackdriver/debian ${PACKAGE_DIR}/

pwd
ls -al
build_package stackdriver
