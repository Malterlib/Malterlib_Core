#!/bin/bash
# Copyright © 2017 Nonna Holding AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: DeployFilesToVersionManager.sh Source RelativeDestination

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"
source ./BuildSystem/SharedBuildSettings.sh

echo Copying files

PackageDirectory="$TempDirectory/Package"

MTool BuildServerGet "Source=$1" "DestinationDir=$PackageDirectory"
CheckErrors

pushd "$PackageDirectory"

tar -czf "$TempDirectory/$2.tar.gz" .

UploadToVersionManagerWithInfo "$TempDirectory/$2.tar.gz" "$2" All

popd

echo 'Important: Build now available at on VersionManager(s)'
