#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildVisualStudioTarget.sh Workspace Target Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

Workspace="$1"
Target="$2"
Platform="$3"
Architecture="$4"

# Remove quotes around config if they exist.
Config="${5%\"}"
Config="${Config#\"}"

if [[ "$Workspace" == "" ]]; then
	Workspace="Tests"
fi

if [[ "$Target" == "" ]]; then
	Target="Build"
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

CallDirect msbuild.exe "\"BuildSystem/Default/${Workspace}.sln\"" /nodereuse:false /m /v:m "\"/target:$Target\"" "\"/property:Platform=$Platform - $Architecture\"" "\"/property:Configuration=$Config\""
CheckErrors

exit 0
