#!/bin/bash

set -ex

source ${DEB_BUILDER_DIR}/extensions/functions.sh

echo "Building stackdriver for gcp-php${SHORT_VERSION}"

PNAME="gcp-php${SHORT_VERSION}-stackdriver-trace"
# EXT_VERSION="0.1.0"
# PACKAGE_VERSION="${EXT_VERSION}-${PHP_VERSION}"
# PACKAGE_FULL_VERSION="${EXT_VERSION}-${FULL_VERSION}"
# PACKAGE_DIR=${PNAME}-${PACKAGE_VERSION}

BRANCH="trace-extension"

download_from_tarball https://github.com/chingor13/google-cloud-php/archive/${BRANCH}.tar.gz 0.1.0
PACKAGE_DIR=${PACKAGE_DIR}/src/Trace/ext

pwd
ls -al
build_package stackdriver_trace
