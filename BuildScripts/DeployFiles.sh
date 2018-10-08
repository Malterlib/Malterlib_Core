#!/bin/bash
# Copyright © 2015 Hansoft AB 
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: DeployFiles.sh Source RelativeDestination

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

TestPath=$(TempPath)/Tests

echo TestPath=$TestPath

if [ "$3" == "WithVersion" ]; then
	DestinationDir="${MalterlibSharedDeployRoot}/Development/Installers/$2/${MalterlibFullBranchOnlyLast}/${ProductVersionStringReadable}$4"
else
	DestinationDir="${MalterlibSharedDeployRoot}/Development/Installers/$2/${MalterlibFullBranchOnlyLast}$4"
fi

MTool BuildServerGet "Source=$1" "DestinationDir=$DestinationDir"
CheckErrors

echo VeryImportant: Build now available at:
echo VeryImportant: $DestinationDir

exit 0
