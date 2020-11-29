#!/bin/bash
# Copyright © 2015 Hansoft AB
# Distributed under the MIT license, see license text in LICENSE.Malterlib

# Usage: BuildXcodeWorkspace.sh Workspace Platform Architecture Configuration

set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

source "$DIR/DetectSystem.sh"

source ./BuildSystem/SharedBuildSettings.sh

export "PATH=/opt/homebrew/sbin:/opt/homebrew/bin:/usr/local/sbin:/usr/local/bin:$PATH"

Workspace="$1"
Platform="$2"
Architecture="$3"
Config="$4"

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

$XCodeBuildTool -workspace "BuildSystem/Default/$Workspace.xcworkspace" -scheme "Build All $Platform $Architecture $Config"
CheckErrors

exit 0
