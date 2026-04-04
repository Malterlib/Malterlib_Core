#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: DeployFilesToVersionManager.sh Source RelativeDestination

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"
source ./BuildSystem/SharedBuildSettings.sh

echo Copying files

PackageDirectory="$TempDirectory/Package"

MTool BuildServerGet "Source=$1" "DestinationDir=$PackageDirectory"

pushd "$PackageDirectory"

tar -czf "$TempDirectory/$2.tar.gz" .

UploadToVersionManagerWithInfo "$TempDirectory/$2.tar.gz" "$2" All

popd

echo 'VeryImportant: Build now available at on VersionManager(s)'
