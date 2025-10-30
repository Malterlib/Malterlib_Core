#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioTarget.sh Workspace Target Platform Architecture Configuration

set -eo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="${1:-Tests}"
Target="${2:-Build}"
Platform="${3:-$HostPlatform}"
Architecture="${4:-$HostArchitecture}"
Config="${5:-Debug}"
BuildSystemDir="${6:-BuildSystem/Default}"

ExtraParams=
if [[ "$MalterlibMSBuildBuildMaxParallelProjects" != "" ]]; then
	ExtraParams="-maxcpucount:$MalterlibMSBuildBuildMaxParallelProjects"
else
	ExtraParams="-m"
fi

echo CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln\"" /nodereuse:false $ExtraParams /v:m "\"/target:$Target\"" "\"/property:Platform=$Platform - $Architecture\"" "\"/property:Configuration=$Config\""
CallDirect msbuild.exe "\"${BuildSystemDir}/${Workspace}.sln\"" "\"/consoleLoggerParameters:Verbosity=normal;ForceConsoleColor;NoSummary;ForceNoAlign;DisableConsoleColor;NoItemAndPropertyList\"" /nologo /nodereuse:false $ExtraParams /v:m "\"/target:$Target\"" "\"/property:Platform=$Platform - $Architecture\"" "\"/property:Configuration=$Config\"" | MTool MSBuildFilter
