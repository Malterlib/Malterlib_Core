#!/bin/bash
# Copyright © 2026 Unbroken AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildNinjaWorkspace.sh Workspace Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="${1:-Tests}"

source "$DIR/ResolveConfig.sh"

Platform="${2:-$MalterlibDefaultPlatform}"
Architecture="${3:-$MalterlibDefaultArchitecture}"
Config="${4:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${5:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

NinjaBuildDir="${BuildSystemDir}/${Workspace}/${Platform} ${Architecture} ${Config}"

NinjaCommandArgs="--quiet-success"
#NinjaCommandArgs="-v"

if [[ "$MalterlibBuildShowProgress" == "false" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs --quiet"
fi

if [[ "$NumCPUs" != "" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs -j $NumCPUs"
fi

echo ninja -C "$NinjaBuildDir" $NinjaCommandArgs "Build All"

StartTimeMs=$(date +%s%N)
StartTimeMs=${StartTimeMs%??????}

set +e
ninja -C "$NinjaBuildDir" $NinjaCommandArgs "Build All"
NinjaExitCode=$?
set -e

EndTimeMs=$(date +%s%N)
EndTimeMs=${EndTimeMs%??????}
DurationMs=$((EndTimeMs - StartTimeMs))
DurationSec=$((DurationMs / 1000))
DurationDeciSec=$(((DurationMs % 1000) / 100))

if [[ $NinjaExitCode -eq 0 ]]; then
	echo
	echo "Build finished successfully in $DurationSec.$DurationDeciSec seconds"
	echo
	exit 0
else
	echo
	echo "Build failed after $DurationSec.$DurationDeciSec seconds"
	echo
	exit $NinjaExitCode
fi
