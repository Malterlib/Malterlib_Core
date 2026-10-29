#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception


set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

# The settings are those of the build system that the arguments name, when they do
if [ -n "${6:-}" ]; then
	export MalterlibBuildSystemDir="$6"
fi

source "$DIR/BuildSystemSettings.sh"
BuildSystemRefuseImportCaches

Workspace="${1:-Tests}"
TargetList="${2:-Build All}"

source "$DIR/ResolveConfig.sh"

Platform="${3:-$MalterlibDefaultPlatform}"
Architecture="${4:-$MalterlibDefaultArchitecture}"
Config="${5:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${6:-${MalterlibGeneratedBuildSystemDir:-$MalterlibBuildSystemDir}}"

# What ninja runs finds the settings of this build system
export MalterlibBuildSystemDir="$BuildSystemDir"

IFS=',' read -r -a Targets <<< "$TargetList"

NinjaBuildDir="${BuildSystemDir}/${Workspace}/${Platform} ${Architecture} ${Config}"

NinjaCommandArgs="--quiet-success"

if [[ "$MalterlibBuildShowProgress" == "false" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs --quiet"
fi

if [[ "$NumCPUs" != "" ]]; then
	NinjaCommandArgs="$NinjaCommandArgs -j $NumCPUs"
fi

# A build that another build runs, as of host tools, waits whatever these say: ninja tells from NINJA_BUILD_LOCK_CHAIN
if [[ "$MalterlibBuildLocking" == "true" ]]; then
	if [[ "$MalterlibBuildLockWait" == "true" ]]; then
		NinjaCommandArgs="$NinjaCommandArgs --wait-for-lock"
	fi

	if [[ -n "$MalterlibBuildLockTimeout" ]]; then
		NinjaCommandArgs="$NinjaCommandArgs --lock-timeout=$MalterlibBuildLockTimeout"
	fi
fi

echo ninja -C "$NinjaBuildDir" $NinjaCommandArgs "${Targets[@]}"

StartTimeMs=$(date +%s%N)
StartTimeMs=${StartTimeMs%??????}

set +e
ninja -C "$NinjaBuildDir" $NinjaCommandArgs "${Targets[@]}"
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
