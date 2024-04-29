#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioWorkspace.sh Workspace Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="$1"
Platform="$2"
Architecture="$3"

# Remove quotes around config if they exist.
Config="${4%\"}"
Config="${Config#\"}"

if [[ "$Workspace" == "" ]]; then
	Workspace="Tests"
fi

if [[ "$Platform" == "" ]]; then
	Platform="$HostPlatform"
fi

if [[ "$Architecture" == "" ]]; then
	Architecture="$HostArchitecture"
fi

if [[ "$Config" == "" ]]; then
	Config="Debug"
fi

ExtraParams=
if [[ "$MalterlibMSBuildBuildMaxParallelProjects" != "" ]]; then
	ExtraParams="-maxcpucount:$MalterlibMSBuildBuildMaxParallelProjects"
else
	ExtraParams="-m"
fi

echo CallDirect msbuild.exe "\"BuildSystem/Default/${Workspace}.sln"\" /nodereuse:false $ExtraParams /v:m "\"/p:Platform=$Platform - $Architecture\"" "\"/p:Configuration=$Config\""
CallDirect msbuild.exe "\"BuildSystem/Default/${Workspace}.sln"\" /nodereuse:false $ExtraParams /v:m "\"/p:Platform=$Platform - $Architecture\"" "\"/p:Configuration=$Config\""

exit 0
