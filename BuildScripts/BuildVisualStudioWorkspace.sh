#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioWorkspace.sh Workspace Platform Architecture Configuration

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

ExtraParams=
if [[ "$MalterlibMSBuildBuildMaxParallelProjects" != "" ]]; then
	ExtraParams="-maxcpucount:$MalterlibMSBuildBuildMaxParallelProjects"
else
	ExtraParams="-m"
fi

echo CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln"\" "/t:Restore;Build" /nodereuse:false $ExtraParams /v:m "\"/p:Platform=$Platform - $Architecture\"" "\"/p:Configuration=$Config\""
CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln"\" \
	"\"/consoleLoggerParameters:Verbosity=normal;ForceConsoleColor;NoSummary;ForceNoAlign;DisableConsoleColor;NoItemAndPropertyList\"" \
	/nologo \
	/nodereuse:false \
	$ExtraParams \
	/v:m \
	"/t:Restore;Build" \
	"\"/p:Platform=$Platform - $Architecture\"" \
	"\"/p:Configuration=$Config\"" \
	2>&1 \
	| MTool MSBuildFilter
