#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception


set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="${1:-Tests}"
TargetList="${2:-Build All}"

source "$DIR/ResolveConfig.sh"

Platform="${3:-$MalterlibDefaultPlatform}"
Architecture="${4:-$MalterlibDefaultArchitecture}"
Config="${5:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${6:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

IFS=',' read -r -a Targets <<< "$TargetList"

NinjaBuildDir="${BuildSystemDir}/${Workspace}/${Platform} ${Architecture} ${Config}"

NinjaCommandArgs="--quiet-success"

if [[ "$MalterlibBuildShowProgress" == "false" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs --quiet"
fi

if [[ "$NumCPUs" != "" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs -j $NumCPUs"
fi

echo ninja -C "$NinjaBuildDir" $NinjaCommandArgs "${Targets[@]}"

StartTimeMs=$(date +%s%N)
StartTimeMs=${StartTimeMs%??????}

source "$DIR/BuildLock.sh"
AcquireBuildLock "$NinjaBuildDir" || exit 1

set +e
# Record the foreground child before exec; a killed wrapper must leave the lock with its surviving Ninja.
(
	BuildLockRecordSelfAsChild || exit 1
	exec ninja -C "$NinjaBuildDir" $NinjaCommandArgs "${Targets[@]}"
)
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
