#!/bin/bash
# Copyright © 2026 Unbroken AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildNinjaTarget.sh Workspace Target Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="${1:-Tests}"
Target="${2:-Build All}"

source "$DIR/ResolveConfig.sh"

Platform="${3:-$MalterlibDefaultPlatform}"
Architecture="${4:-$MalterlibDefaultArchitecture}"
Config="${5:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${6:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

NinjaBuildDir="${BuildSystemDir}/${Workspace}/${Platform} ${Architecture} ${Config}"

NinjaCommandArgs="--quiet-success"

if [[ "$MalterlibBuildShowProgress" == "false" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs --quiet"
fi

if [[ "$NumCPUs" != "" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs -j $NumCPUs"
fi

echo ninja -C "$NinjaBuildDir" $NinjaCommandArgs "$Target"

StartTimeMs=$(date +%s%N)
StartTimeMs=${StartTimeMs%??????}

set +e
ninja -C "$NinjaBuildDir" $NinjaCommandArgs "$Target"
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
