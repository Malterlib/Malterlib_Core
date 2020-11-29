#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeTarget.sh Workspace Target Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="$1"
Target="$2"
Platform="$3"
Architecture="$4"
Config="$5"

if [[ "$Workspace" == "" ]]; then
	Workspace="Tests"
fi

if [[ "$Target" == "" ]]; then
	Target="Build All"
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

$XCodeBuildTool -workspace "BuildSystem/Default/${Workspace}.xcworkspace" -scheme "$Target $Platform $Architecture $Config"
CheckErrors

exit 0

