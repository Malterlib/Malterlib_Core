#!/bin/bash
# Copyright © Unbroken AB
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Usage: BuildVisualStudioTarget.sh Workspace Target[,Target...] Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="${1:-Tests}"
TargetList="${2:-Build}"

source "$DIR/ResolveConfig.sh"

Platform="${3:-$MalterlibDefaultPlatform}"
Architecture="${4:-$MalterlibDefaultArchitecture}"
Config="${5:-$MalterlibDefaultConfiguration}"
BuildSystemDir="${6:-${MalterlibGeneratedBuildSystemDir:-BuildSystem/Default}}"

# Several targets come as one comma separated argument, so the platform, architecture and
# configuration keep their places after it
IFS=',' read -r -a Targets <<< "$TargetList"

ExtraParams=
if [[ "$MalterlibMSBuildBuildMaxParallelProjects" != "" ]]; then
	ExtraParams="-maxcpucount:$MalterlibMSBuildBuildMaxParallelProjects"
else
	ExtraParams="-m"
fi

for Target in "${Targets[@]}"; do

echo CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln\"" /nodereuse:false $ExtraParams /v:m "\"/target:${Target//\./_}\"" "\"/property:Platform=$Platform - $Architecture\"" "\"/property:Configuration=$Config\""

CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln\"" \
	"\"/consoleLoggerParameters:Verbosity=normal;ForceConsoleColor;NoSummary;ForceNoAlign;DisableConsoleColor;NoItemAndPropertyList\"" \
	/nologo \
	/nodereuse:false \
	$ExtraParams \
	/v:m \
	"\"/target:Restore;${Target//\./_}\"" \
	"\"/property:Platform=$Platform - $Architecture\"" \
	"\"/property:Configuration=$Config\"" \
	2>&1 \
	| MTool MSBuildFilter

done
